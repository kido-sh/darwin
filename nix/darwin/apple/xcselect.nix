# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
#
# Clean-room /usr/lib/libxcselect.dylib implementing macOS's 12 xcselect symbols.
{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
, nativeLd
, libSystem
, src
, appleSdk
}:

let
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
in
stdenv.mkDerivation {
  pname = "kido-libxcselect";
  version = "1.0.0";

  inherit src;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CFLAGS="-arch ${arch} -std=c11 -fblocks -fPIC -Os -Wall -Wextra -Werror \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -Isrc/Libraries/xcselect/include -Isrc/Libraries/xcselect"

    ${t}-clang $CFLAGS -c src/Libraries/xcselect/xcselect.c -o xcselect.o

    ${t}-clang -arch ${arch} -dynamiclib -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-fixup_chains \
      -Wl,-install_name,/usr/lib/libxcselect.dylib \
      -Wl,-compatibility_version,1.0.0 \
      -Wl,-current_version,1.0.0 \
      -Wl,-exported_symbols_list,src/Libraries/xcselect/xcselect.exports \
      xcselect.o -lSystem -o libxcselect.dylib

    grep -Ev '^(#|$)' src/Libraries/xcselect/xcselect.exports | sort > want
    ${t}-nm -gUj libxcselect.dylib | sort > have
    cmp -s want have || { echo "libxcselect exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out/usr/lib" "$out/usr/include"
    cp libxcselect.dylib "$out/usr/lib/"
    cp src/Libraries/xcselect/include/xcselect.h "$out/usr/include/"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Clean-room Apache-2.0 implementation of /usr/lib/libxcselect.dylib";
    platforms = platforms.unix;
  };
}
