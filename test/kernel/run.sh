#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/kernel/run.sh IMAGE_DIR
# Builds t_pagecache with Xcode's clang against the macOS SDK, runs it in a 1 GB container of
# IMAGE_DIR with kido (KIDO, default kido on PATH; KIDO_BACKEND and KIDO_QEMU as for kido's
# test/e2e.sh) and checks that the container still answers. t_pagecache writes 1.5 times the guest's
# memory to a file in 4 KiB calls and reads it back. Uses a throwaway KIDO_HOME.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
img=$1
kido=${KIDO:-kido}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-kernel.XXXX")
trap 'rm -rf "$work"' EXIT
export KIDO_HOME=$work/home
xcrun clang -arch arm64 -mmacosx-version-min=13.0 -O1 -o "$work/t_pagecache" "$here/t_pagecache.c" || exit 1
"$kido" image import "$img" kernel-test >/dev/null || exit 1
"$kido" run -d -m 1024 --name kernel-test kernel-test >/dev/null || exit 1
trap '"$kido" rm -f kernel-test >/dev/null 2>&1; rm -rf "$work"' EXIT
rc=0
"$kido" cp "$work/t_pagecache" kernel-test:/var/root/ && "$kido" exec kernel-test /var/root/t_pagecache || rc=1
if ! "$kido" exec kernel-test /bin/sh -c true; then
  echo "FAIL the container stopped answering: $("$kido" logs --console kernel-test 2>&1 | grep -a -m1 'panic(' | tr -d '\r')"
  rc=1
fi
exit $rc
