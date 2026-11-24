#!/bin/bash
# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# usage: scripts/bump-lib.sh LIBRARY VERSION HASH [YEAR]
# Moves one of the image's pinned libraries, curl, expat, libiconv, libutil, nghttp2 or sqlite, to VERSION,
# whose source has the SRI hash HASH. YEAR is the year of sqlite.org's download directory, for sqlite.
# Regenerates the library's export lists from the macOS SDK, builds the library, whose build fails if its
# exports differ from the SDK's, and runs its tests in test/syslibs against IMAGE, a baked image directory
# with the new library in it. Without IMAGE it builds .#image and uses that when the store is local.
# NIX is the nix command, nix by default, and FLAKE the flake it builds, this repository by default.
# Runs on a Mac with Xcode, for the SDK and the tests.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
lib=${1:?usage: bump-lib.sh LIBRARY VERSION HASH [YEAR]}
version=${2:?usage: bump-lib.sh LIBRARY VERSION HASH [YEAR]}
hash=${3:?usage: bump-lib.sh LIBRARY VERSION HASH [YEAR]}
year=${4:-}
dir=$root/nix/darwin/third-party/$lib
nix=${NIX:-nix}
flake=${FLAKE:-$root}
sdk=$(xcrun --show-sdk-path)
case $lib in
curl) stubs=(usr/lib/libcurl.4.tbd:libcurl.exports) tests="curl http2" ;;
expat) stubs=(usr/lib/libexpat.1.tbd:libexpat.exports) tests=expat ;;
libiconv)
  stubs=(usr/lib/libiconv.2.tbd:libiconv.exports usr/lib/libcharset.1.tbd:libcharset.exports)
  for t in "$sdk"/usr/lib/i18n/*.tbd; do
    stubs+=("usr/lib/i18n/${t##*/}:i18n/$(basename "$t" .tbd).exports")
  done
  tests=iconv
  ;;
libutil) stubs=(usr/lib/libutil.tbd:libutil.exports) tests=util ;;
nghttp2) stubs=(usr/lib/libapple_nghttp2.tbd:libapple_nghttp2.exports) tests="nghttp2 http2" ;;
sqlite) stubs=(usr/lib/libsqlite3.tbd:libsqlite3.exports) tests=sqlite ;;
*) echo "bump-lib.sh: no pinned library $lib" >&2; exit 2 ;;
esac
python3 - "$dir/source.json" "$version" "$hash" "$year" <<'PY'
import json, sys
path, version, sri, year = sys.argv[1:]
with open(path) as f:
    pin = json.load(f)
pin["version"], pin["hash"] = version, sri
if year:
    pin["year"] = year
with open(path, "w") as f:
    f.write(json.dumps(pin, indent=2) + "\n")
PY
for s in "${stubs[@]}"; do
  python3 "$root/scripts/tbd-exports.py" "$sdk" "${s%%:*}" > "$dir/${s#*:}"
done
git -C "$root" add -- "$dir"
$nix --extra-experimental-features "nix-command flakes" build --no-link "$flake#$lib"
image=${IMAGE:-}
if [[ -z $image ]]; then
  image=$($nix --extra-experimental-features "nix-command flakes" build --no-link --print-out-paths "$flake#image")
  [[ -d $image ]] || { echo "bump-lib.sh: copy $image here and set IMAGE to it" >&2; exit 1; }
fi
"$root/test/syslibs/run.sh" "$image" $tests
