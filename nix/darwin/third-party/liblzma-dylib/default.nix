# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/liblzma.5.dylib from xz, under macOS's install name, versions and exports.
{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, xz
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
, appleSdk
}:

let
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  linkFlags = lib.concatStringsSep " " [
    "-fuse-ld=${nativeLd}/bin/ld"
    "-nostdlib"
    "-L${libSystem}/usr/lib"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    "-Wl,-platform_version,macos,26.5,26.5"
    "-lSystem"
  ];
in
stdenv.mkDerivation {
  pname = "kido-liblzma-dylib";
  inherit (xz) version;
  src = xz.src;

  configurePhase = ''
    runHook preConfigure
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export CC="${t}-clang -arch ${arch}" AR=${t}-ar RANLIB=${t}-ranlib NM=${t}-nm STRIP=${t}-strip
    export CFLAGS="-isysroot $SDK -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector"
    export LDFLAGS="-isysroot $SDK ${linkFlags} -Wl,-dylinker_install_name,/usr/lib/dyld"
    ./configure --host=${targetTriple} --build=${stdenv.buildPlatform.config} \
      --disable-shared --enable-static --disable-xz --disable-xzdec --disable-lzmadec \
      --disable-lzmainfo --disable-lzma-links --disable-scripts --disable-doc --disable-nls
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -C src/liblzma -j$NIX_BUILD_CORES liblzma.la
    $CC -dynamiclib -o liblzma.5.dylib -Wl,-force_load,src/liblzma/.libs/liblzma.a \
      -install_name /usr/lib/liblzma.5.dylib -compatibility_version 6.0.0 -current_version 6.3.0 \
      -Wl,-exported_symbols_list,${./liblzma.exports} -isysroot "$SDK" ${linkFlags}
    grep -v '^#' ${./liblzma.exports} | sort > want
    $NM -gUj liblzma.5.dylib | sort > have
    cmp -s want have || { echo "liblzma exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib
    cp liblzma.5.dylib $out/usr/lib/
    ln -sf liblzma.5.dylib $out/usr/lib/liblzma.dylib
    runHook postInstall
  '';

  dontFixup = true;
}
