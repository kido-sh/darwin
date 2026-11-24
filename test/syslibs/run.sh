#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/syslibs/run.sh IMAGE_DIR
# Builds the tests of libcurl, ncurses with panel, menu and form, libedit, libbz2, liblzma and libutil
# with Xcode's clang against the macOS SDK, the ABI real macOS tools use, and runs them in a container
# of IMAGE_DIR with kido (KIDO, default kido on PATH; KIDO_BACKEND and KIDO_QEMU as for kido's
# test/e2e.sh). Uses a throwaway KIDO_HOME. The curl test needs the network: it fetches
# https://example.com.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
img=$1
kido=${KIDO:-kido}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-syslibs.XXXX")
trap 'rm -rf "$work"' EXIT
export KIDO_HOME=$work/home
build() {
  xcrun clang -arch arm64 -mmacosx-version-min=15.0 -O1 -Wno-deprecated-declarations \
    -o "$work/$1" "$here/$1.c" "${@:2}"
}
build t_curl -lcurl || exit 1
build t_curses -lncurses -lpanel -lmenu -lform || exit 1
build t_edit -ledit || exit 1
build t_bz2 -lbz2 || exit 1
build t_lzma -llzma || exit 1
build t_util -lutil || exit 1
"$kido" image import "$img" syslibs-test >/dev/null || exit 1
"$kido" run -d --name syslibs-test syslibs-test >/dev/null || exit 1
trap '"$kido" rm -f syslibs-test >/dev/null 2>&1; rm -rf "$work"' EXIT
rc=0
for t in t_curl t_curses t_edit t_bz2 t_lzma t_util; do
  "$kido" cp "$work/$t" syslibs-test:/var/root/ && "$kido" exec syslibs-test /var/root/$t || rc=1
done
exit $rc
