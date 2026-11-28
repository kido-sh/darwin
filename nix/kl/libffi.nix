# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# kl package recipe for libffi (MIT): /usr/lib/libffi.dylib matching macOS's
# install name and ABI versions (compatibility 1.0.0, current 40.0.0).
{ stdenv
, lib
, bash
, darwinCrossToolchain
, nativeLd
, libSystem
, libffiReal
, appleSdk
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
}:

let
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  linkFlags = lib.concatStringsSep " " [
    "-isysroot" sdk
    "-fuse-ld=${nativeLd}/bin/ld"
    "-nostdlib"
    "-L${libSystem}/usr/lib"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    "-Wl,-platform_version,macos,26.5,26.5"
    "-Wl,-fixup_chains"
    "-lSystem"
  ];
  klMeta = {
    name = "libffi";
    version = libffiReal.version;
    arch = arch;
    license = "MIT";
    summary = "Portable Foreign Function Interface library";
    depends = [ ];
    paths = [
      "usr/lib/libffi.dylib"
      "usr/share/doc/kl/libffi/LICENSE"
    ];
  };
in
stdenv.mkDerivation {
  pname = "kido-kl-libffi-${arch}";
  inherit (libffiReal) version src;

  outputs = [ "out" "dev" ];

  # 1. Upstream sysv.S and ffi.c hardcode BTI hint encodings (#34/#36) for Linux
  #    ELF BTI landing pads. On portable ARMv8.0-A (arm64) replace them with
  #    standard nop while keeping the exact 4-instruction jump-table stride intact.
  # 2. On arm64e, upstream closures.c runs ptrauth_auth_data() on the unsigned
  #    data symbol &ffi_closure_trampoline_table_page, and sysv.S branches to the
  #    PAC-signed ffi_closure_SYSV pointer via plain `br x16` instead of
  #    BRANCH_TO_REG (braaz x16). Fix both so arm64e ctypes closures work cleanly.
  postPatch = ''
    substituteInPlace src/closures.c \
      --replace-fail 'ptrauth_auth_data((void *)&ffi_closure_trampoline_table_page, ptrauth_key_function_pointer, 0)' \
                     'ptrauth_strip((void *)&ffi_closure_trampoline_table_page, ptrauth_key_function_pointer)'
    substituteInPlace src/aarch64/sysv.S \
      --replace-fail $'    ldp x17, x16, [x16]\n    br x16' \
                     $'    ldp x17, x16, [x16]\n    BRANCH_TO_REG x16'
  '' + lib.optionalString (arch == "arm64") ''
    substituteInPlace src/aarch64/sysv.S \
      --replace-fail '#define BTI_C hint #34' '#define BTI_C nop' \
      --replace-fail '#define BTI_J hint #36' '#define BTI_J nop'
    substituteInPlace src/aarch64/ffi.c \
      --replace-fail '#define BTI_J "hint #36"' '#define BTI_J "nop"'
  '';

  configurePhase = ''
    runHook preConfigure
    export DARWIN_SDK_ROOT="${sdk}"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    mkdir -p "$TMPDIR/ccwrap"
    cat > "$TMPDIR/ccwrap/cc" <<'EOF'
#!${bash}/bin/bash
exec "${t}-clang" -arch ${arch} -fuse-ld="${nativeLd}/bin/ld" -Qunused-arguments "$@"
EOF
    chmod +x "$TMPDIR/ccwrap/cc"
    export CC="$TMPDIR/ccwrap/cc"
    export AR="${t}-ar"
    export RANLIB="${t}-ranlib"
    export NM="${t}-nm"
    export STRIP="${t}-strip"
    export CFLAGS="-isysroot ${sdk} -O2 -fPIC -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include"
    export LDFLAGS="${linkFlags} -Wl,-dylinker_install_name,/usr/lib/dyld"

    ./configure \
      --host=${targetTriple} \
      --build=${stdenv.buildPlatform.config} \
      --prefix=/usr \
      --disable-shared \
      --enable-static \
      --disable-docs \
      --disable-multi-os-directory
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j"$NIX_BUILD_CORES"
    libffi_a=$(find . -path '*/.libs/libffi.a' -type f | head -n 1)
    [ -f "$libffi_a" ] || { echo "libffi.a not found" >&2; exit 1; }

    "${t}-clang" -arch ${arch} -dynamiclib \
      -Wl,-force_load,"$libffi_a" \
      -install_name /usr/lib/libffi.dylib \
      -compatibility_version 1.0.0 \
      -current_version 40.0.0 \
      ${linkFlags} \
      -o libffi.dylib

    # Verify core symbols required by Homebrew LLVM and Python ctypes.
    "${t}-nm" -gUj libffi.dylib > exported.syms
    for sym in \
      _ffi_call _ffi_prep_cif _ffi_prep_cif_var \
      _ffi_closure_alloc _ffi_closure_free _ffi_prep_closure_loc \
      _ffi_type_void _ffi_type_sint8 _ffi_type_sint16 _ffi_type_sint32 _ffi_type_sint64 \
      _ffi_type_uint8 _ffi_type_uint16 _ffi_type_uint32 _ffi_type_uint64 \
      _ffi_type_float _ffi_type_double _ffi_type_pointer; do
      grep -Fxq "$sym" exported.syms || { echo "missing expected symbol $sym" >&2; exit 1; }
    done
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install DESTDIR="$TMPDIR/stage"

    install -Dm755 libffi.dylib "$out/usr/lib/libffi.dylib"
    install -Dm644 LICENSE "$out/usr/share/doc/kl/libffi/LICENSE"

    mkdir -p "$dev/include" "$dev/lib/pkgconfig"
    cp "$TMPDIR/stage/usr/include/"*.h "$dev/include/"
    ln -s "$out/usr/lib/libffi.dylib" "$dev/lib/libffi.dylib"
    cat > "$dev/lib/pkgconfig/libffi.pc" <<EOF
prefix=$dev
exec_prefix=\''${prefix}
libdir=\''${exec_prefix}/lib
includedir=\''${prefix}/include

Name: libffi
Description: Portable Foreign Function Interface library
Version: ${libffiReal.version}
Libs: -L\''${libdir} -lffi
Cflags: -I\''${includedir}
EOF

    "${bash}/bin/bash" ${./check-gates.sh} \
      --root "$out" \
      --pkg-name libffi \
      --arch "${arch}" \
      --license "${klMeta.license}" \
      --base-dylibs ${./base-dylibs.txt} \
      --otool "${t}-otool" \
      --objdump "${t}-objdump"
    runHook postInstall
  '';

  dontFixup = true;

  passthru = {
    kl = klMeta;
  };

  meta = with lib; {
    description = "kl system package for libffi (${arch})";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
