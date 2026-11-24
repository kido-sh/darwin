# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/libbz2.1.0.dylib from bzip2, under macOS's install name, versions and exports.
{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, bzip2
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
, appleSdk
}:

let
  srcs = [ "blocksort" "huffman" "crctable" "randtable" "compress" "decompress" "bzlib" ];
in
stdenv.mkDerivation {
  pname = "kido-libbz2-dylib";
  inherit (bzip2) version;
  src = bzip2.src;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang -arch ${arch}"
    CFLAGS="-isysroot $SDK -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -O2 -D_FILE_OFFSET_BITS=64"
    NM=${darwinCrossToolchain}/bin/${targetTriple}-nm
    for f in ${lib.concatStringsSep " " srcs}; do
      $CC $CFLAGS -c "$f.c" -o "$f.o"
    done
    $CC -dynamiclib \
      -isysroot "$SDK" \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-exported_symbols_list,${./libbz2.exports} \
      -install_name /usr/lib/libbz2.1.0.dylib \
      -compatibility_version 1.0.0 \
      -current_version 1.0.8 \
      -lSystem \
      ${lib.concatMapStringsSep " " (f: "${f}.o") srcs} \
      -o libbz2.1.0.dylib
    grep -v '^#' ${./libbz2.exports} | sort > want
    $NM -gUj libbz2.1.0.dylib | sort > have
    cmp -s want have || { echo "libbz2 exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib
    cp libbz2.1.0.dylib $out/usr/lib/
    ln -sf libbz2.1.0.dylib $out/usr/lib/libbz2.dylib
    runHook postInstall
  '';

  dontFixup = true;
}
