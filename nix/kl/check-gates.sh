#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# Enforce the four mandatory kl package build gates on a staged package tree:
#   Gate 1: Mach-O ABI, platform/version, LC_ID_DYLIB, and path confinement
#   Gate 2: ARMv8.0-A ISA portability scan (arm64) or arm64e ABI scan (arm64e)
#   Gate 3: otool -L / load-command dependency closure against base + kl deps
#   Gate 4: Permissive SPDX license allowlist and bundled license text

set -euo pipefail

usage() {
  cat >&2 <<'EOF'
usage: check-gates.sh --root DIR --pkg-name NAME --arch arm64|arm64e --license SPDX \
                      --base-dylibs FILE --otool PATH --objdump PATH [--allowed-dep PATH]...
EOF
  exit 2
}

require_arg() {
  if [ "$1" -lt 2 ]; then
    echo "check-gates: $2 requires an argument" >&2
    usage
  fi
}

ROOT=""
PKG_NAME=""
ARCH=""
LICENSE_EXPR=""
BASE_DYLIBS=""
OTOOL=""
OBJDUMP=""
ALLOWED_DEPS=()

while [ $# -gt 0 ]; do
  case "$1" in
    --root) require_arg $# "$1"; ROOT="$2"; shift 2 ;;
    --pkg-name) require_arg $# "$1"; PKG_NAME="$2"; shift 2 ;;
    --arch) require_arg $# "$1"; ARCH="$2"; shift 2 ;;
    --license) require_arg $# "$1"; LICENSE_EXPR="$2"; shift 2 ;;
    --base-dylibs) require_arg $# "$1"; BASE_DYLIBS="$2"; shift 2 ;;
    --otool) require_arg $# "$1"; OTOOL="$2"; shift 2 ;;
    --objdump) require_arg $# "$1"; OBJDUMP="$2"; shift 2 ;;
    --allowed-dep) require_arg $# "$1"; ALLOWED_DEPS+=("$2"); shift 2 ;;
    *) echo "check-gates: unknown argument: $1" >&2; usage ;;
  esac
done

[ -n "$ROOT" ] && [ -d "$ROOT" ] || { echo "check-gates: invalid --root" >&2; usage; }
[ -n "$PKG_NAME" ] || { echo "check-gates: missing --pkg-name" >&2; usage; }
[ -n "$LICENSE_EXPR" ] || { echo "check-gates: missing --license" >&2; usage; }
[ -n "$BASE_DYLIBS" ] && [ -f "$BASE_DYLIBS" ] || { echo "check-gates: invalid --base-dylibs" >&2; usage; }
[ -n "$OTOOL" ] && [ -x "$OTOOL" ] || { echo "check-gates: invalid --otool" >&2; usage; }
[ -n "$OBJDUMP" ] && [ -x "$OBJDUMP" ] || { echo "check-gates: invalid --objdump" >&2; usage; }

case "$ARCH" in
  arm64|arm64e) ;;
  *) echo "check-gates: unsupported --arch '$ARCH' (expected arm64 or arm64e)" >&2; exit 2 ;;
esac

# Validate package name strictly against kl's check_name grammar (no traversal).
if ! printf '%s' "$PKG_NAME" | LC_ALL=C grep -Eq '^[A-Za-z0-9_+][A-Za-z0-9._+-]{0,63}$'; then
  echo "check-gates: invalid --pkg-name '$PKG_NAME'" >&2
  exit 2
fi

ROOT="${ROOT%/}"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/kl-gates.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

fail() {
  echo "GATE FAIL [$PKG_NAME/$ARCH]: $*" >&2
  exit 1
}

# ---------------------------------------------------------------------------
# Gate 4: Permissive SPDX License Allowlist (strict grammar, zero globbing)
# ---------------------------------------------------------------------------
# Validate characters (hyphen at end so no unintended ASCII range), balanced
# parentheses, proper binary operator grammar, and allowlist separation between
# base licenses and WITH exceptions.
spdx_err=$(SPDX_EXPR="$LICENSE_EXPR" LC_ALL=C awk '
  BEGIN {
    expr = ENVIRON["SPDX_EXPR"];
    if (length(expr) == 0 || expr ~ /[^A-Za-z0-9.+_ ()-]/) {
      print "invalid characters in SPDX expression";
      exit 1;
    }
    split("Apache-2.0 MIT BSD-2-Clause BSD-2-Clause-FreeBSD BSD-3-Clause 0BSD PSF-2.0 Zlib ISC blessing LicenseRef-Public-Domain", base_arr, " ");
    for (i in base_arr) allowed_base[base_arr[i]] = 1;
    split("LLVM-exception Swift-exception", exc_arr, " ");
    for (i in exc_arr) allowed_exc[exc_arr[i]] = 1;

    # Tokenize parentheses and whitespace-separated words without shell globbing.
    gsub(/\(/, " ( ", expr);
    gsub(/\)/, " ) ", expr);
    n = split(expr, toks, /[ \t]+/);
    depth = 0;
    # state: "term" expects "(" or license; "op" expects ")", "AND", "OR", or "WITH";
    #        "after_with" expects exception; "after_exc" expects ")", "AND", or "OR".
    state = "term";
    lic_count = 0;
    for (i = 1; i <= n; i++) {
      tok = toks[i];
      if (tok == "") continue;
      if (tok == "(") {
        if (state != "term") { print "unexpected ( in SPDX expression"; exit 1; }
        depth++;
      } else if (tok == ")") {
        if (state != "op" && state != "after_exc") { print "unexpected ) in SPDX expression"; exit 1; }
        depth--;
        if (depth < 0) { print "unmatched ) in SPDX expression"; exit 1; }
        state = "after_exc";
      } else if (tok == "AND" || tok == "OR") {
        if (state != "op" && state != "after_exc") { print "unexpected operator " tok; exit 1; }
        state = "term";
      } else if (tok == "WITH") {
        if (state != "op") { print "unexpected WITH in SPDX expression"; exit 1; }
        state = "after_with";
      } else if (state == "term") {
        if (!(tok in allowed_base)) {
          print "license " tok " is not on the permissive SPDX allowlist";
          exit 1;
        }
        lic_count++;
        state = "op";
      } else if (state == "after_with") {
        if (!(tok in allowed_exc)) {
          print "exception " tok " is not on the permissive SPDX exception allowlist";
          exit 1;
        }
        state = "after_exc";
      } else {
        print "unexpected token " tok " in SPDX expression";
        exit 1;
      }
    }
    if (depth != 0 || lic_count == 0 || (state != "op" && state != "after_exc")) {
      print "malformed SPDX expression";
      exit 1;
    }
  }
' 2>&1) || fail "Gate 4: $spdx_err ('$LICENSE_EXPR')"

license_doc="$ROOT/usr/share/doc/kl/$PKG_NAME/LICENSE"
[ -f "$license_doc" ] && [ ! -L "$license_doc" ] && [ -s "$license_doc" ] || \
  fail "Gate 4: missing or empty upstream license file at usr/share/doc/kl/$PKG_NAME/LICENSE"

echo "Gate 4 PASS [$PKG_NAME/$ARCH]: SPDX '$LICENSE_EXPR' allowlisted, license text present"

# ---------------------------------------------------------------------------
# Managed-directory confinement, symlink safety, and case-collision checks
# ---------------------------------------------------------------------------
is_managed_path() {
  case "$1" in
    usr/lib/*|usr/libexec/*|usr/bin/*|usr/share/*|System/Library/Frameworks/*|System/Library/PrivateFrameworks/*)
      return 0 ;;
    *)
      return 1 ;;
  esac
}

is_managed_ancestor() {
  case "$1" in
    usr|usr/lib|usr/libexec|usr/bin|usr/share|System|System/Library|System/Library/Frameworks|System/Library/PrivateFrameworks)
      return 0 ;;
    *)
      return 1 ;;
  esac
}

has_cntrl() {
  LC_ALL=C awk 'BEGIN { exit (ENVIRON["VAL"] ~ /[[:cntrl:]]/) ? 0 : 1 }'
}

: > "$TMP/folded_paths"
: > "$TMP/folded_symlinks"
: > "$TMP/macho_files"
: > "$TMP/self_provides"

while IFS= read -r -d '' entry; do
  rel="${entry#"$ROOT"/}"
  [ "$rel" != "$entry" ] || continue

  if VAL="$rel" has_cntrl; then
    fail "Gate 1: path contains control character"
  fi

  if [ -L "$entry" ]; then
    is_managed_path "$rel" || fail "Gate 1: symlink '$rel' is outside kl-managed directories"
    target=$(readlink "$entry")
    if VAL="$target" has_cntrl; then
      fail "Gate 1: symlink '$rel' target contains control character"
    fi
    case "$target" in
      /*|""|*/|./*|*/./*|*//*)
        fail "Gate 1: symlink '$rel' has non-canonical or absolute target '$target'" ;;
    esac
    # Verify relative target resolves inside kl-managed directories without climbing past root.
    resolved=$(REL="$rel" TGT="$target" LC_ALL=C awk 'BEGIN {
      p = ENVIRON["REL"];
      t = ENVIRON["TGT"];
      n = split(p, a, "/");
      n--; # drop leaf
      m = split(t, b, "/");
      saw_seg = 0;
      for (i = 1; i <= m; i++) {
        if (b[i] == "..") {
          if (saw_seg || n <= 0) { print "BAD"; exit 0; }
          n--;
        } else if (b[i] == "" || b[i] == ".") {
          print "BAD"; exit 0;
        } else {
          saw_seg = 1;
          a[++n] = b[i];
        }
      }
      out = "";
      for (i = 1; i <= n; i++) out = (i == 1 ? a[i] : out "/" a[i]);
      print out;
    }')
    [ "$resolved" != "BAD" ] && is_managed_path "$resolved" || \
      fail "Gate 1: symlink '$rel' -> '$target' resolves outside kl-managed directories ('$resolved')"

    folded=$(printf '%s' "$rel" | tr '[:upper:]' '[:lower:]')
    printf '%s\t%s\n' "$folded" "$rel" >> "$TMP/folded_paths"
    printf '%s\n' "$folded" >> "$TMP/folded_symlinks"
    continue
  elif [ -d "$entry" ]; then
    if ! is_managed_path "$rel" && ! is_managed_ancestor "$rel"; then
      fail "Gate 1: directory '$rel' is outside kl-managed directories"
    fi
    folded=$(printf '%s' "$rel" | tr '[:upper:]' '[:lower:]')
    printf '%s\t%s\n' "$folded" "$rel" >> "$TMP/folded_paths"
    continue
  elif [ ! -f "$entry" ]; then
    fail "Gate 1: '$rel' is not a regular file, directory, or symlink"
  fi

  is_managed_path "$rel" || fail "Gate 1: file '$rel' is outside kl-managed directories"
  folded=$(printf '%s' "$rel" | tr '[:upper:]' '[:lower:]')
  printf '%s\t%s\n' "$folded" "$rel" >> "$TMP/folded_paths"

  # Inspect 4-byte file magic to identify Mach-O or reject foreign/universal binaries.
  magic=$(od -An -tx1 -N4 "$entry" 2>/dev/null | tr -d ' \n')
  case "$magic" in
    cffaedfe)
      printf '%s\n' "$rel" >> "$TMP/macho_files"
      ;;
    cefaedfe|feedface|feedfacf|cafebabe|bebafeca|cafebabf|bfbafeca)
      fail "Gate 1: unsupported Mach-O magic '$magic' in '$rel' (only thin 64-bit little-endian Mach-O is allowed)"
      ;;
    7f454c46)
      fail "Gate 1: ELF binary leaked into Darwin kl package: '$rel'"
      ;;
  esac
done < <(find "$ROOT" -mindepth 1 -print0 | LC_ALL=C sort -z)

# Reject case-insensitive path collisions (matches kl manifest.rs).
dups=$(cut -f1 "$TMP/folded_paths" | LC_ALL=C sort | uniq -d || true)
if [ -n "$dups" ]; then
  fail "Gate 1: case-insensitive path collision detected: $dups"
fi

# Reject any entry whose ancestor is a symlink (case-insensitively, matches kl manifest.rs).
if [ -s "$TMP/folded_symlinks" ]; then
  sym_ancestor_bad=$(LC_ALL=C awk -F'\t' '
    NR == FNR { syms[$1] = 1; next }
    {
      p = $1;
      while (match(p, /\/[^\/]+$/)) {
        p = substr(p, 1, RSTART - 1);
        if (p in syms) { print $2 " lies under symlink " p; exit 1; }
      }
    }
  ' "$TMP/folded_symlinks" "$TMP/folded_paths" 2>&1) || fail "Gate 1: $sym_ancestor_bad"
fi

# ---------------------------------------------------------------------------
# Gate 1: Mach-O ABI & Platform/Version Check
# ---------------------------------------------------------------------------
macho_count=0
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  macho_count=$((macho_count + 1))
  full="$ROOT/$rel"

  hdr=$("$OTOOL" -hv "$full")
  hdr_line=$(printf '%s\n' "$hdr" | awk '/^MH_MAGIC_64/ { print; exit }')
  [ -n "$hdr_line" ] || fail "Gate 1: '$rel' is not a valid MH_MAGIC_64 binary"

  cputype=$(printf '%s\n' "$hdr_line" | awk '{ print $2 }')
  cpusubtype=$(printf '%s\n' "$hdr_line" | awk '{ print $3 }')
  filetype=$(printf '%s\n' "$hdr_line" | awk '{ print $5 }')

  [ "$cputype" = "ARM64" ] || fail "Gate 1: '$rel' has cputype '$cputype', expected ARM64"
  if [ "$ARCH" = "arm64" ]; then
    case "$cpusubtype" in
      ALL|0) ;;
      *) fail "Gate 1: '$rel' has cpusubtype '$cpusubtype', expected ALL for arm64" ;;
    esac
  else
    case "$cpusubtype" in
      E|ARM64E|2) ;;
      *) fail "Gate 1: '$rel' has cpusubtype '$cpusubtype', expected ARM64E/E for arm64e" ;;
    esac
  fi

  case "$filetype" in
    DYLIB|EXECUTE|BUNDLE) ;;
    *) fail "Gate 1: '$rel' has unexpected Mach-O filetype '$filetype'" ;;
  esac

  load_cmds="$TMP/load_cmds.txt"
  "$OTOOL" -l "$full" > "$load_cmds"

  # Parse LC_BUILD_VERSION, LC_ID_DYLIB, LC_LOAD_DYLINKER, LC_RPATH, LC_DYLD_ENVIRONMENT
  # in a single strict awk pass preserving spaces in paths and counting occurrences.
  cmd_summary=$(LC_ALL=C awk '
    function strip_name(line) {
      sub(/^[[:space:]]*name[[:space:]]+/, "", line);
      sub(/[[:space:]]+\(offset [0-9]+\)[[:space:]]*$/, "", line);
      return line;
    }
    $1 == "Load" && $2 == "command" { cur_cmd = ""; next }
    $1 == "cmd" {
      cur_cmd = $2;
      if (cur_cmd == "LC_BUILD_VERSION") bv_count++;
      else if (cur_cmd == "LC_ID_DYLIB") id_count++;
      else if (cur_cmd == "LC_LOAD_DYLINKER") dl_count++;
      else if (cur_cmd == "LC_RPATH") rp_count++;
      else if (cur_cmd == "LC_DYLD_ENVIRONMENT") env_count++;
      next;
    }
    cur_cmd == "LC_BUILD_VERSION" && $1 == "platform" { bv_plat = $2; next }
    cur_cmd == "LC_BUILD_VERSION" && $1 == "minos" { bv_minos = $2; next }
    cur_cmd == "LC_BUILD_VERSION" && $1 == "sdk" { bv_sdk = $2; next }
    cur_cmd == "LC_ID_DYLIB" && $1 == "name" { id_name = strip_name($0); next }
    cur_cmd == "LC_ID_DYLIB" && $1 == "current" && $2 == "version" { id_cur = $3; next }
    cur_cmd == "LC_ID_DYLIB" && $1 == "compatibility" && $2 == "version" { id_compat = $3; next }
    cur_cmd == "LC_LOAD_DYLINKER" && $1 == "name" { dl_name = strip_name($0); next }
    END {
      sep = sprintf("%c", 31);
      printf "%d%s%s%s%s%s%s%s%d%s%s%s%s%s%s%s%d%s%s%s%d%s%d\n", \
        bv_count+0, sep, bv_plat, sep, bv_minos, sep, bv_sdk, sep, \
        id_count+0, sep, id_name, sep, id_cur, sep, id_compat, sep, \
        dl_count+0, sep, dl_name, sep, rp_count+0, sep, env_count+0;
    }
  ' "$load_cmds")

  IFS=$'\x1f' read -r bv_count bv_plat bv_minos bv_sdk id_count id_name id_cur id_compat dl_count dl_name rp_count env_count <<< "$cmd_summary"

  [ "$bv_count" -eq 1 ] || fail "Gate 1: '$rel' must have exactly 1 LC_BUILD_VERSION (found $bv_count)"
  case "$bv_plat" in
    1|MACOS|macos) ;;
    *) fail "Gate 1: '$rel' LC_BUILD_VERSION platform is '$bv_plat', expected macOS (1/MACOS)" ;;
  esac
  printf '%s' "$bv_minos" | LC_ALL=C grep -Eq '^[1-9][0-9]*\.[0-9]+(\.[0-9]+)?$' || \
    fail "Gate 1: '$rel' has invalid LC_BUILD_VERSION minos '$bv_minos'"
  printf '%s' "$bv_sdk" | LC_ALL=C grep -Eq '^[1-9][0-9]*\.[0-9]+(\.[0-9]+)?$' || \
    fail "Gate 1: '$rel' has invalid LC_BUILD_VERSION sdk '$bv_sdk'"

  [ "$rp_count" -eq 0 ] || fail "Gate 1: '$rel' carries disallowed LC_RPATH ($rp_count found)"
  [ "$env_count" -eq 0 ] || fail "Gate 1: '$rel' carries disallowed LC_DYLD_ENVIRONMENT ($env_count found)"

  if [ "$filetype" = "DYLIB" ]; then
    [ "$id_count" -eq 1 ] || fail "Gate 1: dylib '$rel' must have exactly 1 LC_ID_DYLIB (found $id_count)"
    [ "$dl_count" -eq 0 ] || fail "Gate 1: dylib '$rel' carries unexpected LC_LOAD_DYLINKER"
    expected_id="/$rel"
    [ "$id_name" = "$expected_id" ] || \
      fail "Gate 1: '$rel' LC_ID_DYLIB is '$id_name', expected '$expected_id'"
    printf '%s' "$id_cur" | LC_ALL=C grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' && [ "$id_cur" != "0.0.0" ] || \
      fail "Gate 1: '$rel' has invalid LC_ID_DYLIB current version '$id_cur'"
    printf '%s' "$id_compat" | LC_ALL=C grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' && [ "$id_compat" != "0.0.0" ] || \
      fail "Gate 1: '$rel' has invalid LC_ID_DYLIB compatibility version '$id_compat'"
    printf '%s\n' "$id_name" >> "$TMP/self_provides"
  elif [ "$filetype" = "EXECUTE" ]; then
    [ "$id_count" -eq 0 ] || fail "Gate 1: executable '$rel' carries unexpected LC_ID_DYLIB"
    [ "$dl_count" -eq 1 ] || fail "Gate 1: executable '$rel' must have exactly 1 LC_LOAD_DYLINKER (found $dl_count)"
    [ "$dl_name" = "/usr/lib/dyld" ] || \
      fail "Gate 1: executable '$rel' has LC_LOAD_DYLINKER '$dl_name', expected '/usr/lib/dyld'"
  else
    # BUNDLE (e.g. Python extension .so)
    [ "$id_count" -eq 0 ] || fail "Gate 1: bundle '$rel' carries unexpected LC_ID_DYLIB"
    [ "$dl_count" -eq 0 ] || fail "Gate 1: bundle '$rel' carries unexpected LC_LOAD_DYLINKER"
  fi
done < "$TMP/macho_files"

echo "Gate 1 PASS [$PKG_NAME/$ARCH]: $macho_count Mach-O file(s) verified (ARM64/$ARCH, macOS LC_BUILD_VERSION, LC_ID_DYLIB)"

# ---------------------------------------------------------------------------
# Gate 2: ARMv8.0-A ISA Portability Scan (arm64) / arm64e ABI Scan (arm64e)
# ---------------------------------------------------------------------------
# For arm64: forbid ARMv8.1+ LSE atomics, LOR, RDM, DotProd, FP16/FHM, SHA512,
# SM3/SM4, SHA3, LRCPC/2/3, JSConv (fjcvtzs), PAC, FlagM/2, BTI, SB, FRINTTS,
# MTE, Specres, I8MM, BF16, WFxT, LS64, HBC, SVE/SVE2, SME, and MOPS.
ARMV80_FORBIDDEN_RE='^(ldadd|stadd|swp|cas|casp|ldclr|stclr|ldset|stset|ldeor|steor|ldsmax|ldsmin|ldumax|ldumin|stsmax|stsmin|stumax|stumin|ldlar|stllr|sqrdmlah|sqrdmlsh|sdot|udot|usdot|sudot|fcmla|fcadd|fmlal|fmlsl|bfdot|bfmmla|bfcvt|bfmlal|smmla|ummla|usmmla|sha512h|sha512su|sm3|sm4|eor3|rax1|xar|bcax|ldapr|ldapur|stlur|ldap1|stl1|ldiapp|stilp|fjcvtzs|pac|aut|xpac|braa|brab|blraa|blrab|retaa|retab|eretaa|eretab|ldraa|ldrab|cfinv|setf8|setf16|rmif|axflag|xaflag|bti|sb$|frint32|frint64|irg$|gmi$|addg$|subg$|subp|stg$|stzg$|st2g$|stz2g$|stgp$|ldg|stgm$|stzgm$|cfp$|dvp$|cpp$|wfet$|wfit$|ld64b$|st64b|bc\.|whilel|whileh|whileg|ptrue|pfalse|ptest|rdvl|rdsvl|addvl|addpl|addsvl|addspl|cntb|cnth|cntw|cntd|smstart|smstop|zero$|mopa$|mops$|fmopa|fmops|cpyf?p|cpyf?m|cpyf?e|setg?p|setg?m|setg?e)'
# For arm64e: forbid ARMv9-only SVE/SVE2/SME/MOPS instructions not in Apple arm64e baseline.
ARM64E_FORBIDDEN_RE='^(whilel|whileh|whileg|ptrue|pfalse|ptest|rdvl|rdsvl|addvl|addpl|addsvl|addspl|cntb|cnth|cntw|cntd|smstart|smstop|zero$|mopa$|mops$|fmopa|fmops|cpyf?p|cpyf?m|cpyf?e|setg?p|setg?m|setg?e)'

total_insns=0
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  full="$ROOT/$rel"
  disasm="$TMP/disasm.txt"
  "$OBJDUMP" -d --no-show-raw-insn "$full" > "$disasm"
  insns=$(awk '/^[[:space:]]*[0-9a-f]+:/ { n++ } END { print n+0 }' "$disasm")
  [ "$insns" -gt 0 ] || fail "Gate 2: '$rel' produced 0 disassembled instructions"
  total_insns=$((total_insns + insns))

  if [ "$ARCH" = "arm64" ]; then
    re="$ARMV80_FORBIDDEN_RE"
  else
    re="$ARM64E_FORBIDDEN_RE"
  fi

  bad=$(ARCH="$ARCH" FORB_RE="$re" LC_ALL=C awk '
    /^[[:space:]]*[0-9a-f]+:/ {
      mnem = tolower($2);
      # Extract operand text after mnemonic (strip trailing ; comment).
      rest = $0;
      sub(/^[[:space:]]*[0-9a-f]+:[[:space:]]*[^[:space:]]+[[:space:]]*/, "", rest);
      sub(/[[:space:]]*;.*$/, "", rest);
      ops = tolower(rest);

      if (mnem ~ ENVIRON["FORB_RE"]) {
        print mnem;
        next;
      }
      # Reject SVE/SME register operands (z0-z31, p0-p15, za) on both arm64 and arm64e.
      if (ops ~ /(^|[^a-z0-9_])(z([0-9]|[12][0-9]|3[01])|p([0-9]|1[0-5])|za)([^a-z0-9_]|$)/) {
        print mnem " [sve/sme-reg]";
        next;
      }
      # On arm64 (ARMv8.0-A), also reject FEAT_FP16 half-precision arithmetic operands.
      if (ENVIRON["ARCH"] == "arm64" &&
          mnem ~ /^(fadd|fsub|fmul|fdiv|fmla|fmls|fmadd|fmsub|fnmadd|fnmsub|fnmul|fcmp|fcmpe|fcsel|fmax|fmin|fmaxnm|fminnm|fsqrt|frint|fabs|fneg|frecpe|frecps|frecpx|frsqrte|frsqrts|fcmgt|fcmge|fcmeq|fcmle|fcmlt|facge|facgt)/ &&
          (mnem ~ /\.(4|8)h$/ || ops ~ /(^|[^a-z0-9_.])h([0-9]|[12][0-9]|3[01])([^a-z0-9_]|$)/ || ops ~ /\.(4|8)h([^a-z0-9_]|$)/)) {
        print mnem " [fp16]";
        next;
      }
    }
  ' "$disasm" || true)

  if [ -n "$bad" ]; then
    summary=$(printf '%s\n' "$bad" | LC_ALL=C sort | uniq -c | head -n 20)
    fail "Gate 2: forbidden ISA instructions in '$rel' ($ARCH):"$'\n'"$summary"
  fi
done < "$TMP/macho_files"

echo "Gate 2 PASS [$PKG_NAME/$ARCH]: $total_insns instructions scanned across $macho_count Mach-O file(s), 0 forbidden instructions"

# ---------------------------------------------------------------------------
# Gate 3: otool -L / Load-Command Dependency Closure Check
# ---------------------------------------------------------------------------
grep -Ev '^[[:space:]]*(#|$)' "$BASE_DYLIBS" > "$TMP/allowed_deps"
if [ "${#ALLOWED_DEPS[@]}" -gt 0 ]; then
  for dep in "${ALLOWED_DEPS[@]}"; do
    printf '%s\n' "$dep" >> "$TMP/allowed_deps"
  done
fi
cat "$TMP/self_provides" >> "$TMP/allowed_deps"
LC_ALL=C sort -u -o "$TMP/allowed_deps" "$TMP/allowed_deps"

dep_checks=0
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  full="$ROOT/$rel"
  load_cmds="$TMP/load_cmds.txt"
  "$OTOOL" -l "$full" > "$load_cmds"

  # Inspect all dylib load command variants, preserving any embedded spaces.
  LC_ALL=C awk '
    $1 == "Load" && $2 == "command" { in_d = 0; next }
    $1 == "cmd" && ($2 == "LC_LOAD_DYLIB" || $2 == "LC_LOAD_WEAK_DYLIB" || $2 == "LC_REEXPORT_DYLIB" || $2 == "LC_LAZY_LOAD_DYLIB" || $2 == "LC_LOAD_UPWARD_DYLIB") { in_d = 1; next }
    in_d && $1 == "name" {
      line = $0;
      sub(/^[[:space:]]*name[[:space:]]+/, "", line);
      sub(/[[:space:]]+\(offset [0-9]+\)[[:space:]]*$/, "", line);
      print line;
      in_d = 0;
    }
  ' "$load_cmds" > "$TMP/file_deps"

  # Cross-check with otool -L, preserving any embedded spaces in install names.
  hdr_type=$("$OTOOL" -hv "$full" | awk '/^MH_MAGIC_64/ { print $5; exit }')
  skip_lines=2
  [ "$hdr_type" = "DYLIB" ] && skip_lines=3
  "$OTOOL" -L "$full" | tail -n +"$skip_lines" | LC_ALL=C awk '{
    line = $0;
    sub(/^[[:space:]]+/, "", line);
    sub(/[[:space:]]+\(compatibility version .*$/, "", line);
    if (line != "") print line;
  }' >> "$TMP/file_deps"
  LC_ALL=C sort -u -o "$TMP/file_deps" "$TMP/file_deps"

  while IFS= read -r dep; do
    [ -n "$dep" ] || continue
    dep_checks=$((dep_checks + 1))
    case "$dep" in
      /nix/store/*|/opt/homebrew/*|/usr/local/*|@rpath/*|@loader_path/*|@executable_path/*|*" "*)
        fail "Gate 3: '$rel' leaks forbidden dependency '$dep'"
        ;;
    esac
    if ! grep -Fxq "$dep" "$TMP/allowed_deps"; then
      fail "Gate 3: '$rel' links undeclared dependency '$dep' (not in KidoDarwin base image or declared kl deps)"
    fi
  done < "$TMP/file_deps"
done < "$TMP/macho_files"

echo "Gate 3 PASS [$PKG_NAME/$ARCH]: $dep_checks dependency edge(s) verified inside closed base+kl allowlist"
