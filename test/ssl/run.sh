#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/ssl/run.sh IMAGE_DIR
# Builds the tests of LibreSSL's libcrypto, libssl and libtls, for arm64e and arm64, and of libcurl
# over them, with Xcode's clang against the macOS SDK and linked under macOS's install names and
# versions. Runs them in a container of IMAGE_DIR with kido (KIDO, default kido on PATH; KIDO_BACKEND
# and KIDO_QEMU as for kido's test/e2e.sh), next to the guest's openssl and ocspcheck, against servers
# on the guest's loopback. Each command in the container fails after TIMEOUT seconds (default 120).
# Uses a throwaway KIDO_HOME.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
exports=$here/../../nix/darwin/third-party/libressl
img=$1
kido=${KIDO:-kido}
limit=${TIMEOUT:-120}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-ssl.XXXX")
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
stub() {
  {
    printf -- "--- !tapi-tbd\ntbd-version: 4\ntargets: [ arm64-macos, arm64e-macos ]\ninstall-name: '/usr/lib/%s.dylib'\n" "$1"
    printf 'current-version: %s\ncompatibility-version: %s\nexports:\n  - targets: [ arm64-macos, arm64e-macos ]\n' "$2" "$3"
    printf '    symbols: [ %s ]\n...\n' "$(grep -v '^#' "$exports/${1%.*}.exports" | paste -sd, - | sed 's/,/, /g')"
  } > "$work/$1.tbd"
}
stub libcrypto.46 47.2 47
stub libssl.48 49.2 49
stub libtls.20 21.3 21
build() {
  xcrun clang -arch "$1" -mmacosx-version-min=15.0 -O1 -o "$work/$2" "$here/${2%-arm64*}.c" "${@:3}"
}
for arch in arm64e arm64; do
  build $arch t_crypto-$arch "$work/libcrypto.46.tbd" || exit 1
  build $arch t_tls-$arch "$work/libssl.48.tbd" "$work/libtls.20.tbd" || exit 1
  build $arch t_rsakex-$arch "$work/libcrypto.46.tbd" "$work/libssl.48.tbd" || exit 1
done
build arm64 t_libcurl -lcurl || exit 1
"$kido" image import "$img" ssl-test >/dev/null || exit 1
"$kido" run -d --name ssl-test ssl-test >/dev/null || exit 1
trap '"$kido" rm -f ssl-test >/dev/null 2>&1; rm -rf "$work"' EXIT
timed "$kido" exec ssl-test /bin/mkdir -p /var/root/ssl || exit 1
for f in "$work"/t_crypto-* "$work"/t_tls-* "$work"/t_rsakex-* "$work/t_libcurl" "$here/ssl.sh"; do
  "$kido" cp "$f" ssl-test:/var/root/ssl/ || exit 1
done
timed "$kido" exec ssl-test /bin/sh /var/root/ssl/ssl.sh
