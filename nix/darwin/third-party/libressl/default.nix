# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's LibreSSL 3.3.6: /usr/lib/libcrypto.46.dylib, libssl.48 and libtls.20 under macOS's install
# names, versions and exports, /usr/bin/openssl and /usr/bin/ocspcheck.
{ stdenv
, lib
, fetchurl
, darwinCrossToolchain
, nativeLd
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
, libSystem
, appleSdk
}:

let
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  linkFlags = lib.concatStringsSep " " [
    "-nostdlib"
    "-L${libSystem}/usr/lib"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    "-Wl,-platform_version,macos,26.5,26.5"
    "-Wl,-fixup_chains"
    "-lSystem"
  ];
in
stdenv.mkDerivation rec {
  pname = "kido-libressl";
  version = "3.3.6";

  src = fetchurl {
    url = "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-${version}.tar.gz";
    hash = "sha256-PyiEk2XhGQ2yuvkBT/loYBLCWxym34s6CF94niT+S5o=";
  };

  outputs = [ "out" "dev" ];

  postPatch = ''
    substituteInPlace include/openssl/opensslfeatures.h \
      --replace-fail '/* #define OPENSSL_NO_IDEA */' '#define OPENSSL_NO_IDEA'
    substituteInPlace include/openssl/idea.h \
      --replace-fail $'#ifdef OPENSSL_NO_IDEA\n#error IDEA is disabled.\n#endif\n' ""
  '';

  configurePhase = ''
    runHook preConfigure
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${t}-clang -arch ${arch}" AR=${t}-ar RANLIB=${t}-ranlib STRIP=${t}-strip NM=${t}-nm
    export CFLAGS="-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="${linkFlags} -Wl,-dylinker_install_name,/usr/lib/dyld"
    export ac_cv_func_explicit_bzero=no ac_cv_func_freezero=no ac_cv_func_reallocarray=no \
      ac_cv_func_recallocarray=no ac_cv_func_timingsafe_memcmp=no
    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --with-openssldir=/private/etc/ssl \
      --disable-shared \
      --enable-static \
      --disable-asm \
      --disable-tests
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    for d in crypto ssl tls; do
      make -j$NIX_BUILD_CORES -C $d
    done

    $CC -dynamiclib -o libcrypto.46.dylib -Wl,-force_load,crypto/.libs/libcrypto.a \
      -install_name /usr/lib/libcrypto.46.dylib -compatibility_version 47 -current_version 47.2 \
      -Wl,-exported_symbols_list,${./libcrypto.exports} \
      -Wl,-alias,_SHA256_Init,_SHA256_Init_legacy -Wl,-alias,_SHA256_Transform,_SHA256_Transform_legacy \
      ${linkFlags}
    $CC -dynamiclib -o libssl.48.dylib -Wl,-force_load,ssl/.libs/libssl.a libcrypto.46.dylib \
      -install_name /usr/lib/libssl.48.dylib -compatibility_version 49 -current_version 49.2 \
      -Wl,-exported_symbols_list,${./libssl.exports} ${linkFlags}
    mkdir tls-objects
    (cd tls-objects && $AR x ../tls/.libs/libtls.a $($AR t ../tls/.libs/libtls.a | grep '^libtls_la-'))
    $CC -dynamiclib -o libtls.20.dylib tls-objects/*.o libssl.48.dylib libcrypto.46.dylib \
      -install_name /usr/lib/libtls.20.dylib -compatibility_version 21 -current_version 21.3 \
      -Wl,-exported_symbols_list,${./libtls.exports} ${linkFlags}

    for l in libcrypto.46:${./libcrypto.exports} libssl.48:${./libssl.exports} libtls.20:${./libtls.exports}; do
      grep -v '^#' ''${l#*:} | sort > want
      $NM -gUj ''${l%%:*}.dylib | sort > have
      cmp -s want have || { echo "''${l%%:*} exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    done

    make -j$NIX_BUILD_CORES -C apps/openssl openssl \
      openssl_LDADD="$PWD/libssl.48.dylib $PWD/libcrypto.46.dylib"
    make -j$NIX_BUILD_CORES -C apps/ocspcheck ocspcheck \
      ocspcheck_LDADD="$PWD/libtls.20.dylib $PWD/libssl.48.dylib $PWD/libcrypto.46.dylib"
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $out/usr/bin $out/etc/ssl $dev/include/openssl $dev/lib
    for l in libcrypto.46 libssl.48 libtls.20; do
      cp $l.dylib $out/usr/lib/
      ln -s $out/usr/lib/$l.dylib $dev/lib/''${l%.*}.dylib
    done
    cp apps/openssl/openssl apps/ocspcheck/ocspcheck $out/usr/bin/
    cp openssl.cnf x509v3.cnf $out/etc/ssl/
    cp include/openssl/*.h $dev/include/openssl/
    cp include/tls.h $dev/include/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.unix;
  };
}
