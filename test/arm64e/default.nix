# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# t_pac.c built for arm64e and arm64 by the image's LLVM toolchain, for run.sh's LLVM_TESTS_DIR.
{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, targetTriple ? "arm64-apple-darwin20.4"
, appleSdk
}:

stdenv.mkDerivation {
  pname = "kido-arm64e-tests";
  version = "1";
  src = lib.fileset.toSource {
    root = ./..;
    fileset = lib.fileset.unions [ ./t_pac.c ../libsystem/check.h ];
  };

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    for arch in arm64e arm64; do
      ${darwinCrossToolchain}/bin/${targetTriple}-clang -arch $arch -isysroot "$SDK" -O1 \
        -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
        -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib \
        -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
        -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 \
        -Wl,-fixup_chains -lSystem -o t_pac-$arch arm64e/t_pac.c
    done
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp t_pac-arm64e t_pac-arm64 $out/
    runHook postInstall
  '';

  dontFixup = true;
}
