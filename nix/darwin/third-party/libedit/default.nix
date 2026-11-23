# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/libedit.3.dylib, the readline replacement, under macOS's install name, versions
# and exports, over the /usr/lib/libncurses.5.4.dylib of ./../ncurses-dylib.
{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "arm64-apple-darwin20.4"
, libSystem
, libedit
, ncurses
, appleSdk
}:

let
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  linkFlags = lib.concatStringsSep " " [
    "-nostdlib"
    "-L${libSystem}/usr/lib"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    "-Wl,-platform_version,macos,26.5,26.5"
    "-lSystem"
  ];
in
stdenv.mkDerivation {
  pname = "kido-libedit";
  inherit (libedit) version src;

  configurePhase = ''
    runHook preConfigure
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC=${t}-clang AR=${t}-ar RANLIB=${t}-ranlib STRIP=${t}-strip NM=${t}-nm
    export CFLAGS="-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export CPPFLAGS="-I${ncurses}/include"
    export LDFLAGS="-L${ncurses}/usr/lib ${linkFlags} -Wl,-dylinker_install_name,/usr/lib/dyld"
    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --disable-shared \
      --enable-static \
      --disable-examples
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES -C src
    $CC -dynamiclib -o libedit.3.dylib -Wl,-force_load,src/.libs/libedit.a -lncurses \
      -install_name /usr/lib/libedit.3.dylib -compatibility_version 2 -current_version 3 \
      -Wl,-exported_symbols_list,${./libedit.exports} -L${ncurses}/usr/lib ${linkFlags}
    grep -v '^#' ${./libedit.exports} | sort > want
    $NM -gUj libedit.3.dylib | sort > have
    cmp -s want have || { echo "libedit exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib
    cp libedit.3.dylib $out/usr/lib/
    # the names the SDK links by
    for l in libedit libedit.2 libedit.3.0 libreadline; do
      ln -s libedit.3.dylib $out/usr/lib/$l.dylib
    done
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.unix;
  };
}
