#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: test/syslibs/run.sh IMAGE_DIR [TEST...]
# Builds the tests of macOS's libraries in /usr/lib with Xcode's clang against the macOS SDK, the ABI real
# macOS tools use, and runs them in a container of IMAGE_DIR with kido (KIDO, default kido on PATH;
# KIDO_BACKEND and KIDO_QEMU as for kido's test/e2e.sh). TEST names a subset: curl, http2, curses, edit,
# bz2, lzma, util, expat, sqlite, nghttp2 and iconv, all by default. The tests of the libraries the image
# builds for arm64e run as arm64e and as arm64 programs. expat, sqlite, nghttp2, iconv and http2 also run
# on this Mac against macOS's own libraries, and each side must print the same results but for the
# differences by design, which are shown apart. util must pass on this Mac as well. http2 runs a Go
# HTTP/2 server in the container, published on PORT and the two ports after it (default 18463), which
# both sides use. Each command in the container fails after TIMEOUT seconds (default 120). Uses a
# throwaway KIDO_HOME. The curl test needs the network: it fetches https://example.com.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
lib=$here/../../nix/darwin/third-party
img=$1
shift
tests=${*:-curl http2 curses edit bz2 lzma util expat sqlite nghttp2 iconv}
kido=${KIDO:-kido}
port=${PORT:-18463}
limit=${TIMEOUT:-120}
work=$(mktemp -d "${TMPDIR:-/tmp}/kido-syslibs.XXXX")
trap 'rm -rf "$work"' EXIT
export KIDO_HOME=$work/home
sdk=$(xcrun --show-sdk-path)
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
wanted() {
  [[ " $tests " == *" $1 "* ]]
}
build() {
  local arch=$1 name=$2 src=$3
  shift 3
  xcrun clang -arch "$arch" -mmacosx-version-min=15.0 -O1 -Wno-deprecated-declarations \
    -Wno-unguarded-availability-new -I"$here/../libsystem" -o "$work/$name" "$src" "$@"
}
source_of() {
  case $1 in
  curl) echo "$lib/curl/t_curl.c" ;;
  http2) echo "$lib/curl/t_http2.c" ;;
  util) echo "$lib/libutil/t_util.c" ;;
  expat | sqlite | nghttp2) echo "$lib/$1/t_$1.c" ;;
  iconv) echo "$lib/libiconv/t_iconv.c" ;;
  *) echo "$here/t_$1.c" ;;
  esac
}
link_of() {
  case $1 in
  curl | http2) echo -lcurl ;;
  curses) echo -lncurses -lpanel -lmenu -lform ;;
  edit) echo -ledit ;;
  bz2) echo -lbz2 ;;
  lzma) echo -llzma ;;
  util) echo -lutil ;;
  expat) echo -lexpat ;;
  sqlite) echo -lsqlite3 ;;
  nghttp2) echo "$sdk/usr/lib/libapple_nghttp2.tbd" ;;
  iconv) echo -liconv ;;
  esac
}
arm64e_lib() {
  [[ " util expat sqlite nghttp2 iconv " == *" $1 "* ]]
}
compared() {
  [[ " expat sqlite nghttp2 iconv http2 " == *" $1 "* ]]
}
by_design() {
  case $1 in
  expat) echo '^(version |(newname|surrogate|badxmldecl) (whole|bytewise):|laughs (whole|bytewise): text )' ;;
  sqlite) echo '^(version |source |pragma: cache_spill=|option (CCCRYPT256|CODEC=|COMPILER=|ENABLE_SQLLOG|HAS_CODEC_RESTRICTED|MAX_SCHEMA=|MUTEX_))' ;;
  nghttp2) echo '^version ' ;;
  http2) echo '^libcurl ' ;;
  *) echo '^$' ;;
  esac
}
for t in $tests; do
  build arm64 "$t" "$(source_of "$t")" $(link_of "$t") || exit 1
  if arm64e_lib "$t"; then
    build arm64e "$t-arm64e" "$(source_of "$t")" $(link_of "$t") || exit 1
  fi
done
if wanted http2; then
  GOOS=darwin GOARCH=arm64 CGO_ENABLED=0 go build -trimpath -o "$work/h2server" "$lib/curl/h2server/main.go" || exit 1
fi
gates=()
if wanted sqlite; then
  for v in 0.0 10.13 10.14 10.15 10.16; do
    mkdir -p "$work/gate/sdk-$v"
    vtool -set-build-version macos 11.0 "$v" -replace -output "$work/gate/sdk-$v/t_sqlite" "$work/sqlite" 2>/dev/null &&
      codesign -f -s - "$work/gate/sdk-$v/t_sqlite" 2>/dev/null ||
      { echo "syslibs: cannot make the SDK $v copy of t_sqlite" >&2; exit 1; }
    gates+=("sdk-$v/t_sqlite")
  done
  for n in sqlite3 testfixture xctest; do
    mkdir -p "$work/gate/name-$n" && cp "$work/sqlite" "$work/gate/name-$n/$n" || exit 1
    gates+=("name-$n/$n")
  done
  mkdir -p "$work/gate/name-xctest-sdk-10.13" &&
    cp "$work/gate/sdk-10.13/t_sqlite" "$work/gate/name-xctest-sdk-10.13/xctest" || exit 1
  gates+=("name-xctest-sdk-10.13/xctest")
fi
"$kido" image import "$img" syslibs-test >/dev/null || exit 1
"$kido" run -d --name syslibs-test -p 127.0.0.1:$port:8443 -p 127.0.0.1:$((port + 1)):8444 \
  -p 127.0.0.1:$((port + 2)):8080 syslibs-test >/dev/null || exit 1
trap '"$kido" rm -f syslibs-test >/dev/null 2>&1; rm -rf "$work"' EXIT
g=/var/root/syslibs
timed "$kido" exec syslibs-test /bin/mkdir -p $g/h2 || exit 1
for t in $tests h2server; do
  for f in "$work/$t" "$work/$t-arm64e"; do
    [[ -f $f ]] || continue
    "$kido" cp "$f" syslibs-test:$g/ || exit 1
  done
done
for v in ${gates[@]+"${gates[@]}"}; do
  timed "$kido" exec syslibs-test /bin/mkdir -p "$g/gate/${v%/*}" || exit 1
  "$kido" cp "$work/gate/$v" "syslibs-test:$g/gate/${v%/*}/" || exit 1
done
if wanted http2; then
  timed "$kido" exec -d syslibs-test $g/h2server -dir $g/h2 || exit 1
  for i in $(seq 1 50); do
    timed "$kido" exec syslibs-test /bin/test -f $g/h2/ca.pem && break
    sleep 0.2
  done
  "$kido" cp syslibs-test:$g/h2/ca.pem "$work/ca.pem" || exit 1
fi
args() {
  case $1 in
  http2) echo "https://127.0.0.1:${2:-8443} https://127.0.0.1:$((${2:-8443} + 1)) http://127.0.0.1:${3:-8080} ${4:-$g/h2/ca.pem}" ;;
  sqlite | util) echo "${2:-$g}" ;;
  esac
}
rc=0
for t in $tests; do
  for bin in $t $t-arm64e; do
    [[ -f $work/$bin ]] || continue
    timed "$kido" exec syslibs-test $g/$bin $(args "$t") > "$work/$bin.guest" 2>&1 || rc=1
    cat "$work/$bin.guest"
  done
  if [[ $t == util ]]; then
    mkdir -p "$work/mac-util"
    "$work/util" "$work/mac-util" | sed 's/^/on this Mac: /' || rc=1
  fi
  compared "$t" || continue
  mode=
  [[ $t == expat ]] && mode=compare
  case $t in
  http2) "$work/http2" $(args http2 $port $((port + 2)) "$work/ca.pem") > "$work/$t.mac" 2>&1 ;;
  sqlite) mkdir -p "$work/mac-sqlite" && "$work/sqlite" "$work/mac-sqlite" > "$work/$t.mac" 2>&1 ;;
  *) "$work/$t" $mode > "$work/$t.mac" 2>&1 ;;
  esac
  for bin in $t $t-arm64e; do
    [[ -f $work/$bin.guest ]] || continue
    if [[ $t == expat ]]; then
      timed "$kido" exec syslibs-test $g/$bin compare > "$work/$bin.guest" 2>&1
    fi
    for side in mac guest; do
      file=$work/$t.mac
      [[ $side == guest ]] && file=$work/$bin.guest
      grep -Ev '^(FAIL |[a-z0-9]+: [0-9]+/[0-9]+ passed$)' "$file" | grep -Ev "$(by_design "$t")" > "$work/$bin.$side.same"
      grep -E "$(by_design "$t")" "$file" > "$work/$bin.$side.design"
    done
    if diff "$work/$bin.mac.same" "$work/$bin.guest.same" > "$work/$bin.diff"; then
      echo "$bin: the same as macOS: $(wc -l < "$work/$bin.guest.same" | tr -d ' ') results"
    else
      echo "$bin: differs from macOS (< macOS, > guest):"
      head -n 60 "$work/$bin.diff"
      [[ $(wc -l < "$work/$bin.diff") -le 60 ]] || echo "... $(($(wc -l < "$work/$bin.diff") - 60)) more lines"
      rc=1
    fi
    if ! diff -q "$work/$bin.mac.design" "$work/$bin.guest.design" >/dev/null; then
      echo "$bin: different by design (< macOS, > guest):"
      diff "$work/$bin.mac.design" "$work/$bin.guest.design" | grep '^[<>]'
    fi
  done
done
for v in ${gates[@]+"${gates[@]}"}; do
  timed "$kido" exec syslibs-test "$g/gate/$v" "$g" gate > "$work/gate.guest" 2>&1 || rc=1
  "$work/gate/$v" "$work" gate > "$work/gate.mac" 2>&1 || rc=1
  if diff "$work/gate.mac" "$work/gate.guest" > "$work/gate.diff"; then
    echo "sqlite ${v%/*}: the same as macOS: $(head -n 1 "$work/gate.guest")"
  else
    echo "sqlite ${v%/*}: differs from macOS (< macOS, > guest):"
    cat "$work/gate.diff"
    rc=1
  fi
done
exit $rc
