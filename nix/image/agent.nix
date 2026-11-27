# kido's guest agent, built from the kido input's native Rust implementation (agent-rs/) for
# aarch64-apple-darwin with -Z build-std targeting ARMv8.0-A (apple-a10 +aes,+neon), and the root
# tree that installs it with its launchd job.
{ lib
, stdenv
, rustc
, cargo
, rustPlatform
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
    name = "rustc-sysroot-with-libsrc";
    paths = [ rustc.unwrapped ];
    postBuild = ''
      mkdir -p "$out/lib/rustlib/src/rust"
      ln -s "${rustPlatform.rustLibSrc}" "$out/lib/rustlib/src/rust/library"
    '';
  };
  rustcWithLibSrc = rustc.override { inherit sysroot; };

  # Vendored Cargo crates for agent-rs/Cargo.lock (fixed-output hash). Exposed via passthru so
  # notices.nix collects only agent-rs's crate licenses.
  cargoDeps = rustPlatform.fetchCargoVendor {
    name = "kido-agent-${version}";
    src = "${kido}/agent-rs";
    hash = "sha256-oTTpPymghE82FOq/tR3aUvuIUPHfftdaLg6QKo+z2BA=";
  };

  sdkRoot = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";

  agent = stdenv.mkDerivation {
    pname = "kido-agent";
    inherit version;
    src = "${kido}/agent-rs";

    nativeBuildInputs = [
      rustcWithLibSrc
      cargo
      darwinCrossToolchain
    ];

    env = {
      RUSTC_BOOTSTRAP = "1";
      DARWIN_SDK_ROOT = sdkRoot;
      CC_aarch64_apple_darwin = "${darwinCrossToolchain}/bin/arm64-apple-darwin20.4-clang";
      AR_aarch64_apple_darwin = "${darwinCrossToolchain}/bin/arm64-apple-darwin20.4-ar";
    };

    configurePhase = ''
      runHook preConfigure

      export HOME="$TMPDIR"
      vendor="$TMPDIR/vendor"
      mkdir -p "$vendor" .cargo

      # Merge agent-rs's vendored crates with rustc's vendored std dependencies for offline
      # `-Z build-std`. Agent crates take precedence on name-version collisions.
      for d in "${cargoDeps}"/source-registry-0/*; do
        [ -e "$d" ] || continue
        ln -s "$d" "$vendor/$(basename "$d")"
      done
      for d in "${rustPlatform.rustVendorSrc}"/*; do
        [ -e "$d" ] || continue
        base="$(basename "$d")"
        [ -e "$vendor/$base" ] || ln -s "$d" "$vendor/$base"
      done

      # Rust's libc crate unconditionally emits `#[link(name = "iconv")]` on macOS,
      # which appleSdk omits (and kido-agent never calls, and -dead_strip_dylibs would
      # drop it anyway). Filter `-liconv` (and `-lc`/`-lm`, which Darwin provides via
      # `-lSystem`) and link against KidoDarwin's real libSystem.B.dylib + re-exported
      # libdyld.dylib so TLV (`_tlv_bootstrap`/`_tlv_atexit`) and dyld symbols resolve
      # and every imported symbol is verified against the guest's real libSystem.
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
        "--cfg", "aes_armv8",
        "--cfg", "polyval_armv8",
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
        --target aarch64-apple-darwin \
        -Z build-std=core,alloc,std,panic_abort \
        -j "$NIX_BUILD_CORES"
      runHook postBuild
    '';

    # darwin binaries do not run on the Linux build host
    doCheck = false;

    installPhase = ''
      runHook preInstall
      install -Dm755 target/aarch64-apple-darwin/release/kido-agent "$out/bin/kido-agent"

      # Mandatory linkage gate: must link ONLY /usr/lib/libSystem.B.dylib.
      deps=$(arm64-apple-darwin20.4-otool -L "$out/bin/kido-agent" \
        | tail -n +2 | awk '{ print $1 }' | LC_ALL=C sort)
      if [ "$deps" != "/usr/lib/libSystem.B.dylib" ]; then
        echo "ERROR: unexpected dynamic library dependencies in kido-agent:" >&2
        arm64-apple-darwin20.4-otool -L "$out/bin/kido-agent" >&2
        exit 1
      fi

      # Mandatory ISA gate: ZERO ARMv8.1+/v8.3+/v8.4+ instructions (LSE, LRCPC/LRCPC2, FLAGM, SB, PAC).
      # Uses llvm-objdump (arm64-apple-darwin20.4-objdump) because host cctools otool is built
      # without LLVM disassembler support on Linux.
      disasm="$TMPDIR/kido-agent.disasm"
      arm64-apple-darwin20.4-objdump -d --no-show-raw-insn "$out/bin/kido-agent" > "$disasm"
      insn_count=$(awk '/^[0-9a-f]+:/ { n++ } END { print n+0 }' "$disasm")
      if [ "$insn_count" -lt 1000 ]; then
        echo "ERROR: disassembly unexpectedly small ($insn_count instructions)" >&2
        exit 1
      fi
      bad_insns=$(awk '/^[0-9a-f]+:/ { print $2 }' "$disasm" \
        | grep -iE '^(ldadd|stadd|swp|cas|ldclr|stclr|ldset|stset|ldeor|steor|ldapr|ldapur|stlur|cfinv|setf8|setf16|rmif|sb$|pac|aut|xpac)' || true)
      if [ -n "$bad_insns" ]; then
        echo "ERROR: forbidden ARMv8.1+/v8.3+/v8.4+ instructions found in kido-agent:" >&2
        echo "$bad_insns" | sort | uniq -c >&2
        exit 1
      fi
      echo "ISA gate passed: $insn_count instructions disassembled, 0 forbidden instructions"

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
  inherit agent;
  root = runCommand "kido-agent-root-${agent.version}" { } ''
    install -Dm755 ${agent}/bin/kido-agent $out/usr/libexec/kido-agent
    mkdir -p $out/System/Library/LaunchDaemons
    cat > $out/System/Library/LaunchDaemons/sh.kido.agent.plist <<'EOF'
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0">
    <dict>
    	<key>Label</key>
    	<string>sh.kido.agent</string>
    	<key>ProgramArguments</key>
    	<array>
    		<string>/usr/libexec/kido-agent</string>
    	</array>
    	<key>RunAtLoad</key>
    	<true/>
    	<key>KeepAlive</key>
    	<true/>
    	<key>StandardOutPath</key>
    	<string>/var/log/kido-agent.log</string>
    	<key>StandardErrorPath</key>
    	<string>/var/log/kido-agent.log</string>
    </dict>
    </plist>
    EOF
    chmod 644 $out/System/Library/LaunchDaemons/sh.kido.agent.plist
  '';
}
