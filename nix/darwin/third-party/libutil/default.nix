# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/libutil.dylib and its exports: gpt's NetBSD humanize_number(3) and libutil.c.
{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, libSystem
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
, appleSdk
}:

stdenv.mkDerivation {
  pname = "kido-libutil";
  version = "1.0";
  src = lib.fileset.toSource {
    root = ../../../../darwin/src/Userspace/gpt/compat;
    fileset = lib.fileset.unions [
      ../../../../darwin/src/Userspace/gpt/compat/humanize_number.c
      ../../../../darwin/src/Userspace/gpt/compat/namespace.h
    ];
  };

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CC="${darwinCrossToolchain}/bin/${targetTriple}-clang -arch ${arch}"
    NM=${darwinCrossToolchain}/bin/${targetTriple}-nm
    CFLAGS="-isysroot $SDK -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -O2"
    $CC $CFLAGS -I. "-D__RCSID(x)=" "-D_DIAGASSERT(e)=((void)0)" -DHN_DECIMAL=0x01 -DHN_NOSPACE=0x02 \
      -DHN_B=0x04 -DHN_DIVISOR_1000=0x08 -DHN_GETSCALE=0x10 -DHN_AUTOSCALE=0x20 \
      -c humanize_number.c -o humanize_number.o
    $CC $CFLAGS -c ${./libutil.c} -o libutil.o
    $CC -dynamiclib \
      -isysroot "$SDK" \
      -fuse-ld=${nativeLd}/bin/ld \
      -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-exported_symbols_list,${./libutil.exports} \
      -install_name /usr/lib/libutil.dylib \
      -compatibility_version 1.0.0 \
      -current_version 1.0.0 \
      -lSystem \
      humanize_number.o libutil.o \
      -o libutil.dylib
    grep -v '^#' ${./libutil.exports} | sort > want
    $NM -gUj libutil.dylib | sort > have
    cmp -s want have || { echo "libutil exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib
    cp libutil.dylib $out/usr/lib/
    runHook postInstall
  '';

  dontFixup = true;
}
