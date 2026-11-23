#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/libsystem/run.sh IMAGE_DIR
# Builds the tests with Xcode's clang against the macOS SDK, the ABI real macOS tools use, and runs
# them in a container of IMAGE_DIR with kido (KIDO, default kido on PATH; KIDO_BACKEND and KIDO_QEMU
# as for kido's test/e2e.sh). Uses a throwaway KIDO_HOME.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
img=$1
kido=${KIDO:-kido}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-libsystem.XXXX")
trap 'rm -rf "$work"' EXIT
export KIDO_HOME=$work/home
tests="t_math t_ucontext t_libc t_oslog t_keychain"
for t in $tests; do
  extra=
  [[ $t == t_keychain ]] && extra="-framework Security"
  xcrun clang -arch arm64 -mmacosx-version-min=13.0 -O1 -Wno-deprecated-declarations -Wno-format \
    -o "$work/$t" "$here/$t.c" $extra || exit 1
done
"$kido" image import "$img" libsystem-test >/dev/null || exit 1
"$kido" run -d --name libsystem-test libsystem-test >/dev/null || exit 1
trap '"$kido" rm -f libsystem-test >/dev/null 2>&1; rm -rf "$work"' EXIT
rc=0
for t in $tests; do
  "$kido" cp "$work/$t" libsystem-test:/var/root/ && "$kido" exec libsystem-test /var/root/$t || rc=1
done
"$kido" exec libsystem-test /var/root/t_oslog rotate || rc=1
sleep 2
if "$kido" exec libsystem-test /bin/sh -c \
  'test -f /var/log/system.log.1 && test $(wc -c < /var/log/system.log.1) -le 2097152 && test $(wc -c < /var/log/system.log) -le 2097152'; then
  echo "logd: system.log rotated, both files within 2 MiB"
else
  echo "FAIL logd: system.log was not rotated within 2 MiB"
  rc=1
fi
exit $rc
