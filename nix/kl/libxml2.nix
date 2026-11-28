# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# kl package recipe for libxml2 (MIT): /usr/lib/libxml2.2.dylib and its
# /usr/lib/libxml2.dylib symlink, linked solely against KidoDarwin base image
# libraries (libSystem, libz, libiconv) with no ICU, lzma, or readline deps.
{ stdenv
, lib
, bash
, meson
, ninja
, pkg-config
, python3
, git
, darwinCrossToolchain
, nativeLd
, libSystem
, libz
, zlibDev
, libiconv
, libxml2
, appleSdk
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
}:

let
  targetInfo = import ../lib/target-info.nix targetTriple;
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  linkFlags = lib.concatStringsSep " " [
    "-isysroot" sdk
    "-fuse-ld=${nativeLd}/bin/ld"
    "-nostdlib"
    "-L${libSystem}/usr/lib"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    "-Wl,-dylib_file,/usr/lib/libz.1.dylib:${libz}/usr/lib/libz.1.dylib"
    "-Wl,-dylib_file,/usr/lib/libiconv.2.dylib:${libiconv}/usr/lib/libiconv.2.dylib"
    "-Wl,-dylib_file,/usr/lib/libcharset.1.dylib:${libiconv}/usr/lib/libcharset.1.dylib"
    "-Wl,-platform_version,macos,26.5,26.5"
    "-Wl,-fixup_chains"
    "-lSystem"
  ];
  klMeta = {
    name = "libxml2";
    version = libxml2.version;
    arch = arch;
    license = "MIT";
    summary = "XML C parser and toolkit library";
    depends = [ ];
    paths = [
      "usr/lib/libxml2.2.dylib"
      "usr/lib/libxml2.dylib"
      "usr/share/doc/kl/libxml2/LICENSE"
    ];
  };
in
stdenv.mkDerivation {
  pname = "kido-kl-libxml2-${arch}";
  inherit (libxml2) version src;

  outputs = [ "out" "dev" ];

  nativeBuildInputs = [ meson ninja pkg-config python3 git ];

  postPatch = ''
    patchShebangs .
  '';

  configurePhase = ''
    runHook preConfigure
    export DARWIN_SDK_ROOT="${sdk}"
    mkdir -p "$TMPDIR/pkgconfig"
    cat > "$TMPDIR/pkgconfig/zlib.pc" <<EOF
prefix=${zlibDev}
libdir=${libz}/usr/lib
includedir=${zlibDev}/include

Name: zlib
Description: zlib compression library
Version: ${libz.version}
Libs: -L${libz}/usr/lib -lz
Cflags: -I${zlibDev}/include
EOF
    export PKG_CONFIG_LIBDIR="$TMPDIR/pkgconfig"
    export PKG_CONFIG_PATH="$TMPDIR/pkgconfig"

    cat > kido-cross.ini <<EOF
[binaries]
c = '${t}-clang'
cpp = '${t}-clang++'
ar = '${t}-ar'
strip = '${t}-strip'
pkg-config = '${pkg-config}/bin/pkg-config'

[built-in options]
c_args = ['-arch', '${arch}', '-isysroot', '${sdk}', '-O2', '-fPIC', '-Qunused-arguments', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-stack-protector', '-I${libSystem}/usr/include', '-I${libiconv.dev}/include', '-I${zlibDev}/include']
c_link_args = ['-arch', '${arch}', '-isysroot', '${sdk}', '-fuse-ld=${nativeLd}/bin/ld', '-nostdlib', '-L${libSystem}/usr/lib', '-L${libz}/usr/lib', '-L${libiconv}/usr/lib', '-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib', '-Wl,-dylib_file,/usr/lib/libz.1.dylib:${libz}/usr/lib/libz.1.dylib', '-Wl,-dylib_file,/usr/lib/libiconv.2.dylib:${libiconv}/usr/lib/libiconv.2.dylib', '-Wl,-dylib_file,/usr/lib/libcharset.1.dylib:${libiconv}/usr/lib/libcharset.1.dylib', '-Wl,-dylinker_install_name,/usr/lib/dyld', '-Wl,-platform_version,macos,26.5,26.5', '-lSystem']

[host_machine]
system = 'darwin'
subsystem = 'macos'
cpu_family = '${targetInfo.mesonCpuFamily}'
cpu = '${targetInfo.mesonCpu}'
endian = '${targetInfo.mesonEndian}'

[properties]
needs_exe_wrapper = true
EOF

    meson setup build \
      --cross-file kido-cross.ini \
      --prefix=/usr \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=static \
      -Diconv=enabled \
      -Dzlib=enabled \
      -Dlegacy=enabled \
      -Ddocs=disabled \
      -Dhttp=disabled \
      -Dtls=disabled \
      -Dicu=disabled \
      -Dpython=disabled \
      -Dhistory=disabled \
      -Dreadline=disabled \
      -Dmodules=disabled
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build libxml2.a

    "${t}-clang" -arch ${arch} -dynamiclib \
      -Wl,-force_load,build/libxml2.a \
      -install_name /usr/lib/libxml2.2.dylib \
      -compatibility_version 10.0.0 \
      -current_version 10.9.0 \
      -L${libz}/usr/lib -lz \
      -L${libiconv}/usr/lib -liconv \
      ${linkFlags} \
      -o libxml2.2.dylib

    # Verify core symbols required by Homebrew LLVM.
    "${t}-nm" -gUj libxml2.2.dylib > exported.syms
    for sym in \
      _xmlAddChild _xmlCopyNamespace _xmlDocDumpFormatMemoryEnc \
      _xmlDocGetRootElement _xmlDocSetRootElement _xmlFreeDoc \
      _xmlFreeNode _xmlFreeNs _xmlNewDoc _xmlNewNs _xmlNewProp \
      _xmlReadMemory _xmlSetGenericErrorFunc _xmlStrdup _xmlUnlinkNode; do
      grep -Fxq "$sym" exported.syms || { echo "missing expected symbol $sym" >&2; exit 1; }
    done
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 libxml2.2.dylib "$out/usr/lib/libxml2.2.dylib"
    ln -s libxml2.2.dylib "$out/usr/lib/libxml2.dylib"
    install -Dm644 Copyright "$out/usr/share/doc/kl/libxml2/LICENSE"

    mkdir -p "$dev/include/libxml2/libxml" "$dev/lib/pkgconfig"
    cp include/libxml/*.h "$dev/include/libxml2/libxml/"
    cp build/include/libxml/xmlversion.h "$dev/include/libxml2/libxml/"
    ln -s "$out/usr/lib/libxml2.2.dylib" "$dev/lib/libxml2.dylib"
    cat > "$dev/lib/pkgconfig/libxml-2.0.pc" <<EOF
prefix=$dev
exec_prefix=\''${prefix}
libdir=\''${exec_prefix}/lib
includedir=\''${prefix}/include/libxml2

Name: libXML
Version: ${libxml2.version}
Description: libXML library version 2
Libs: -L\''${libdir} -lxml2
Cflags: -I\''${includedir}
EOF

    "${bash}/bin/bash" ${./check-gates.sh} \
      --root "$out" \
      --pkg-name libxml2 \
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
    description = "kl system package for libxml2 (${arch})";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
