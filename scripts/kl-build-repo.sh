#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# Build, incrementally index, timestamp, and sign a kl v1 repository from a
# staged kl-all catalog directory (such as `nix build .#kl-all`).
#
# Supports incremental updates: pass `--packages name1,name2` to pack ONLY the
# newly built or changed packages while `kl-repo index` preserves all unchanged
# packages and content-addressed blobs from the existing published index.
# Enforces monotonic timestamp versions and preserves unchanged architectures.

set -euo pipefail

usage() {
  cat >&2 <<'EOF'
usage: kl-build-repo.sh --catalog DIR --repo DIR --os-version X.Y[.Z] --os-build BUILD \
                        --version N --key SSH_KEY [options]

Required:
  --catalog DIR         Path to staged kl-all output containing manifest.json and packages/
  --repo DIR            Local kl v1 repository root directory
  --os-version VER      Target OS version (e.g. 26.5)
  --os-build BUILD      Target OS build string (e.g. 25F74)
  --version N           Monotonic positive integer index and timestamp version
  --key FILE            Path to Ed25519 SSH private key for ssh-keygen -Y sign

Optional:
  --kl-version VER      Repository schema version (default: v1)
  --channel CHAN        Repository channel: latest or nightly (default: latest)
  --arch ARCH           Override architecture from manifest.json (arm64 or arm64e)
  --packages LIST       Comma-separated subset of packages to pack incrementally
  --days N              Timestamp expiry lifetime in days (default: 7)
  --kl-repo BIN         Path to kl-repo binary (default: kl-repo on PATH)
  --no-sign             Skip timestamp signing and remove any stale signature
EOF
  exit 2
}

require_arg() {
  if [ "$1" -lt 2 ]; then
    echo "kl-build-repo: $2 requires an argument" >&2
    usage
  fi
}

CATALOG=""
REPO=""
KL_VERSION="v1"
OS_VERSION=""
OS_BUILD=""
CHANNEL="latest"
ARCH=""
VERSION=""
KEY=""
PACKAGES_CSV=""
DAYS="7"
KL_REPO_BIN="kl-repo"
DO_SIGN=1

while [ $# -gt 0 ]; do
  case "$1" in
    --catalog) require_arg $# "$1"; CATALOG="$2"; shift 2 ;;
    --repo) require_arg $# "$1"; REPO="$2"; shift 2 ;;
    --kl-version) require_arg $# "$1"; KL_VERSION="$2"; shift 2 ;;
    --os-version) require_arg $# "$1"; OS_VERSION="$2"; shift 2 ;;
    --os-build) require_arg $# "$1"; OS_BUILD="$2"; shift 2 ;;
    --channel) require_arg $# "$1"; CHANNEL="$2"; shift 2 ;;
    --arch) require_arg $# "$1"; ARCH="$2"; shift 2 ;;
    --version) require_arg $# "$1"; VERSION="$2"; shift 2 ;;
    --key) require_arg $# "$1"; KEY="$2"; shift 2 ;;
    --packages) require_arg $# "$1"; PACKAGES_CSV="$2"; shift 2 ;;
    --days) require_arg $# "$1"; DAYS="$2"; shift 2 ;;
    --kl-repo) require_arg $# "$1"; KL_REPO_BIN="$2"; shift 2 ;;
    --no-sign) DO_SIGN=0; shift ;;
    -h|--help) usage ;;
    *) echo "kl-build-repo: unknown argument: $1" >&2; usage ;;
  esac
done

[ -n "$CATALOG" ] && [ -f "$CATALOG/manifest.json" ] || { echo "kl-build-repo: invalid --catalog" >&2; usage; }
[ -n "$REPO" ] || { echo "kl-build-repo: missing --repo" >&2; usage; }
[ -n "$OS_VERSION" ] || { echo "kl-build-repo: missing --os-version" >&2; usage; }
[ -n "$OS_BUILD" ] || { echo "kl-build-repo: missing --os-build" >&2; usage; }
[ -n "$VERSION" ] || { echo "kl-build-repo: missing --version" >&2; usage; }
if [ "$DO_SIGN" -eq 1 ]; then
  [ -n "$KEY" ] && [ -f "$KEY" ] || { echo "kl-build-repo: invalid --key" >&2; usage; }
fi

case "$KL_VERSION" in
  v[1-9]|v[1-9][0-9]*) ;;
  *) echo "kl-build-repo: invalid --kl-version '$KL_VERSION'" >&2; exit 2 ;;
esac
case "$CHANNEL" in
  latest|nightly) ;;
  *) echo "kl-build-repo: invalid --channel '$CHANNEL'" >&2; exit 2 ;;
esac
case "$VERSION" in
  ""|0*|*[!0-9]*) echo "kl-build-repo: --version must be a canonical positive integer" >&2; exit 2 ;;
esac
case "$DAYS" in
  ""|0*|*[!0-9]*) echo "kl-build-repo: --days must be a canonical positive integer" >&2; exit 2 ;;
esac

if ! command -v "$KL_REPO_BIN" >/dev/null 2>&1 && [ ! -x "$KL_REPO_BIN" ]; then
  echo "kl-build-repo: kl-repo binary not found: $KL_REPO_BIN" >&2
  exit 1
fi
command -v python3 >/dev/null 2>&1 || { echo "kl-build-repo: python3 is required" >&2; exit 1; }

MANIFEST_ARCH=$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["arch"])' "$CATALOG/manifest.json")
if [ -z "$ARCH" ]; then
  ARCH="$MANIFEST_ARCH"
elif [ "$ARCH" != "$MANIFEST_ARCH" ]; then
  echo "kl-build-repo: --arch '$ARCH' does not match catalog arch '$MANIFEST_ARCH'" >&2
  exit 1
fi
case "$ARCH" in
  arm64|arm64e) ;;
  *) echo "kl-build-repo: unsupported arch '$ARCH'" >&2; exit 1 ;;
esac

mkdir -p -- "$REPO"

TARGET_META_PREFIX=$(python3 - "$KL_VERSION" "$OS_VERSION" "$OS_BUILD" "$CHANNEL" <<'PY'
import re, sys
kl_ver, os_ver, os_build, channel = sys.argv[1:5]
parts = os_ver.split(".")
if len(parts) not in (2, 3) or not all(re.fullmatch(r"0|[1-9][0-9]*", p) for p in parts):
    sys.exit(f"bad os_version: {os_ver}")
if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}", os_build):
    sys.exit(f"bad os_build: {os_build}")
major, minor = parts[0], parts[1]
patch = parts[2] if len(parts) == 3 else "0"
print(f"{kl_ver}/{major}/{minor}/{patch}/{os_build}/{channel}")
PY
)

# Enforce timestamp version monotonicity and preserve unchanged architectures
# when updating a single arch in a repo hydrated from a remote store without CURRENT files.
python3 - "$REPO" "$TARGET_META_PREFIX" "$ARCH" "$VERSION" <<'PY'
import hashlib, json, os, re, sys

repo, target_prefix, active_arch, new_ver = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
meta_dir = os.path.join(repo, target_prefix)
ts_path = os.path.join(meta_dir, "timestamp.json")
if not os.path.isfile(ts_path):
    sys.exit(0)

ts = json.load(open(ts_path, encoding="utf-8"))
prev_ver = int(ts.get("version", 0))
# Same-version multi-arch publishing is allowed ONLY when the active arch has
# not yet been published at prev_ver or is currently at an older index version;
# rolling backwards is strictly refused.
active_cur = os.path.join(meta_dir, active_arch, "CURRENT")
active_idx_ver = 0
if active_arch in ts.get("indexes", {}):
    ref = ts["indexes"][active_arch]
    idx_p = os.path.join(meta_dir, active_arch, f"index.{ref['sha256']}.json")
    if os.path.isfile(idx_p):
        active_idx_ver = int(json.load(open(idx_p, encoding="utf-8")).get("version", 0))

if new_ver < prev_ver:
    sys.exit(f"kl-build-repo: refusing version rollback ({new_ver} < existing timestamp version {prev_ver})")
if new_ver == prev_ver and active_idx_ver >= new_ver:
    sys.exit(f"kl-build-repo: refusing non-monotonic version {new_ver} for {active_arch} (already at version {active_idx_ver})")

# Seed CURRENT for any other architecture present in timestamp.json so
# kl-repo timestamp never silently drops unchanged architectures.
for other_arch, ref in ts.get("indexes", {}).items():
    if other_arch == active_arch or other_arch not in ("arm64", "arm64e"):
        continue
    sha = ref.get("sha256", "")
    size = ref.get("size", -1)
    if not re.fullmatch(r"[0-9a-f]{64}", sha):
        sys.exit(f"kl-build-repo: corrupt sha256 in existing timestamp for {other_arch}")
    other_dir = os.path.join(meta_dir, other_arch)
    cur_path = os.path.join(other_dir, "CURRENT")
    if not os.path.isfile(cur_path):
        idx_path = os.path.join(other_dir, f"index.{sha}.json")
        if not os.path.isfile(idx_path):
            sys.exit(f"kl-build-repo: missing existing index {idx_path} needed to preserve {other_arch}")
        raw = open(idx_path, "rb").read()
        if len(raw) != size or hashlib.sha256(raw).hexdigest() != sha:
            sys.exit(f"kl-build-repo: digest mismatch for existing index {idx_path}")
        tmp_cur = cur_path + ".tmp"
        with open(tmp_cur, "w", encoding="utf-8") as fh:
            json.dump({"sha256": sha, "size": size}, fh)
        os.replace(tmp_cur, cur_path)
PY

PKG_STAGE_DIR="$REPO/$TARGET_META_PREFIX/$ARCH/packages.d"
if [ -d "$PKG_STAGE_DIR" ]; then
  rm -rf -- "$PKG_STAGE_DIR"
fi

python3 - "$CATALOG" "$PACKAGES_CSV" "$KL_REPO_BIN" "$REPO" "$KL_VERSION" "$OS_VERSION" "$OS_BUILD" "$CHANNEL" "$ARCH" <<'PY'
import json, os, re, subprocess, sys

catalog, pkgs_csv, kl_repo, repo, kl_ver, os_ver, os_build, channel, arch = sys.argv[1:10]
catalog_real = os.path.realpath(catalog)
data = json.load(open(os.path.join(catalog_real, "manifest.json"), encoding="utf-8"))

if pkgs_csv:
    wanted = [x.strip() for x in pkgs_csv.split(",") if x.strip()]
    if not wanted:
        sys.exit("kl-build-repo: --packages contained no package names")
else:
    wanted = None

by_name = {p["name"]: p for p in data["packages"]}
if wanted is not None:
    missing = [w for w in wanted if w not in by_name]
    if missing:
        sys.exit(f"kl-build-repo: unknown package(s) in --packages: {', '.join(missing)}")
    selected = [by_name[w] for w in wanted]
else:
    selected = data["packages"]

if not selected:
    sys.exit("kl-build-repo: catalog contains no packages")

for pkg in selected:
    rel_dir = pkg["dir"]
    if not re.fullmatch(r"packages/[A-Za-z0-9_+]([A-Za-z0-9._+@-]{0,62}[A-Za-z0-9._+-])?", rel_dir):
        sys.exit(f"kl-build-repo: unsafe package dir in manifest: {rel_dir!r}")
    pkg_dir = os.path.realpath(os.path.join(catalog_real, rel_dir))
    if not os.path.isdir(pkg_dir):
        sys.exit(f"kl-build-repo: package directory missing: {pkg_dir}")
    cmd = [
        kl_repo,
        f"--repo={repo}",
        f"--kl-version={kl_ver}",
        f"--os-version={os_ver}",
        f"--os-build={os_build}",
        f"--channel={channel}",
        "pack",
        f"--arch={arch}",
        f"--from={pkg_dir}",
        f"--name={pkg['name']}",
        f"--version={pkg['version']}",
        f"--license={pkg['license']}",
        f"--summary={pkg['summary']}",
    ]
    for dep in pkg.get("depends", []):
        cmd.append(f"--depend={dep}")
    cmd.append("--")
    cmd.extend(pkg["paths"])
    subprocess.run(cmd, check=True)
PY

"$KL_REPO_BIN" \
  "--repo=$REPO" \
  "--kl-version=$KL_VERSION" \
  "--os-version=$OS_VERSION" \
  "--os-build=$OS_BUILD" \
  "--channel=$CHANNEL" \
  index "--arch=$ARCH" "--version=$VERSION"

# Clean temporary staging entries once merged into the content-addressed index.
rm -rf -- "$PKG_STAGE_DIR"

"$KL_REPO_BIN" \
  "--repo=$REPO" \
  "--kl-version=$KL_VERSION" \
  "--os-version=$OS_VERSION" \
  "--os-build=$OS_BUILD" \
  "--channel=$CHANNEL" \
  timestamp "--version=$VERSION" "--days=$DAYS"

if [ "$DO_SIGN" -eq 1 ]; then
  "$KL_REPO_BIN" \
    "--repo=$REPO" \
    "--kl-version=$KL_VERSION" \
    "--os-version=$OS_VERSION" \
    "--os-build=$OS_BUILD" \
    "--channel=$CHANNEL" \
    sign "--key=$KEY" "--arch=$ARCH"
else
  rm -f -- "$REPO/$TARGET_META_PREFIX/timestamp.json.sig"
fi
