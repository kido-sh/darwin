#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/virtiofs/run.sh IMAGE_DIR
# Mounts directories of this Mac in a VZ container of IMAGE_DIR over virtio-fs and checks them against the
# Mac. t_virtiofs walks the tree on both sides and the listings must be identical: types, modes, sizes,
# times, link targets, hard links and contents read both with read and mmap. Then it checks the errors of a
# read-only mount, reads every file from many threads at once, sees changes made on the Mac once the host's
# one-second entry timeout has passed, and checks that the host releases what it opened for the guest: all
# but a few files once the guest unmounts, and a file looked up 500 times once it is forgotten. On a
# writable share the guest builds a tree that must come out on the Mac as the Mac itself builds it, its
# errors and extended attributes must match the Mac's, and the Mac's permissions and read-only shares hold
# for every change. The writable share is mounted a second time, so that checks can read and change what the
# Mac holds without the first mount's cache, and a 16 MiB disk image serves as a volume that runs full on
# demand. It ends with read, write and metadata timings. Needs a kido whose VMM attaches the directories
# named in KIDO_DEV_VIRTIOFS.
# Each command in the container fails after TIMEOUT seconds (default 120). Uses a throwaway KIDO_HOME.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
img=$1
kido=${KIDO:-kido}
limit=${TIMEOUT:-120}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-virtiofs.XXXX") && work=$(cd "$work" && pwd -P) || exit 1
name=virtiofs-test
export KIDO_HOME=$work/home KIDO_BACKEND=vz
trap '"$kido" rm -f $name >/dev/null 2>&1; hdiutil detach -quiet -force "$work/full" 2>/dev/null; chmod -R u+rwX "$work"; rm -rf "$work"' EXIT
rc=0

exec 3>&1
timed() {
  "$@" &
  local pid=$! mark
  mark=$(mktemp -u "$work/timeout.XXXXXX")
  (s=; trap 'kill $s 2>/dev/null; exit 0' TERM; sleep "$limit" & s=$!; wait $s; : > "$mark"; kill "$pid"
   sleep 5 & s=$!; wait $s; kill -9 "$pid") >/dev/null 2>&1 3>&- &
  local watchdog=$!
  wait "$pid"
  local status=$?
  kill "$watchdog" 2>/dev/null
  wait "$watchdog"
  if [[ -e $mark ]]; then
    echo "FAIL timeout after ${limit}s: $*" | tee -a "$work/timeouts" >&3
    rm -f "$mark"
    status=124
  fi
  return $status
}
timeouts() {
  if [[ -f $work/timeouts ]]; then wc -l < "$work/timeouts" | tr -d ' '; else echo 0; fi
}
guest() {
  timed "$kido" exec $name "$@"
}
check() {
  local what=$1 before
  shift
  before=$(timeouts)
  if "$@" && [[ $(timeouts) == "$before" ]]; then echo "ok   $what"; else echo "FAIL $what"; rc=1; fi
}
fails() {
  ! "$@" >/dev/null 2>&1
}
prints() {
  local want=$1
  shift
  [[ "$("$@" 2>&1)" == "$want" ]]
}
owner_after() {
  local paths="/mnt/data /mnt/data/private /mnt/data/private/f"
  [[ $(guest /bin/sh -c "/tmp/t_virtiofs as $1 $2 $paths >/dev/null; /tmp/t_virtiofs as $3 $4 $paths" |
    grep -c " uid=$3 gid=$4 ") == 3 ]]
}
host_open() {
  lsof -p "$vm" 2>/dev/null | awk '$5 == "REG" || $5 == "DIR"' | grep -c "$1" || true
}
host_writing() {
  lsof -p "$vm" 2>/dev/null | awk '$5 == "REG" && $4 ~ /[wu]$/' | grep -c "$1" || true
}
host_fds() {
  host_open "$share${1:-}"
}
revoked() {
  local pause=$1 file=$2 mode=$3 was
  shift 3
  was=$(stat -f %Lp "$share/$file") && [[ $(host_fds "/$file\$") == 2 ]] || return 1
  chmod "$mode" "$share/$file"
  sleep "$pause"
  fails guest "$@"
  local denied=$?
  sleep 0.5
  local held=$(host_fds "/$file\$")
  chmod "$was" "$share/$file"
  sleep "$pause"
  echo "$file at mode $mode: $( ((denied == 0)) && echo refused || echo allowed ) after ${pause}s, then $held host files open on it"
  [[ $denied == 0 && $held == 1 ]] && guest "$@" >/dev/null
}
held_open() {
  local was holder status denied
  was=$(stat -f %Lp "$share/revoke") || return 1
  guest /tmp/t_virtiofs hold /mnt/data/revoke 3 > "$work/hold.out" 2>&1 &
  holder=$!
  sleep 1
  chmod 000 "$share/revoke"
  fails guest /bin/cat /mnt/data/revoke
  denied=$?
  wait $holder
  status=$?
  chmod "$was" "$share/revoke"
  echo "a descriptor open across a refused open read: $(cat "$work/hold.out")"
  [[ $denied == 0 && $status == 0 ]]
}
ro_share_refuses() {
  local out
  out=$(guest /bin/sh -c 'mkdir -p /mnt/rwro && /sbin/mount -t virtiofs kido:data /mnt/rwro &&
    /tmp/t_virtiofs ro-share /mnt/rwro; /sbin/umount /mnt/rwro') || return 1
  echo "a writable mount of a read-only share: $(sed 's/^[^:]*: //' <<<"$out" | sort | uniq -c | tr -s ' ' | tr '\n' ',')"
  [[ $(grep -cE ': (Operation not permitted|Permission denied|Read-only file system)$' <<<"$out") -eq 12 ]]
}
nostamp() {
  sed -E 's/ ctime=[0-9.]+//'
}
setuid_dropped() {
  guest /bin/sh -c 'chmod 4755 /mnt/rw/w/exec && chmod 2755 /mnt/rw/w/d' || return 1
  local file dir
  file=$(stat -f %Lp "$rw/w/exec") dir=$(stat -f %Lp "$rw/w/d")
  echo "chmod 4755 and 2755 in the guest leave $file and $dir on this Mac"
  [[ $file == 755 && $dir == 755 ]]
}
write_revoked() {
  local hold=$1 holder denied cut status
  guest /bin/sh -c 'printf "a\n" > /mnt/rw/wr && cat /mnt/rw/wr > /dev/null' || return 1
  guest /tmp/t_virtiofs "$hold" /mnt/rw/wr 3 > "$work/held.out" 2>&1 &
  holder=$!
  sleep 1
  [[ $hold == hold ]] && guest /bin/sh -c ': >> /mnt/rw/wr'
  local writers
  writers=$(host_writing "$rw/wr")
  chmod 444 "$rw/wr"
  fails guest /bin/sh -c 'exec 3>> /mnt/rw/wr'
  denied=$?
  cut=1
  [[ $hold == hold ]] && fails guest /tmp/t_virtiofs truncate /mnt/rw/wr 0 && cut=0
  wait $holder
  status=$?
  chmod 644 "$rw/wr"
  local truncated=
  [[ $hold == hold ]] && truncated=" truncating by name was $( ((cut == 0)) && echo refused || echo allowed ),"
  echo "with a descriptor from $hold: opening for writing was $( ((denied == 0)) && echo refused || echo allowed ),$truncated" \
    "the Mac had $writers files open for writing, the descriptor got $(cat "$work/held.out")"
  [[ $denied == 0 && $status == 0 && $writers -ge 1 ]] || return 1
  [[ $hold == hold-write ]] && [[ $(cat "$rw/wr") == $'a\nheld' ]] && return 0
  [[ $hold == hold && $cut == 0 && $(cat "$rw/wr") == a ]]
}
appended_kept() {
  printf '%04000d' 0 > "$rw/app"
  head -c 6000 /dev/urandom > "$work/appended"
  guest /tmp/t_virtiofs map-hold /mnt/rw/app 3 > "$work/map-hold.out" 2>&1 &
  local holder=$! status first kept=0
  sleep 1
  cat "$work/appended" >> "$rw/app"
  wait $holder
  status=$?
  first=$(head -c 1 "$rw/app")
  cmp -s <(tail -c 6000 "$rw/app") "$work/appended" && kept=1
  echo "a page stored to before this Mac appended: $(cat "$work/map-hold.out"), first byte $first," \
    "appended bytes $( ((kept)) && echo kept || echo overwritten)"
  [[ $status == 0 && $first == A && $kept == 1 ]]
}
times_now() {
  local mtime now
  mtime=$(stat -f %m "$rw/edge/times") now=$(date +%s)
  echo "utimes(NULL) in the guest: mtime $((now - mtime)) s before now on this Mac"
  [[ $((now - mtime)) -lt 120 && $((mtime - now)) -lt 120 ]]
}
evtonly_kept() {
  local was holder status
  was=$(stat -f %Lp "$share/revoke") || return 1
  guest /tmp/t_virtiofs hold-mode /mnt/data/revoke evtonly 3 > "$work/evt.out" 2>&1 &
  holder=$!
  sleep 1
  chmod 000 "$share/revoke"
  fails guest /bin/cat /mnt/data/revoke
  local denied=$?
  wait $holder
  status=$?
  chmod "$was" "$share/revoke"
  echo "an O_EVTONLY descriptor open across a refused open read: $(cat "$work/evt.out")"
  [[ $denied == 0 && $status == 0 && $(cat "$work/evt.out") == "7 bytes" ]]
}
exec_beside_writer() {
  local holder denied
  guest /bin/sh -c 'cp /tmp/t_virtiofs /mnt/rw/prog && chmod 755 /mnt/rw/prog && /mnt/rw/prog lookups 1 /' || return 1
  chmod 311 "$rw/prog"
  guest /tmp/t_virtiofs hold-mode /mnt/rw/prog wronly 3 > /dev/null 2>&1 &
  holder=$!
  sleep 1
  local writers status
  writers=$(host_writing "$rw/prog\$")
  fails guest /mnt/rw/prog lookups 1 /
  denied=$?
  wait $holder
  status=$?
  chmod 755 "$rw/prog"
  echo "a program this Mac made unreadable, run while another descriptor writes it ($writers open for writing):" \
    "$( ((denied == 0)) && echo refused || echo ran)"
  [[ $denied == 0 && $status == 0 && $writers -ge 1 ]]
}
trunc_reader() {
  local holder writers
  printf 'abc\n' > "$rw/ot"
  guest /tmp/t_virtiofs hold-mode /mnt/rw/ot rdonly-trunc 3 > /dev/null 2>&1 &
  holder=$!
  sleep 1.5
  local opened created status
  writers=$(host_writing "$rw/ot\$") opened=$(host_open "$rw/ot\$")
  wait $holder
  status=$?
  guest /tmp/t_virtiofs hold-mode /mnt/rw/oc rdonly-creat 3 > /dev/null 2>&1 &
  holder=$!
  sleep 1.5
  local held
  created=$(host_writing "$rw/oc\$") held=$(host_open "$rw/oc\$")
  wait $holder
  status=$((status | $?))
  echo "while a descriptor lives, the Mac holds $writers files open for writing after O_RDONLY | O_TRUNC" \
    "($opened open at all) and $created after O_RDONLY | O_CREAT"
  [[ $status == 0 && $writers == 0 && $opened -ge 1 && $created == 0 && $held -ge 1 && ! -s "$rw/ot" ]]
}
synced_map() {
  local holder seen
  printf 'xxxxxxxxxx' > "$rw/ms"
  guest /tmp/t_virtiofs map-sync /mnt/rw/ms 3 > /dev/null 2>&1 &
  holder=$!
  sleep 1.5
  seen=$(head -c 6 "$rw/ms")
  wait $holder
  echo "sync(2) with a store in a live mapping: this Mac reads $seen"
  [[ $seen == synced ]]
}
closed_released() {
  local writers
  guest /tmp/t_virtiofs close-stat /mnt/rw/closing 3 || return 1
  sleep 0.5
  writers=$(host_writing "$rw/closing/")
  echo "after that, the Mac holds $writers of those files open for writing"
  [[ $writers == 0 ]]
}
removed_released() {
  local held
  held=$(host_open "$rw/gone\$")
  echo "a file the guest removed before thousands of others is open $held times on this Mac"
  [[ $held == 0 ]]
}
inode_open() {
  lsof -p "$vm" 2>/dev/null |
    awk -v dev="$((${1} >> 24)),$((${1} & 0xffffff))" -v ino="$2" '$5 == "REG" && $6 == dev && $8 == ino' | wc -l
}
links_released() {
  local dev ino before held
  guest /tmp/t_virtiofs link-pair /mnt/rw/edge make > /dev/null || return 1
  dev=$(stat -f %d "$rw/edge/linked") ino=$(stat -f %i "$rw/edge/linked") || return 1
  before=$(inode_open "$dev" "$ino")
  guest /tmp/t_virtiofs link-pair /mnt/rw/edge remove > /dev/null || return 1
  sleep 1
  held=$(inode_open "$dev" "$ino")
  echo "a file with two links is open $((before)) times on this Mac, and $((held)) times once the guest removed both"
  [[ $before -ge 1 && $held -eq 0 ]]
}
raw_create_released() {
  local writers
  guest /tmp/t_virtiofs raw-create /mnt/rw/raw || return 1
  sleep 0.5
  writers=$(host_writing "$rw/raw\$")
  echo "a file whose creating open failed before it opened is open $writers times for writing on this Mac"
  [[ -e "$rw/raw" && $writers == 0 ]]
}
exec_read_released() {
  local was held status
  was=$(guest /bin/sh -c 'sysctl -n vfs.generic.virtiofs.idle_files') || return 1
  guest /bin/sh -c 'sysctl -w vfs.generic.virtiofs.idle_files=0 >/dev/null && /tmp/t_virtiofs exec-fail /mnt/data/notprog'
  status=$?
  sleep 1
  held=$(host_fds '/notprog$')
  guest /bin/sh -c "sysctl -w vfs.generic.virtiofs.idle_files=$was" >/dev/null
  echo "a file an exec read and refused, with no idle handles kept: the host holds it open $held times"
  [[ $status == 0 && $held -le 1 ]]
}
idle_none() {
  local was held status
  was=$(guest /bin/sh -c 'sysctl -n vfs.generic.virtiofs.idle_files') || return 1
  head -c $((32 << 20)) /dev/urandom > "$share/big/cold"
  guest /bin/sh -c 'sysctl -w vfs.generic.virtiofs.idle_files=0 >/dev/null && /tmp/t_virtiofs advise-close /mnt/data/big/cold 32'
  status=$?
  sleep 1
  held=$(host_fds '/big/cold$')
  guest /bin/sh -c "sysctl -w vfs.generic.virtiofs.idle_files=$was" >/dev/null
  echo "a file never read before, closed while 32 MiB of read advice ran, with no idle handles kept: the host holds it" \
    "open $held times"
  [[ $status == 0 && $held -le 1 ]]
}
readers_beside_writer() {
  local was how holder opened writing status=0
  was=$(guest /bin/sh -c 'sysctl -n vfs.generic.virtiofs.idle_files') || return 1
  guest /bin/sh -c 'sysctl -w vfs.generic.virtiofs.idle_files=0' > /dev/null
  local reading
  printf 'readers\n' > "$rw/rb-advise"
  guest /tmp/t_virtiofs hold-readers /mnt/rw/rb-advise advise 3 > /dev/null 2>&1 &
  holder=$!
  sleep 1.5
  opened=$(host_open "$rw/rb-advise\$") writing=$(host_writing "$rw/rb-advise\$")
  wait $holder || status=1
  echo "read advice beside a writer: the Mac holds the file open $opened times, $writing of them for writing"
  [[ $opened == 2 && $writing == 1 ]] || status=1
  printf 'readers\n' > "$rw/rb-close"
  guest /tmp/t_virtiofs hold-readers /mnt/rw/rb-close read-close 3 > /dev/null 2>&1 &
  holder=$!
  sleep 1.2
  reading=$(host_open "$rw/rb-close\$")
  sleep 2.3
  opened=$(host_open "$rw/rb-close\$") writing=$(host_writing "$rw/rb-close\$")
  wait $holder || status=1
  echo "a reader beside a writer: the Mac holds the file open $reading times while it reads and $opened after it" \
    "closes, $writing of them for writing"
  [[ $reading == 3 && $opened == 2 && $writing == 1 ]] || status=1
  guest /bin/sh -c "sysctl -w vfs.generic.virtiofs.idle_files=$was" >/dev/null
  return $status
}
unmapped_released() {
  local holder writers status
  printf '%016384d' 0 > "$rw/mr"
  guest /tmp/t_virtiofs map-reader /mnt/rw/mr 3 > "$work/map-reader.out" 2>&1 &
  holder=$!
  sleep 1.5
  writers=$(host_writing "$rw/mr\$")
  wait $holder
  status=$?
  echo "after munmap, with a reader still open: the Mac holds $writers files open for writing, first byte" \
    "$(head -c 1 "$rw/mr")"
  [[ $status == 0 && $writers == 0 && $(head -c 1 "$rw/mr") == W ]]
}
mapped_writer_kept() {
  local holder status
  printf '%016384d' 0 > "$rw/ms2"
  guest /tmp/t_virtiofs map-store /mnt/rw/ms2 3 > "$work/map-store.out" 2>&1 &
  holder=$!
  sleep 1
  guest /tmp/t_virtiofs truncate /mnt/rw/ms2 16384 > /dev/null
  chmod 444 "$rw/ms2"
  wait $holder
  status=$?
  chmod 644 "$rw/ms2"
  echo "a mapping whose descriptor closed, after a truncation by name and a chmod 444 on this Mac:" \
    "$(cat "$work/map-store.out"), first byte $(head -c 1 "$rw/ms2")"
  [[ $status == 0 && $(head -c 1 "$rw/ms2") == M ]]
}
fill() {
  local pass
  sync
  for pass in 1 2 3; do
    dd if=/dev/zero of="$full/ballast$pass" bs=65536 2>/dev/null
    dd if=/dev/zero of="$full/ballast$pass-last" bs=512 2>/dev/null
    sync
    sleep 0.2
  done
}
room() {
  rm -f "$full"/ballast*
  sync
}
refused_kept() {
  local holder status byte
  dd if=/dev/zero of="$full/stored" bs=1 count=0 seek=$((129 << 14)) 2>/dev/null
  guest /tmp/t_virtiofs map-fail /mnt/full/stored 3 > "$work/map-fail.out" 2>&1 &
  holder=$!
  sleep 1
  fill
  sleep 2.5
  room
  wait $holder
  status=$?
  byte=$(for page in 0 64 127; do dd if="$full/stored" bs=1 skip=$((16384 + 5000 + (page << 14))) count=1 2>/dev/null; done)
  echo "on a volume of this Mac that ran full: $(cat "$work/map-fail.out"), and this Mac reads $byte for those three"
  [[ $status == 0 && $byte == $(sed -n 's/.*the last read \(...\),.*/\1/p' "$work/map-fail.out") ]]
}
unpushed_kept() {
  local holder status byte mode
  dd if=/dev/zero of="$full/unpushed" bs=1 count=0 seek=$((128 << 14)) 2>/dev/null
  guest /tmp/t_virtiofs map-unpushed /mnt/full/unpushed 3 > "$work/unpushed.out" 2>&1 &
  holder=$!
  sleep 1
  fill
  sleep 2.5
  chmod 444 "$full/unpushed"
  room
  wait $holder
  status=$?
  sleep 0.5
  byte=$(for page in 0 63 127; do dd if="$full/unpushed" bs=1 skip=$((100 + (page << 14))) count=1 2>/dev/null; done)
  mode=$(stat -f %Lp "$full/unpushed")
  echo "stores whose pushes failed until their mapping was gone: $(cat "$work/unpushed.out"), then this Mac made the" \
    "file mode $mode and room, and after sync(2) it reads $byte in the first, a middle and the last page"
  [[ $status == 0 && $byte == kkk && $mode == 444 ]]
}
short_kept() {
  local holder status wrong lead first
  dd if=/dev/zero of="$full/short" bs=1 count=0 seek=$((2 << 20)) 2>/dev/null
  head -c $((1 << 20)) /dev/zero | tr '\0' p > "$work/written"
  head -c $((640 << 10)) /dev/urandom > "$full/slack"
  fill
  rm -f "$full/slack"
  sync
  sleep 0.5
  guest /tmp/t_virtiofs short-write /mnt/full/short 2 > "$work/short.out" 2>&1 &
  holder=$!
  sleep 1.2
  lead=$(cmp "$work/written" "$full/short" 2>/dev/null | sed -n 's/.*char \([0-9]*\),.*/\1/p')
  lead=$((${lead:-1048577} - 1))
  room
  wait $holder
  status=$?
  first=$(sed -n 's/.*returned \(-*[0-9]*\) .*/\1/p' "$work/short.out")
  wrong=$(cmp -l "$work/written" "$full/short" 2>/dev/null | wc -l | tr -d ' ')
  echo "on a volume of this Mac with 640 KiB free: $(cat "$work/short.out"), this Mac held $lead bytes of it before" \
    "there was room and $wrong bytes differ after"
  [[ $status == 0 && $wrong == 0 && $lead -ge ${first:-0} && $lead -lt $((${first:-0} + 131072)) ]]
}
idle_kept() {
  local was held
  was=$(guest /bin/sh -c 'sysctl -n vfs.generic.virtiofs.idle_files') || return 1
  guest /bin/sh -c 'sysctl -w vfs.generic.virtiofs.idle_files=4 >/dev/null && cat /mnt/data/many/f1?? >/dev/null'
  local status=$?
  sleep 1
  held=$(host_fds '/many/f1[0-9][0-9]$')
  guest /bin/sh -c "sysctl -w vfs.generic.virtiofs.idle_files=$was" >/dev/null
  echo "100 files read with 4 idle handles kept: the host holds $held files of them open"
  [[ $status == 0 && $held -gt 100 && $held -le 104 ]]
}

xcrun clang -arch arm64 -mmacosx-version-min=15.0 -O2 -o "$work/t_virtiofs" "$here/t_virtiofs.c" || exit 1

share=$work/share
wide=$(printf '\xe4\xb8\xad%.0s' $(seq 1 85))
rw=$work/rw
full=$work/full
mkdir -p "$share"/{small,big,deep/1/2/3/4/5,many,names,long,links,private} "$work/empty" "$work/locked" "$work/wide" \
  "$rw" "$full"
hdiutil create -quiet -size 16m -fs APFS -volname kido-full "$work/full.dmg" &&
  hdiutil attach -quiet -nobrowse -mountpoint "$full" "$work/full.dmg" || exit 1
printf 'wide share\n' > "$work/wide/f"
printf 'mine\n' > "$share/private/f"
printf 'revoke\n' > "$share/revoke"
printf 'not a program\n' > "$share/notprog"
chmod 755 "$share/notprog"
chmod 700 "$share/private"
: > "$share/small/0"
for n in 1 4095 4096 4097 16383 16384 16385 131071 131072 131073 1048583; do
  head -c $n /dev/urandom > "$share/small/$n"
done
head -c $((64 << 20)) /dev/urandom > "$share/big/64m"
head -c $((32 << 20)) /dev/urandom > "$share/big/32m"
head -c $((16 << 20)) /dev/urandom > "$share/big/16m"
head -c $((8 << 20)) /dev/urandom > "$share/big/8m"
for i in $(seq 1 3000); do printf '%d\n' $i > "$share/many/f$i"; done
printf 'deep\n' > "$share/deep/1/2/3/4/5/leaf"
name255=$(printf 'n%.0s' $(seq 1 255))
printf 'long\n' > "$share/names/$name255"
printf 'utf8\n' > "$share/names/caf$(printf '\xc3\xa9') ünïcødé"
printf 'space\n' > "$share/names/with space"
printf 'colon\n' > "$share/names/a:b"
printf 'wide\n' > "$share/names/$(printf '\xe4\xb8\xad%.0s' $(seq 1 100))"
printf 'longest\n' > "$share/long/$(printf '\xe4\xb8\xad%.0s' $(seq 1 255))"
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
KIDO_DEV_VIRTIOFS="data=$share:ro,empty=$work/empty:ro,locked=$work/locked:ro,rw=$rw,full=$full,$wide=$work/wide:ro" \
  "$kido" run -d --name $name $name /bin/sleep 100000 >/dev/null 3>&- || exit 1
vm=$(pgrep -n -f com.apple.Virtualization.VirtualMachine)
"$kido" cp "$work/t_virtiofs" $name:/tmp/t_virtiofs || exit 1
before=$(host_fds)
guest /bin/mkdir -p /mnt || exit 1

check "a writable mount of a share the host made read-only refuses every change" ro_share_refuses
check "mount kido:data" guest /sbin/mount -t virtiofs -o ro kido:data /mnt/data
check "mount kido:empty" guest /sbin/mount -t virtiofs -o ro kido:empty /mnt/empty
check "mount the device root" guest /sbin/mount -t virtiofs -o ro kido /mnt/root
check "an unknown tag is refused" fails guest /sbin/mount -t virtiofs -o ro nope:data /mnt/x
check "an unknown share is refused" fails guest /sbin/mount -t virtiofs -o ro kido:nope /mnt/x
guest /bin/sh -c 'mkdir -p /tmp/u501 && chown 501:20 /tmp/u501'
check "a user other than root cannot mount a share" prints "mount: Operation not permitted" \
  guest /tmp/t_virtiofs mount-as 501 20 kido:data /tmp/u501
guest /sbin/mount | grep virtiofs

check "files closed while their reads are in flight" guest /tmp/t_virtiofs read-close /mnt/data/big/64m 128
check "a close is not held up by reads that start after it" guest /tmp/t_virtiofs reopen-close /mnt/data/big/32m 40
check "fcntl F_RDADVISE brings a file into memory without a read" guest /tmp/t_virtiofs rdadvise /mnt/data/small/1048583
check "megabyte reads with F_NOCACHE leave nothing in the cache" guest /tmp/t_virtiofs nocache /mnt/data/big/8m
check "a user's read through a descriptor opened for raw encrypted bytes fails or reads as on this Mac" \
  guest /tmp/t_virtiofs raw-read /mnt/data/big/8m /mnt/data/small/1048583 501
guest /tmp/t_virtiofs tree /mnt/data > "$work/tree.guest"
check "the guest sees the same tree as this Mac" diff -u "$work/tree.host" "$work/tree.guest"
check "the empty share is empty" prints "" guest /bin/ls -A /mnt/empty
check "the device root lists the shares" prints "$(printf 'data\nempty\nfull\nlocked\nrw\n%s' "$wide")" \
  guest /bin/sh -c 'cd /mnt/root && printf "%s\n" *'
check "a share with a 255-byte name mounts" prints "wide share" \
  guest /bin/sh -c "mkdir -p /mnt/wide && /sbin/mount -t virtiofs -o ro kido:$wide /mnt/wide && cat /mnt/wide/f && /sbin/umount /mnt/wide"
check "mount kido:locked" guest /sbin/mount -t virtiofs -o ro kido:locked /mnt/locked
check "a file the host cannot read cannot be opened" fails guest /bin/cat /mnt/locked/noaccess
check "unmount kido:locked" guest /sbin/umount /mnt/locked
check "errors of a read-only mount" guest /tmp/t_virtiofs errors /mnt/data kido:data
guest /tmp/t_virtiofs xattr-list /mnt/data/small/4097 all > "$work/xattr.guest"
check "the guest lists the extended attributes this Mac has" diff -u <("$work/t_virtiofs" xattr-list "$share/small/4097" all) \
  "$work/xattr.guest"
check "a 512-byte readdir buffer lists every entry" guest /tmp/t_virtiofs small-readdir /mnt/data/names 512
check "a 256-byte readdir buffer stops only at an entry too large for it" \
  guest /tmp/t_virtiofs small-readdir /mnt/data/names 256
check "the old getdirentries lists names up to 255 bytes and skips longer ones" \
  guest /bin/sh -c '/tmp/t_virtiofs classic-readdir /mnt/data/names && /tmp/t_virtiofs classic-readdir /mnt/data/long'
check "another user is shown as the owner right after root looked" owner_after 0 0 501 20
check "root is shown as the owner right after another user looked" owner_after 501 20 0 0
check "a program runs from the share" guest /mnt/data/exec tree /mnt/data/deep
check "a FIFO on the share works as a pipe" prints hello guest /bin/sh -c 'echo hello >> /mnt/data/links/fifo & cat /mnt/data/links/fifo; wait'
check "concurrent reads" guest /tmp/t_virtiofs stress /mnt/data 16 2

mid=$(host_fds)
check "the host opened files of the share for the guest" test "$mid" -gt "$before"
printf 'changed\n' >> "$share/small/16384"
rm "$share/many/f1"
mv "$share/deep/1/2/3/4" "$share/deep/1/2/3/four"
sleep 1.1
check "a file grown on this Mac" prints changed guest /bin/tail -c 8 /mnt/data/small/16384
check "a file removed on this Mac" fails guest /bin/test -e /mnt/data/many/f1
check "a directory renamed on this Mac" guest /bin/test -f /mnt/data/deep/1/2/3/four/5/leaf
printf 'X' | dd of="$share/small/4096" bs=1 seek=100 conv=notrunc 2>/dev/null
sleep 1.1
"$work/t_virtiofs" tree "$share/small" > "$work/small.host"
guest /tmp/t_virtiofs tree /mnt/data/small > "$work/small.guest"
check "a file rewritten in place on this Mac" diff -u "$work/small.host" "$work/small.guest"
guest /bin/cat /mnt/data/revoke >/dev/null
check "a file the host makes unreadable cannot be opened through a kept handle" revoked 0 revoke 000 /bin/cat /mnt/data/revoke
check "a program the host makes unreadable cannot be run through a kept handle" revoked 0 exec 111 /mnt/data/exec tree /mnt/data/deep
check "a descriptor already open keeps reading when another open is refused" held_open
check "an O_EVTONLY descriptor already open keeps reading when another open is refused" evtonly_kept
guest /bin/sh -c 'sysctl -w vfs.generic.virtiofs.revalidate_open=0 >/dev/null && cat /mnt/data/revoke >/dev/null'
check "with revalidate_open at 0, a refusal takes effect after the attribute timeout" revoked 1.1 revoke 000 /bin/cat /mnt/data/revoke
guest /bin/sh -c 'sysctl -w vfs.generic.virtiofs.revalidate_open=1' >/dev/null
check "a mount keeps at most idle_files closed files open on the host" idle_kept
check "with no idle handles kept, a file closed during its read-ahead is released" idle_none
check "with no idle handles kept, a file an exec read without opening it is released" exec_read_released
check "concurrent reads while every close releases a handle" guest /bin/sh -c 'w=$(sysctl -n vfs.generic.virtiofs.idle_files) &&
  sysctl -w vfs.generic.virtiofs.idle_files=1 >/dev/null && /tmp/t_virtiofs stress /mnt/data 16 1; r=$?;
  sysctl -w vfs.generic.virtiofs.idle_files=$w >/dev/null; exit $r'

rw_before=$(host_open "$rw")
check "mount kido:rw for writing" guest /sbin/mount -t virtiofs kido:rw /mnt/rw
"$work/t_virtiofs" write-tree "$work/expect" > /dev/null || { echo "FAIL the tree this Mac writes"; rc=1; }
check "the guest writes a tree" guest /tmp/t_virtiofs write-tree /mnt/rw/w
sleep 1.1
"$work/t_virtiofs" tree "$work/expect" 2>/dev/null | nostamp > "$work/expect.tree"
"$work/t_virtiofs" tree "$rw/w" > "$work/rw.host" 2>/dev/null
check "what the guest wrote is on this Mac as this Mac writes it" diff -u "$work/expect.tree" <(nostamp < "$work/rw.host")
guest /tmp/t_virtiofs tree /mnt/rw/w > "$work/rw.guest" 2>/dev/null
check "the guest reads back what this Mac holds" diff -u "$work/rw.host" "$work/rw.guest"
"$work/t_virtiofs" rw-errors "$work/errors" > "$work/errors.host"
guest /tmp/t_virtiofs rw-errors /mnt/rw/e > "$work/errors.guest"
check "errors of a writable mount match this Mac's" diff -u "$work/errors.host" "$work/errors.guest"
"$work/t_virtiofs" xattrs "$work/xattrs" > "$work/xattrs.host"
guest /tmp/t_virtiofs xattrs /mnt/rw/x > "$work/xattrs.guest"
check "extended attributes match this Mac's" diff -u "$work/xattrs.host" "$work/xattrs.guest"
check "this Mac holds the attributes the guest set" diff -u "$work/xattrs.guest" <("$work/t_virtiofs" xattr-list "$rw/x")
check "writes, stores through a mapping and truncation agree" guest /tmp/t_virtiofs coherence /mnt/rw/coherent
check "a descriptor keeps working after its file is removed or renamed" guest /tmp/t_virtiofs open-unlink /mnt/rw/ou
check "mount kido:rw a second time" guest /bin/sh -c 'mkdir -p /mnt/peer && /sbin/mount -t virtiofs kido:rw /mnt/peer'
check "edge cases of creation, stores beside growth, times and flags" \
  guest /tmp/t_virtiofs rw-edge /mnt/rw/edge /mnt/peer/edge
check "a file whose links were all removed is released on this Mac" links_released
check "a writable mapping keeps writing after its descriptor closes and this Mac revokes writing" mapped_writer_kept
check "a write handle is let go at munmap while a reader keeps the file open" unmapped_released
check "beside a writer, read advice opens nothing and a closed reader's handle is let go" readers_beside_writer
check "read advice beside appends never caches zeros inside the file" guest /tmp/t_virtiofs advise-append /mnt/rw/aa 3
check "a counter stored to in a tight loop beside appends loses no store" \
  guest /tmp/t_virtiofs store-loop /mnt/rw/sl /mnt/peer/sl 3
check "random writes, truncations and mapped stores on one file, and changes on this Mac, match a model" \
  guest /tmp/t_virtiofs model /mnt/rw/model /mnt/peer/model 30000 1
check "six writers on their own parts of one file, sharing pages, find every byte as they left it" \
  guest /tmp/t_virtiofs owned /mnt/rw/owned /mnt/peer/owned 10
check "the modification time asked for alone of a mapped file follows a write here and one on the Mac" \
  guest /tmp/t_virtiofs modtime /mnt/rw/modtime /mnt/peer/modtime
check "unmount the second mount of kido:rw" guest /sbin/umount /mnt/peer
check "mount kido:full" guest /bin/sh -c 'mkdir -p /mnt/full && /sbin/mount -t virtiofs kido:full /mnt/full'
check "a write and a push this Mac has no room for fail, and the store they met is pushed once there is room" refused_kept
check "a write this Mac has room for only in part reports what it wrote" short_kept
check "a store whose pushes failed is still pushed after its mapping is gone and this Mac revoked writing" unpushed_kept
check "unmount kido:full" guest /sbin/umount /mnt/full
check "a create whose open fails before it opens leaves nothing open for writing" raw_create_released
check "a case-only rename works after the first case probe asked about a removed file" guest /bin/sh -c \
  'mkdir -p /mnt/cs && /sbin/mount -t virtiofs kido:rw /mnt/cs && /tmp/t_virtiofs case-probe /mnt/cs/cs; r=$?;
  /sbin/umount /mnt/cs; exit $r'
check "utimes(NULL) in the guest sets the current time on this Mac" times_now
check "a page stored to through a mapping does not overwrite what this Mac appended" appended_kept
check "sync(2) pushes stores of a live mapping to this Mac" synced_map
check "an open with O_RDONLY and O_TRUNC leaves nothing open for writing" trunc_reader
check "a program this Mac makes unreadable cannot run while it is open for writing" exec_beside_writer
check "stores through a mapping twice the guest's memory reach the host, also beside reads into that mapping" \
  guest /tmp/t_virtiofs map-pressure /mnt/rw/big 2048
check "setuid and setgid bits set in the guest do not reach this Mac" setuid_dropped
check "a write handle kept for a closed writer does not open or truncate a file this Mac made read-only" \
  write_revoked hold
check "a descriptor already open for writing keeps writing" write_revoked hold-write
guest /bin/sh -c 'cp /tmp/t_virtiofs /mnt/rw/gone && cat /mnt/rw/gone > /dev/null && rm /mnt/rw/gone'
check "concurrent creates, writes, renames, unlinks and appends" guest /tmp/t_virtiofs rw-stress /mnt/rw/stress 8 5
check "writes, stores, reads into the file's own mapping, truncations and syncs on one file all keep going" \
  guest /tmp/t_virtiofs chaos /mnt/rw/chaos 10
check "eight threads that store and sync, and a read into fresh pages of a mapping of the same file, keep going" \
  guest /tmp/t_virtiofs syncers /mnt/rw/syncers 6 8
check "a write beside a thread that makes and removes the file's only mapping keeps going" \
  guest /tmp/t_virtiofs unmapping /mnt/rw/unmapping 6
check "files written and closed beside stats of them are closed on this Mac" closed_released
check "flock and fcntl locks hold between descriptors and processes of the guest" guest /tmp/t_virtiofs locks /mnt/rw/locked
check "a file removed in the guest is released on this Mac" removed_released
sleep 0.5
check "no file stays open for writing on this Mac once the guest closed it" test "$(host_writing "$rw")" == 0
check "unmount kido:rw" guest /sbin/umount /mnt/rw
rw_after=$(host_open "$rw")
echo "files of the writable share the host holds open: $rw_before before mounting, $rw_after after unmounting"
check "the host closed all but a few of the files the guest wrote" test "$rw_after" -le $((rw_before + 64))

check "unmount kido:data" guest /sbin/umount /mnt/data
check "unmount kido:empty" guest /sbin/umount /mnt/empty
check "unmount the device root" guest /sbin/umount /mnt/root
after=$(host_fds)
echo "files of the share the host holds open: $before before mounting, $mid after the walks, $after after unmounting"
check "the host closed all but a few of the files it opened for the guest" test "$after" -le $((before + 64))
forget_exact() {
  guest /bin/sh -c 'sysctl -w vfs.generic.virtiofs.entry_cache=0' >/dev/null &&
    guest /sbin/mount -t virtiofs -o ro kido:data /mnt/data >/dev/null &&
    guest /tmp/t_virtiofs lookups 500 /mnt/data/small/4097 &&
    guest /bin/sh -c 'sysctl -w vfs.generic.virtiofs.entry_cache=1' >/dev/null &&
    guest /sbin/umount /mnt/data &&
    guest /sbin/mount -t virtiofs -o ro kido:data /mnt/data >/dev/null &&
    guest /bin/sh -c '/tmp/t_virtiofs lookups 1 /mnt/data/many/f1??' &&
    guest /sbin/umount /mnt/data &&
    ! lsof -p "$vm" 2>/dev/null | grep -q "/small/4097$"
}
check "a file looked up 500 times is released once forgotten" forget_exact

guest /sbin/mount -t virtiofs -o ro kido:data /mnt/data >/dev/null
guest /tmp/t_virtiofs bench-random /mnt/data/big/32m 1000
guest /tmp/t_virtiofs bench-cold /mnt/data/big/16m 1000
guest /tmp/t_virtiofs bench /mnt/data/big/64m
guest /tmp/t_virtiofs bench-meta /mnt/data/many 3
guest /sbin/umount /mnt/data
"$kido" cp "$share/big/64m" $name:/tmp/64m
guest /tmp/t_virtiofs bench /tmp/64m | sed 's/^read/read on the container disk/'
"$work/t_virtiofs" bench "$share/big/64m" | sed 's/^read/read on this Mac/'
"$work/t_virtiofs" bench-meta "$share/many" 3 | sed 's/^/on this Mac: /'
guest /sbin/mount -t virtiofs kido:rw /mnt/rw
guest /tmp/t_virtiofs bench-write /mnt/rw/bw 256
guest /tmp/t_virtiofs bench-files /mnt/rw/bf 2000
guest /sbin/umount /mnt/rw
guest /tmp/t_virtiofs bench-write /tmp/bw 256 | sed 's/^write/write on the container disk/'
guest /tmp/t_virtiofs bench-files /tmp/bf 2000 | sed 's/^/on the container disk: /'
"$work/t_virtiofs" bench-write "$work/bw" 256 | sed 's/^write/write on this Mac/'
"$work/t_virtiofs" bench-files "$work/bf" 2000 | sed 's/^/on this Mac: /'

if [[ -s $work/timeouts ]]; then
  echo "$(timeouts) commands timed out"
  rc=1
fi
[[ $rc == 0 ]] && echo "virtiofs: all checks passed" || echo "virtiofs: FAILED"
exit $rc
