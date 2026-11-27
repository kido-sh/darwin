#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
if [[ $(uname -s) != Darwin ]]; then
  echo "SKIP restrictions: requires Darwin host"
  exit 0
fi
img=${1:-}
if [[ -z $img || ! -e $img ]]; then
  echo "SKIP restrictions: IMAGE_DIR not provided"
  exit 0
fi
kido=${KIDO:-kido}
if ! command -v "$kido" >/dev/null 2>&1 && [[ ! -x $kido ]]; then
  echo "SKIP restrictions: kido not found"
  exit 0
fi
if ! command -v xcrun >/dev/null 2>&1; then
  echo "SKIP restrictions: xcrun not found"
  exit 0
fi
limit=${TIMEOUT:-120}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-restrictions.XXXX")
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
xcrun clang -arch arm64 -mmacosx-version-min=15.0 -O1 -Wno-deprecated-declarations \
  -o "$work/t_restrictions" "$here/t_restrictions.c" -framework IOKit -framework CoreFoundation || exit 1
codesign -f -s - --entitlements "$here/entitlements.plist" "$work/t_restrictions" >/dev/null 2>&1 || exit 1
xcrun clang -arch arm64 -mmacosx-version-min=15.0 -O1 -dynamiclib \
  -o "$work/t_inject.dylib" "$here/t_inject.c" || exit 1
codesign -f -s - "$work/t_inject.dylib" >/dev/null 2>&1 || exit 1
xcrun clang -arch arm64 -mmacosx-version-min=15.0 -O1 \
  -Wl,-sectcreate,__RESTRICT,__restrict,/dev/null \
  -o "$work/t_restricted_env" "$here/t_restricted_env.c" || exit 1
codesign -f -s - "$work/t_restricted_env" >/dev/null 2>&1 || exit 1
xcrun clang -arch arm64 -mmacosx-version-min=15.0 -O1 \
  -o "$work/t_cs_restrict" "$here/t_restricted_env.c" || exit 1
codesign -f -s - -o restrict "$work/t_cs_restrict" >/dev/null 2>&1 || exit 1
xcrun clang -arch arm64 -mmacosx-version-min=15.0 -O1 \
  -o "$work/t_unrestricted_env" "$here/t_restricted_env.c" || exit 1
codesign -f -s - "$work/t_unrestricted_env" >/dev/null 2>&1 || exit 1

"$kido" image import "$img" restrictions-test >/dev/null || exit 1
"$kido" run -d --name restrictions-test restrictions-test >/dev/null || exit 1
trap '"$kido" rm -f restrictions-test >/dev/null 2>&1; rm -rf "$work"' EXIT

g=/tmp/kido-restrictions
timed "$kido" exec restrictions-test /bin/mkdir -p "$g" || exit 1
for f in t_restrictions t_inject.dylib t_restricted_env t_cs_restrict t_unrestricted_env; do
  "$kido" cp "$work/$f" "restrictions-test:$g/" || exit 1
done
timed "$kido" exec restrictions-test /bin/chmod 755 "$g" "$g/t_restrictions" \
  "$g/t_inject.dylib" "$g/t_restricted_env" "$g/t_cs_restrict" "$g/t_unrestricted_env" || exit 1

rc=0
timed "$kido" exec restrictions-test "$g/t_restrictions" unpriv || rc=1
timed "$kido" exec restrictions-test "$g/t_restrictions" root "$g/t_cs_restrict" || rc=1
timed "$kido" exec restrictions-test "$g/t_restrictions" files || rc=1

out_restricted=$(timed "$kido" exec restrictions-test /bin/sh -c \
  "DYLD_INSERT_LIBRARIES=$g/t_inject.dylib $g/t_restricted_env")
if [[ $out_restricted == "injected=0 env=0" ]]; then
  echo "ok   dyld: DYLD_INSERT_LIBRARIES scrubbed on __RESTRICT binary"
else
  echo "FAIL dyld: expected injected=0 env=0 on __RESTRICT binary, got: $out_restricted"
  rc=1
fi

out_unrestricted=$(timed "$kido" exec restrictions-test /bin/sh -c \
  "DYLD_INSERT_LIBRARIES=$g/t_inject.dylib $g/t_unrestricted_env")
if [[ $out_unrestricted == "injected=1 env=1" ]]; then
  echo "ok   dyld: DYLD_INSERT_LIBRARIES loaded on unrestricted binary"
else
  echo "FAIL dyld: expected injected=1 env=1 on unrestricted binary, got: $out_unrestricted"
  rc=1
fi

exit $rc
