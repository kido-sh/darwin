#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/security/run.sh IMAGE_DIR
# Builds the tests of Security's keys, PKCS#12 import, CMS decoding and SecureTransport with Xcode's
# clang against the macOS SDK and runs them in a container of IMAGE_DIR with kido (KIDO, default kido
# on PATH; KIDO_BACKEND and KIDO_QEMU as for kido's test/e2e.sh), and again on this Mac, macOS 15 or
# later, against macOS's own frameworks, on the same files and the same TLS servers in the container.
# Built for macOS 15, t_pkcs12 loads only where Security exports kSecImportToMemoryOnly, as macOS's does.
# Passes when the guest's checks pass, each side accepts the other's keys and signatures, and both
# sides print the same results. Three results differ by design and are shown apart: macOS refuses the
# AES-encrypted PKCS#12 file, and macOS's libcurl names a second TLS backend and cannot be limited to
# TLS 1.3, which curl 8.21 over LibreSSL can. vectors.txt holds known answers made by macOS's
# Security. PORT (default 18453) is the first of the four local ports the servers are published on.
# Uses a throwaway KIDO_HOME.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
img=$1
kido=${KIDO:-kido}
port=${PORT:-18453}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-security.XXXX")
trap 'rm -rf "$work"' EXIT
export KIDO_HOME=$work/home
exports=$here/../../nix/darwin/third-party/libressl
{
  printf -- "--- !tapi-tbd\ntbd-version: 4\ntargets: [ arm64-macos ]\ninstall-name: '/usr/lib/libcrypto.46.dylib'\n"
  printf 'current-version: 47.2\ncompatibility-version: 47\nexports:\n  - targets: [ arm64-macos ]\n'
  printf '    symbols: [ %s ]\n...\n' "$(grep -v '^#' "$exports/libcrypto.exports" | paste -sd, - | sed 's/,/, /g')"
} > "$work/libcrypto.46.tbd"
mkdir -p "$work/bin"
build() {
  xcrun clang -arch arm64 -mmacosx-version-min=15.0 -O1 -Wno-deprecated-declarations -o "$work/bin/$1" "$2" "${@:3}"
}
for t in t_seckey t_pkcs12 t_cms t_sectls; do
  build $t "$here/$t.c" -framework Security -framework CoreFoundation || exit 1
done
build t_secspi "$here/t_secspi.c" "$work/libcrypto.46.tbd" -framework Security || exit 1
build t_libcurl "$here/../ssl/t_libcurl.c" -lcurl || exit 1
cp "$here/security.sh" "$here/vectors.txt" "$work/bin/"
"$kido" image import "$img" security-test >/dev/null || exit 1
"$kido" run -d --name security-test -p 127.0.0.1:$port:8453 -p 127.0.0.1:$((port + 1)):8454 \
  -p 127.0.0.1:$((port + 2)):8455 -p 127.0.0.1:$((port + 3)):8456 security-test >/dev/null || exit 1
trap '"$kido" rm -f security-test >/dev/null 2>&1; rm -rf "$work"' EXIT
g=/var/root/security
"$kido" exec security-test /bin/mkdir -p $g || exit 1
for f in "$work"/bin/*; do
  "$kido" cp "$f" security-test:$g/ || exit 1
done
"$kido" exec security-test /bin/sh $g/security.sh setup $g/pki || exit 1
"$kido" exec -d security-test /bin/sh $g/security.sh serve $g/pki || exit 1
"$kido" exec security-test /bin/sh $g/security.sh ready || exit 1
rc=0
"$kido" exec security-test /bin/sh $g/security.sh check $g || rc=1
"$kido" cp security-test:$g/pki "$work/pki" || exit 1
"$work/bin/t_seckey" check "$work/pki/guest.vec" | sed 's/^/macOS on the guest'"'"'s keys: /' || rc=1
"$work/bin/t_seckey" gen > "$work/mac.vec" && "$kido" cp "$work/mac.vec" security-test:$g/mac.vec || exit 1
"$kido" exec security-test $g/t_seckey check $g/mac.vec | sed 's/^/the guest on macOS'"'"'s keys: /' || rc=1
"$kido" exec security-test /bin/sh $g/security.sh compare $g $g/pki 8453 8454 8455 8456 > "$work/guest.txt" || rc=1
sh "$here/security.sh" compare "$work/bin" "$work/pki" $port $((port + 1)) $((port + 2)) $((port + 3)) \
  > "$work/mac.txt" || rc=1
cat "$work/guest.txt"
known='^(AES p12|result curl backend|result curl TLS 1\.3 only)'
for side in mac guest; do
  grep -Ev "$known" "$work/$side.txt" > "$work/$side.same"
  grep -E "$known" "$work/$side.txt" > "$work/$side.known"
done
if diff "$work/mac.same" "$work/guest.same" > "$work/diff.txt"; then
  echo "the same as macOS: $(wc -l < "$work/guest.same" | tr -d ' ') results"
else
  echo "differs from macOS (< macOS, > guest):"
  cat "$work/diff.txt"
  rc=1
fi
echo "different by design (< macOS, > guest):"
diff "$work/mac.known" "$work/guest.known" | grep '^[<>]'
exit $rc
