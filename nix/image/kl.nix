# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# kl, KidoDarwin's system package manager, built from the kido input's Rust workspace
# (crates/kl with --no-default-features) for aarch64-apple-darwin with -Z build-std
# targeting ARMv8.0-A (apple-a10 +aes,+neon), and the root tree installing /usr/bin/kl.
{ lib
, stdenv
, rustc
, cargo
, rustPlatform
, python3
, symlinkJoin
, runCommand
, darwinCrossToolchain
, appleSdk
, libSystem
, kido
}:

let
  version = kido.shortRev or "dev";

  # Provide rustlib/src/rust/library in rustc's sysroot so `-Z build-std` works purely offline
  # without touching the network.
  sysroot = symlinkJoin {
    name = "rustc-sysroot-with-libsrc-kl";
    paths = [ rustc.unwrapped ];
    postBuild = ''
      mkdir -p "$out/lib/rustlib/src/rust"
      ln -s "${rustPlatform.rustLibSrc}" "$out/lib/rustlib/src/rust/library"
    '';
  };
  rustcWithLibSrc = rustc.override { inherit sysroot; };

  klSrc = runCommand "kido-kl-src-${version}" { } ''
    mkdir -p "$out/crates"
    cp ${kido}/Cargo.toml ${kido}/Cargo.lock "$out/"
    cp -r ${kido}/crates/kido-trust ${kido}/crates/kl "$out/crates/"
  '';

  # Vendored Cargo crates for kido's workspace Cargo.lock (fixed-output hash). Exposed via
  # passthru so notices.nix and sbom.nix collect exact crate licenses.
  cargoDeps = rustPlatform.fetchCargoVendor {
    name = "kido-kl-${version}";
    src = klSrc;
    hash = "sha256-gwxLcAP9e4IW2UNEG06nzpAzSI0VSXkQ8BTdaxj6d+w=";
  };

  sdkRoot = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";

  kl = stdenv.mkDerivation {
    pname = "kido-kl";
    inherit version;
    src = klSrc;

    nativeBuildInputs = [
      rustcWithLibSrc
      cargo
      python3
      darwinCrossToolchain
    ];

    env = {
      RUSTC_BOOTSTRAP = "1";
      DARWIN_SDK_ROOT = sdkRoot;
      CC_aarch64_apple_darwin = "${darwinCrossToolchain}/bin/arm64-apple-darwin20.4-clang";
      AR_aarch64_apple_darwin = "${darwinCrossToolchain}/bin/arm64-apple-darwin20.4-ar";
      CFLAGS_aarch64_apple_darwin = "-mcpu=apple-a10";
    };

    configurePhase = ''
      runHook preConfigure

      export HOME="$TMPDIR"
      vendor="$TMPDIR/vendor"
      mkdir -p "$vendor" .cargo

      # Merge workspace vendored crates with rustc's vendored std dependencies for offline
      # `-Z build-std`. Workspace crates take precedence on name-version collisions.
      for d in "${cargoDeps}"/source-registry-0/*; do
        [ -e "$d" ] || continue
        ln -s "$d" "$vendor/$(basename "$d")"
      done
      for d in "${rustPlatform.rustVendorSrc}"/*; do
        [ -e "$d" ] || continue
        base="$(basename "$d")"
        [ -e "$vendor/$base" ] || ln -s "$d" "$vendor/$base"
      done

      cat > "$TMPDIR/darwin-linker" <<'LINKER_EOF'
      #!${stdenv.shell}
      out_args=()
      for a in "$@"; do
        case "$a" in
          -liconv|-lc|-lm) continue ;;
        esac
        out_args+=("$a")
      done
      exec "${darwinCrossToolchain}/bin/arm64-apple-darwin20.4-clang" \
        -L"${libSystem}/usr/lib" \
        -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:"${libSystem}/usr/lib/system/libdyld.dylib" \
        -Wl,-dylinker_install_name,/usr/lib/dyld \
        "''${out_args[@]}"
      LINKER_EOF
      chmod +x "$TMPDIR/darwin-linker"

      cat > .cargo/config.toml <<EOF
      [source.crates-io]
      replace-with = "vendored-sources"

      [source.vendored-sources]
      directory = "$vendor"

      [target.aarch64-apple-darwin]
      linker = "$TMPDIR/darwin-linker"
      ar = "${darwinCrossToolchain}/bin/arm64-apple-darwin20.4-ar"
      rustflags = [
        "-C", "target-cpu=apple-a10",
        "-C", "target-feature=+aes,+neon",
        "-C", "link-arg=-Wl,-dead_strip_dylibs",
        "-C", "link-arg=-Wl,-x",
      ]
      EOF

      runHook postConfigure
    '';

    buildPhase = ''
      runHook preBuild
      export HOME="$TMPDIR"
      cargo build \
        --release \
        --offline \
        --locked \
        --no-default-features \
        --package kido-kl \
        --bin kl \
        --target aarch64-apple-darwin \
        -Z build-std=core,alloc,std,panic_abort \
        -j "$NIX_BUILD_CORES"

      # Record exact external crates resolved for aarch64-apple-darwin without publisher or dev deps.
      cargo metadata \
        --offline \
        --locked \
        --format-version 1 \
        --no-default-features \
        --filter-platform aarch64-apple-darwin \
        --manifest-path crates/kl/Cargo.toml \
        > "$TMPDIR/kl-metadata.json"

      python3 - "$TMPDIR/kl-metadata.json" "$TMPDIR/kl-crates.json" <<'PY'
      import json, sys

      meta = json.load(open(sys.argv[1], encoding="utf-8"))
      pkgs = {p["id"]: p for p in meta["packages"]}
      nodes = {n["id"]: n for n in meta["resolve"]["nodes"]}
      root_id = meta["resolve"]["root"]

      visited = set()
      stack = [root_id]
      while stack:
          cur = stack.pop()
          if cur in visited:
              continue
          visited.add(cur)
          for dep in nodes[cur].get("deps", []):
              kinds = dep.get("dep_kinds", [])
              # Include normal (kind is None) and build dependencies; skip dev-only dependencies.
              if not kinds or any(k.get("kind") in (None, "build") for k in kinds):
                  stack.append(dep["pkg"])

      crates = []
      for pid in sorted(visited):
          p = pkgs[pid]
          if not p.get("source"):
              continue
          is_pm = any("proc-macro" in t.get("kind", []) for t in p.get("targets", []))
          crates.append({
              "name": p["name"],
              "version": p["version"],
              "dir": f"{p['name']}-{p['version']}",
              "is_proc_macro": is_pm,
          })
      crates.sort(key=lambda c: (c["name"], c["version"]))
      with open(sys.argv[2], "w", encoding="utf-8") as fh:
          json.dump(crates, fh, indent=2)
          fh.write("\n")
      PY
      runHook postBuild
    '';

    # darwin binaries do not run on the Linux build host
    doCheck = false;

    installPhase = ''
      runHook preInstall
      install -Dm755 target/aarch64-apple-darwin/release/kl "$out/bin/kl"
      install -Dm644 "$TMPDIR/kl-crates.json" "$out/share/kl-crates.json"

      # Mandatory linkage gate: must link ONLY /usr/lib/libSystem.B.dylib.
      deps=$(arm64-apple-darwin20.4-otool -L "$out/bin/kl" \
        | tail -n +2 | awk '{ print $1 }' | LC_ALL=C sort)
      if [ "$deps" != "/usr/lib/libSystem.B.dylib" ]; then
        echo "ERROR: unexpected dynamic library dependencies in kl:" >&2
        arm64-apple-darwin20.4-otool -L "$out/bin/kl" >&2
        exit 1
      fi

      # Mandatory ISA gate: ZERO ARMv8.1+/v8.3+/v8.4+ instructions (LSE, LRCPC/LRCPC2, FLAGM, SB, PAC).
      # Ignore non-code ASCII "for ARMv8, CRYPTOGAMS" literals placed in __TEXT,__text by ring's
      # OpenSSL assembly (where " CRY" = 0x59524320 coincidentally decodes as ldapurh).
      disasm="$TMPDIR/kl.disasm"
      arm64-apple-darwin20.4-objdump -d "$out/bin/kl" > "$disasm"
      python3 - "$disasm" <<'PY'
      import re, sys

      disasm_path = sys.argv[1]
      line_re = re.compile(r"^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(\S+)")
      bad_re = re.compile(
          r"^(ldadd|stadd|swp|cas|ldclr|stclr|ldset|stset|ldeor|steor|"
          r"ldapr|ldapur|stlur|cfinv|setf8|setf16|rmif|sb$|pac|aut|xpac)",
          re.IGNORECASE,
      )

      entries = []
      with open(disasm_path, encoding="utf-8", errors="replace") as fh:
          for line in fh:
              m = line_re.match(line)
              if m:
                  entries.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3)))

      if len(entries) < 1000:
          sys.exit(f"ERROR: disassembly unexpectedly small ({len(entries)} instructions)")

      bad = []
      ignored_ascii = 0
      for idx, (addr, word, mnem) in enumerate(entries):
          if not bad_re.match(mnem):
              continue
          # Reconstruct surrounding little-endian bytes to verify exact CRYPTOGAMS .asciz literal.
          lo = max(0, idx - 8)
          hi = min(len(entries), idx + 8)
          window = b"".join(w.to_bytes(4, "little") for _, w, _ in entries[lo:hi])
          if word == 0x59524320 and b"for ARMv8, CRYPTOGAMS" in window:
              ignored_ascii += 1
              continue
          bad.append((addr, word, mnem))

      if bad:
          print("ERROR: forbidden ARMv8.1+/v8.3+/v8.4+ instructions found in kl:", file=sys.stderr)
          for addr, word, mnem in bad[:50]:
              print(f"  0x{addr:x}: {word:08x} {mnem}", file=sys.stderr)
          sys.exit(1)

      print(
          f"ISA gate passed: {len(entries)} words scanned, 0 forbidden instructions "
          f"({ignored_ascii} verified CRYPTOGAMS ASCII literals skipped)"
      )
      PY

      runHook postInstall
    '';

    # a Mach-O binary: nothing for the ELF fixup hooks to do
    dontFixup = true;

    passthru = {
      inherit cargoDeps;
    };
  };
in
{
  inherit kl;
  root = runCommand "kido-kl-root-${kl.version}" { } ''
    install -Dm755 ${kl}/bin/kl $out/usr/bin/kl
  '';
}
