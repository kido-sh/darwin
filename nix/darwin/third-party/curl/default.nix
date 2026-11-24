# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/libcurl.4.dylib under macOS's install name, versions and exports. TLS is the
# image's LibreSSL, its libssl.48 and libcrypto.46 weak-linked as on macOS, verifying against
# /etc/ssl/cert.pem. HTTP/2 through libapple_nghttp2, linked as on macOS. No GSS-API or Kerberos,
# LDAP or HTTP/3.
{ stdenv
, lib
, callPackage
, fetchFromGitHub
, autoreconfHook
, darwinCrossToolchain
, nativeLd
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
, libSystem
, libressl
, nghttp2
, systemConfiguration
, appleSdk
}:

let
  pin = lib.importJSON ./source.json;
  dylib = callPackage ../dylib.nix { };
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
  pname = "kido-curl";
  inherit (pin) version;

  src = fetchFromGitHub {
    owner = "curl";
    repo = "curl";
    name = "curl-${pin.version}-source";
    tag = "curl-${lib.replaceStrings [ "." ] [ "_" ] pin.version}";
    inherit (pin) hash;
  };

  patches = [ ./libressl-tls13.patch ];
  postPatch = ''
    substituteInPlace include/curl/curlver.h --replace-fail '"${pin.version}-DEV"' '"${pin.version}"'
  '';
  nativeBuildInputs = [ autoreconfHook ];

  configurePhase = ''
    runHook preConfigure
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC="${t}-clang -arch ${arch}" AR=${t}-ar RANLIB=${t}-ranlib STRIP=${t}-strip NM=${t}-nm
    export CFLAGS="-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    # <SystemConfiguration/SCDynamicStoreCopySpecific.h>, which the SDK carries no headers for
    export CPPFLAGS="-I${systemConfiguration}/include"
    export LDFLAGS="${linkFlags} -Wl,-dylinker_install_name,/usr/lib/dyld"
    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --disable-shared \
      --enable-static \
      --disable-symbol-hiding \
      --with-openssl=${libressl.dev} \
      --with-ca-bundle=/etc/ssl/cert.pem \
      --without-ca-path \
      --without-ca-fallback \
      --with-zlib \
      --enable-ipv6 \
      --enable-threaded-resolver \
      --disable-ldap \
      --disable-ldaps \
      --without-gssapi \
      --without-libpsl \
      --without-libidn2 \
      --without-apple-idn \
      --without-brotli \
      --without-zstd \
      --with-nghttp2=${nghttp2.dev} \
      --without-ngtcp2 \
      --without-nghttp3 \
      --without-quiche \
      --without-libssh2 \
      --without-libssh \
      --without-libgsasl \
      --without-libuv \
      --disable-docs \
      --disable-manual \
      --without-zsh-functions-dir \
      --without-fish-functions-dir
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES -C lib
    $CC -dynamiclib -o libcurl.4.dylib -Wl,-force_load,lib/.libs/libcurl.a \
      -framework CoreFoundation -framework CoreServices \
      -Wl,-weak_library,${libressl}/usr/lib/libcrypto.46.dylib \
      -Wl,-weak_library,${libressl}/usr/lib/libssl.48.dylib \
      -framework SystemConfiguration ${nghttp2}/usr/lib/libapple_nghttp2.dylib -lz \
      ${dylib.idFlags ./libcurl.exports} \
      -Wl,-exported_symbols_list,${./libcurl.exports} -Wl,-dead_strip ${linkFlags}
    grep -v '^#' ${./libcurl.exports} | sort > want
    $NM -gUj libcurl.4.dylib | sort > have
    cmp -s want have || { echo "libcurl exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib
    cp libcurl.4.dylib $out/usr/lib/
    # the names the SDK links by
    for l in libcurl libcurl.3; do
      ln -s libcurl.4.dylib $out/usr/lib/$l.dylib
    done
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.unix;
  };
}
