#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/kernel/run.sh IMAGE_DIR
# Builds t_pagecache and t_panic with Xcode's clang against the macOS SDK and runs each in a 1 GB
# container of IMAGE_DIR with kido (KIDO, default kido on PATH; KIDO_BACKEND and KIDO_QEMU as for
# kido's test/e2e.sh). t_pagecache writes 1.5 times the guest's memory to a file in 4 KiB calls and
# reads it back, and the container must still answer. t_panic panics the kernel through reboot(2)
# with RB_PANIC, and the container must stop within 60 s with the whole panic log, backtrace
# included, on its console; on VZ, which has no console yet, it only has to stop. Each command in a
# container fails after TIMEOUT seconds (default 120). Uses a throwaway KIDO_HOME and removes every
# container in it at exit.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
img=$1
kido=${KIDO:-kido}
limit=${TIMEOUT:-120}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-kernel.XXXX")
export KIDO_HOME=$work/home
cleanup() {
  "$kido" ps -aq 2>/dev/null | while read -r id; do "$kido" rm -f "$id" >/dev/null 2>&1; done
  rm -rf "$work"
}
trap cleanup EXIT
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
console() {
  "$kido" logs --console "$1" 2>/dev/null | tr -d '\r' | sed 's/\\x0d//g'
}
for t in t_pagecache t_panic; do
  xcrun clang -arch arm64 -mmacosx-version-min=13.0 -O1 -o "$work/$t" "$here/$t.c" || exit 1
done
"$kido" image import "$img" kernel-test >/dev/null || exit 1
rc=0
"$kido" run -d -m 1024 --name kernel-test kernel-test >/dev/null || exit 1
"$kido" cp "$work/t_pagecache" kernel-test:/var/root/ && timed "$kido" exec kernel-test /var/root/t_pagecache || rc=1
if ! timed "$kido" exec kernel-test /bin/sh -c true; then
  echo "FAIL the container stopped answering: $(console kernel-test | grep -a -m1 'panic(')"
  rc=1
fi
"$kido" rm -f kernel-test >/dev/null 2>&1

msg="kido kernel test panic $$"
"$kido" run -d -m 1024 --name kernel-panic kernel-test >/dev/null || exit 1
"$kido" cp "$work/t_panic" kernel-panic:/var/root/ || exit 1
timed "$kido" exec kernel-panic /var/root/t_panic "$msg" 2>/dev/null
state=running
for _ in $(seq 60); do
  state=$("$kido" inspect -f '{{.State}}' kernel-panic 2>&1)
  [[ $state == running ]] || break
  sleep 1
done
all=$(console kernel-panic)
missing=
[[ $state == running ]] && missing="; still running 60 s after the panic"
if [[ -z $all && ${KIDO_BACKEND:-vz} == vz ]]; then
  if [[ -n $missing ]]; then
    echo "FAIL panic${missing}"
    rc=1
  else
    echo "panic: container $state; VZ has no console, so the panic log is not checked"
  fi
  exit $rc
fi
log=$(sed -n "/userspace panic: $msg/,\$p" <<<"$all")
for want in "userspace panic: $msg" "Debugger message: panic" "Kernel version:" "Panicked task" \
  "Panicked thread:" "lr: 0x" "loaded kexts:"; do
  grep -q -F "$want" <<<"$log" || missing="$missing; no \"$want\""
done
grep -q "Nested panic" <<<"$log" && missing="$missing; a nested panic"
if [[ -n $missing ]]; then
  echo "FAIL panic console${missing}"
  rc=1
else
  echo "panic: the console has the panic log with $(grep -c 'lr: 0x' <<<"$log") backtrace frames, container $state"
fi
exit $rc
