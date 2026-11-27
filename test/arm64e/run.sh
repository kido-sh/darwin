#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/arm64e/run.sh IMAGE_DIR [LLVM_TESTS_DIR]
# Builds the tests for arm64e and arm64 with Xcode's clang and runs them in a container of IMAGE_DIR:
# pointer authentication keys, an atexit handler in a dlclosed dylib, libobjc and libc++, and the
# image's arm64e notifyd and logd. Then runs the Mac's own arm64e /bin/ls and /usr/bin/grep there:
# kido cp puts them in the container only, never in an image. LLVM_TESTS_DIR, a copy of what
# `nix build .#arm64e-tests` builds, adds t_pac.c built by the image's LLVM toolchain. Each command
# in the container fails after TIMEOUT seconds (default 120). KIDO, KIDO_BACKEND and KIDO_QEMU as for
# test/libsystem/run.sh.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
img=$1
llvm=${2:-}
kido=${KIDO:-kido}
limit=${TIMEOUT:-120}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-arm64e.XXXX")
trap 'rm -rf "$work"' EXIT
export KIDO_HOME=$work/home
timed() {
  "$@" &
  local pid=$!
  (trap 'kill $s 2>/dev/null; exit 0' TERM; sleep "$limit" & s=$!; wait $s; kill "$pid") >/dev/null 2>&1 &
  local watchdog=$!
  wait "$pid"
  local rc=$?
  kill "$watchdog" 2>/dev/null
  wait "$watchdog"
  [[ $rc == 143 ]] && echo "FAIL timeout after ${limit}s: $*"
  return $rc
}
flags=(-mmacosx-version-min=13.0 -O1)
for arch in arm64e arm64; do
  xcrun clang -arch $arch "${flags[@]}" -o "$work/t_pac-$arch" "$here/t_pac.c" || exit 1
  xcrun clang -arch $arch "${flags[@]}" -o "$work/t_hardening-$arch" "$here/t_hardening.c" || exit 1
  xcrun clang -arch $arch "${flags[@]}" -dynamiclib -o "$work/libt_atexit-$arch.dylib" "$here/t_atexit_lib.c" || exit 1
  xcrun clang -arch $arch "${flags[@]}" -o "$work/t_atexit-$arch" "$here/t_atexit.c" || exit 1
  xcrun clang++ -arch $arch "${flags[@]}" -std=c++17 -fno-exceptions -fno-objc-arc \
    -o "$work/t_objcxx-$arch" "$here/t_objcxx.mm" -lobjc || exit 1
done
xcrun clang -arch arm64 "${flags[@]}" -o "$work/t_daemons" "$here/t_daemons.c" || exit 1
"$kido" image import "$img" arm64e-test >/dev/null || exit 1
"$kido" run -d --name arm64e-test arm64e-test >/dev/null || exit 1
trap '"$kido" rm -f arm64e-test >/dev/null 2>&1; rm -rf "$work"' EXIT
rc=0
run() {
  timed "$kido" exec arm64e-test "$@" || rc=1
}
for f in "$work"/t_pac-* "$work"/t_hardening-* "$work"/libt_atexit-* "$work"/t_atexit-* "$work"/t_objcxx-* "$work/t_daemons"; do
  "$kido" cp "$f" arm64e-test:/var/root/ || rc=1
done
for arch in arm64e arm64; do
  run /var/root/t_pac-$arch
  run /var/root/t_atexit-$arch /var/root/libt_atexit-$arch.dylib
  run /var/root/t_objcxx-$arch
done
run /var/root/t_hardening-arm64e
run /var/root/t_hardening-arm64 /var/root/t_hardening-arm64e
run /var/root/t_daemons
if [[ -n $llvm ]]; then
  timed "$kido" exec arm64e-test /bin/mkdir -p /var/root/llvm || rc=1
  for arch in arm64e arm64; do
    "$kido" cp "$llvm/t_pac-$arch" arm64e-test:/var/root/llvm/ || rc=1
    run /var/root/llvm/t_pac-$arch
  done
fi
timed "$kido" exec arm64e-test /bin/mkdir -p /var/root/mac || rc=1
for tool in /bin/ls /usr/bin/grep; do
  "$kido" cp "$tool" arm64e-test:/var/root/mac/ || rc=1
done
if out=$(timed "$kido" exec arm64e-test /bin/sh -c \
  '/var/root/mac/ls -l /usr/lib/libSystem.B.dylib && printf "root\nuser\n" | /var/root/mac/grep -c root') &&
  [[ $out == *libSystem.B.dylib*1 ]]; then
  echo "macOS ls and grep: ran"
else
  echo "FAIL macOS ls and grep: $out"
  rc=1
fi
exit $rc
