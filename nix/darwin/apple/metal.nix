# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0
{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "arm64-apple-darwin20.4"
, nativeLd
, libSystem
, libobjc
, corefoundation
, foundation
, iokit
, src
, appleSdk
}:

let
  cc = "${darwinCrossToolchain}/bin/${targetTriple}-clang";
  mSrcs = [
    "KidoMetalDescriptors"
    "KidoMetalDevice"
    "KidoMetalCommand"
  ];
in
stdenv.mkDerivation {
  pname = "kido-metal";
  version = "0.1";

  inherit src;

  buildPhase = ''
    runHook preBuild

    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    mkdir -p cf-headers iokit-headers
    ln -s ${corefoundation}/include cf-headers/CoreFoundation
    ln -s ${iokit}/System/Library/Frameworks/IOKit.framework/Versions/A/Headers iokit-headers/IOKit

    CFLAGS="-x objective-c -fno-objc-arc -fblocks -fPIC -Os -DNDEBUG -D__PUREDARWIN__=1 \
      -Wall -Wextra -Wno-unused-parameter \
      -isysroot $DARWIN_SDK_ROOT \
      -Isrc/Frameworks/Metal \
      -I${foundation}/usr/include \
      -Icf-headers \
      -Iiokit-headers \
      -I${libobjc}/usr/include \
      -I${libSystem}/usr/include \
      -I${corefoundation}/include"

    objs=""
    for s in ${lib.concatStringsSep " " mSrcs}; do
      ${cc} $CFLAGS -c "src/Frameworks/Metal/$s.m" -o "$s.o"
      objs="$objs $s.o"
    done

    ${cc} -isysroot "$DARWIN_SDK_ROOT" -dynamiclib \
      -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libobjc}/usr/lib -L${corefoundation}/usr/lib \
      -L${foundation}/usr/lib -L${iokit}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-compatibility_version,1.0.0 \
      -Wl,-current_version,400.0.0 \
      -Wl,-install_name,/System/Library/Frameworks/Metal.framework/Versions/A/Metal \
      -Wl,-fixup_chains \
      -lobjc -lFoundation -lCoreFoundation -lIOKitCF -lSystem \
      -o Metal $objs

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    fwdir=$out/System/Library/Frameworks/Metal.framework
    mkdir -p "$fwdir/Versions/A/Resources"
    cp Metal "$fwdir/Versions/A/Metal"
    cp src/Frameworks/Metal/Info.plist "$fwdir/Versions/A/Resources/Info.plist"
    ln -sf A "$fwdir/Versions/Current"
    ln -sf Versions/Current/Metal "$fwdir/Metal"
    ln -sf Versions/Current/Resources "$fwdir/Resources"

    mkdir -p "$out/usr/lib"
    ln -sf ../../System/Library/Frameworks/Metal.framework/Versions/A/Metal "$out/usr/lib/libMetal.dylib"
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Open-source Metal.framework backed by KidoDarwin virtio-metal GPU passthrough";
    platforms = platforms.unix;
  };
}
