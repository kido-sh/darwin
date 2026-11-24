#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/virtiofs/run.sh IMAGE_DIR
# Mounts directories of this Mac in a VZ container of IMAGE_DIR over virtio-fs and checks them against the
# Mac. t_virtiofs walks the tree on both sides and the listings must be identical: types, modes, sizes,
# times, link targets, hard links and contents read both with read and mmap. Then it checks the errors of a
# read-only mount, reads every file from many threads at once, sees changes made on the Mac, and checks that
# the host releases what it opened for the guest: all but a few files once the guest unmounts, and a file
# looked up 500 times once it is forgotten. It ends with read and metadata timings. Needs a kido whose VMM
# attaches the directories named in KIDO_DEV_VIRTIOFS. Each command in the container fails after TIMEOUT
# seconds (default 120). Uses a throwaway KIDO_HOME.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
img=$1
kido=${KIDO:-kido}
limit=${TIMEOUT:-120}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-virtiofs.XXXX")
name=virtiofs-test
export KIDO_HOME=$work/home KIDO_BACKEND=vz
trap '"$kido" rm -f $name >/dev/null 2>&1; chmod -R u+rwX "$work"; rm -rf "$work"' EXIT
rc=0

timed() {
  "$@" &
  local pid=$!
  (trap 'kill $s 2>/dev/null; exit 0' TERM; sleep "$limit" & s=$!; wait $s; kill "$pid") >/dev/null 2>&1 &
  local watchdog=$!
  wait "$pid"
  local status=$?
  kill "$watchdog" 2>/dev/null
  wait "$watchdog"
  [[ $status == 143 ]] && echo "FAIL timeout after ${limit}s: $*"
  return $status
}
guest() {
  timed "$kido" exec $name "$@"
}
check() {
  local what=$1
  shift
  if "$@"; then echo "ok   $what"; else echo "FAIL $what"; rc=1; fi
}
fails() {
  ! "$@" >/dev/null 2>&1
}
prints() {
  local want=$1
  shift
  [[ "$("$@" 2>&1)" == "$want" ]]
}
owned_by() {
  [[ $(guest /tmp/t_virtiofs as "$1" "$2" /mnt/data /mnt/data/private /mnt/data/private/f | grep -c " uid=$1 gid=$2 ") == 3 ]]
}
host_fds() {
  lsof -p "$vm" 2>/dev/null | awk '$5 == "REG" || $5 == "DIR"' | grep -c "$share" || true
}

xcrun clang -arch arm64 -mmacosx-version-min=15.0 -O2 -o "$work/t_virtiofs" "$here/t_virtiofs.c" || exit 1

share=$work/share
mkdir -p "$share"/{small,big,deep/1/2/3/4/5,many,names,links,private} "$work/empty" "$work/locked"
printf 'mine\n' > "$share/private/f"
chmod 700 "$share/private"
: > "$share/small/0"
for n in 1 4095 4096 4097 16383 16384 16385 131071 131072 131073 1048583; do
  head -c $n /dev/urandom > "$share/small/$n"
done
head -c $((64 << 20)) /dev/urandom > "$share/big/64m"
for i in $(seq 1 3000); do printf '%d\n' $i > "$share/many/f$i"; done
printf 'deep\n' > "$share/deep/1/2/3/4/5/leaf"
name255=$(printf 'n%.0s' $(seq 1 255))
printf 'long\n' > "$share/names/$name255"
printf 'utf8\n' > "$share/names/caf$(printf '\xc3\xa9') ünïcødé"
printf 'space\n' > "$share/names/with space"
printf 'colon\n' > "$share/names/a:b"
ln -s ../small/4097 "$share/links/relative"
ln -s /etc/hosts "$share/links/absolute"
ln -s nowhere "$share/links/dangling"
ln -s "$(printf 't%.0s' $(seq 1 1000))" "$share/links/long"
ln "$share/small/4096" "$share/links/hard"
mkfifo "$share/links/fifo"
cp "$work/t_virtiofs" "$share/exec"
printf 'secret\n' > "$work/locked/noaccess"
chmod 000 "$work/locked/noaccess"
chmod 750 "$share/deep"
touch -t 200102030405.06 "$share/small/1"

"$work/t_virtiofs" tree "$share" > "$work/tree.host" || { echo "FAIL the tree on this Mac"; rc=1; }

"$kido" image import "$img" $name >/dev/null || exit 1
KIDO_DEV_VIRTIOFS="data=$share,empty=$work/empty,locked=$work/locked" "$kido" run -d --name $name $name /bin/sleep 100000 \
  >/dev/null || exit 1
vm=$(pgrep -n -f com.apple.Virtualization.VirtualMachine)
"$kido" cp "$work/t_virtiofs" $name:/tmp/t_virtiofs || exit 1
before=$(host_fds)
guest /bin/mkdir -p /mnt || exit 1

check "a writable mount is refused" fails guest /sbin/mount -t virtiofs kido:data /mnt/rw
check "mount kido:data" guest /sbin/mount -t virtiofs -o ro kido:data /mnt/data
check "mount kido:empty" guest /sbin/mount -t virtiofs -o ro kido:empty /mnt/empty
check "mount the device root" guest /sbin/mount -t virtiofs -o ro kido /mnt/root
check "an unknown tag is refused" fails guest /sbin/mount -t virtiofs -o ro nope:data /mnt/x
check "an unknown share is refused" fails guest /sbin/mount -t virtiofs -o ro kido:nope /mnt/x
guest /sbin/mount | grep virtiofs

guest /tmp/t_virtiofs tree /mnt/data > "$work/tree.guest"
check "the guest sees the same tree as this Mac" diff -u "$work/tree.host" "$work/tree.guest"
check "the empty share is empty" prints "" guest /bin/ls -A /mnt/empty
check "the device root lists the shares" prints "$(printf 'data\nempty\nlocked')" guest /bin/ls /mnt/root
check "mount kido:locked" guest /sbin/mount -t virtiofs -o ro kido:locked /mnt/locked
check "a file the host cannot read cannot be opened" fails guest /bin/cat /mnt/locked/noaccess
check "unmount kido:locked" guest /sbin/umount /mnt/locked
check "errors of a read-only mount" guest /tmp/t_virtiofs errors /mnt/data kido:data
guest /tmp/t_virtiofs as 0 0 /mnt/data /mnt/data/private/f >/dev/null
check "another user is shown as the owner right after root looked" owned_by 501 20
check "root is shown as the owner right after another user looked" owned_by 0 0
check "a program runs from the share" guest /mnt/data/exec tree /mnt/data/deep
check "a FIFO on the share works as a pipe" prints hello guest /bin/sh -c 'echo hello >> /mnt/data/links/fifo & cat /mnt/data/links/fifo; wait'
check "concurrent reads" guest /tmp/t_virtiofs stress /mnt/data 16 2

mid=$(host_fds)
printf 'changed\n' >> "$share/small/16384"
rm "$share/many/f1"
mv "$share/deep/1/2/3/4" "$share/deep/1/2/3/four"
check "a file grown on this Mac" prints changed guest /bin/tail -c 8 /mnt/data/small/16384
check "a file removed on this Mac" fails guest /bin/test -e /mnt/data/many/f1
check "a directory renamed on this Mac" guest /bin/test -f /mnt/data/deep/1/2/3/four/5/leaf
printf 'X' | dd of="$share/small/4096" bs=1 seek=100 conv=notrunc 2>/dev/null
"$work/t_virtiofs" tree "$share/small" > "$work/small.host"
guest /tmp/t_virtiofs tree /mnt/data/small > "$work/small.guest"
check "a file rewritten in place on this Mac" diff -u "$work/small.host" "$work/small.guest"

check "unmount kido:data" guest /sbin/umount /mnt/data
check "unmount kido:empty" guest /sbin/umount /mnt/empty
check "unmount the device root" guest /sbin/umount /mnt/root
after=$(host_fds)
echo "files of the share the host holds open: $before before mounting, $mid after the walks, $after after unmounting"
check "the host closed all but a few of the files it opened for the guest" test "$after" -le $((before + 32))
forget_exact() {
  guest /sbin/mount -t virtiofs -o ro kido:data /mnt/data >/dev/null &&
    guest /tmp/t_virtiofs lookups 500 /mnt/data/small/4097 &&
    guest /sbin/umount /mnt/data &&
    guest /sbin/mount -t virtiofs -o ro kido:data /mnt/data >/dev/null &&
    guest /bin/sh -c '/tmp/t_virtiofs lookups 1 /mnt/data/many/f1??' &&
    guest /sbin/umount /mnt/data &&
    ! lsof -p "$vm" 2>/dev/null | grep -q "/small/4097$"
}
check "a file looked up 500 times is released once forgotten" forget_exact

guest /sbin/mount -t virtiofs -o ro kido:data /mnt/data >/dev/null
guest /tmp/t_virtiofs bench /mnt/data/big/64m
guest /tmp/t_virtiofs bench-meta /mnt/data/many 3
guest /sbin/umount /mnt/data
"$kido" cp "$share/big/64m" $name:/tmp/64m
guest /tmp/t_virtiofs bench /tmp/64m | sed 's/^read/read on the container disk/'
"$work/t_virtiofs" bench "$share/big/64m" | sed 's/^read/read on this Mac/'
"$work/t_virtiofs" bench-meta "$share/many" 3 | sed 's/^/on this Mac: /'

[[ $rc == 0 ]] && echo "virtiofs: all checks passed" || echo "virtiofs: FAILED"
exit $rc
