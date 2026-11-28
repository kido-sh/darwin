# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/libcurl.4.dylib (under macOS's install name, versions and exports) and
# /usr/bin/curl CLI linked dynamically against it. TLS is the image's LibreSSL, its libssl.48
# and libcrypto.46 weak-linked as on macOS, verifying against /etc/ssl/cert.pem. HTTP/2 through
# libapple_nghttp2, linked as on macOS. No GSS-API or Kerberos, LDAP or HTTP/3.
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
    export CFLAGS="-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -ffile-prefix-map=$PWD=."
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
    # Export macOS's libcurl ABI plus the four public curl 8.23.0 symbols that
    # curl's own CLI calls directly.
    {
      grep -v '^#' ${./libcurl.exports}
      printf '%s\n' \
        _curl_easy_ssls_export \
        _curl_easy_ssls_import \
        _curl_multi_get_offt \
        _curl_multi_notify_enable
    } | sort -u > want
    $CC -dynamiclib -o libcurl.4.dylib -Wl,-force_load,lib/.libs/libcurl.a \
      -framework CoreFoundation -framework CoreServices \
      -Wl,-weak_library,${libressl}/usr/lib/libcrypto.46.dylib \
      -Wl,-weak_library,${libressl}/usr/lib/libssl.48.dylib \
      -framework SystemConfiguration ${nghttp2}/usr/lib/libapple_nghttp2.dylib -lz \
      ${dylib.idFlags ./libcurl.exports} \
      -Wl,-exported_symbols_list,want -Wl,-dead_strip ${linkFlags}
    $NM -gUj libcurl.4.dylib | sort > have
    cmp -s want have || { echo "libcurl exports differ from expected:" >&2; diff want have >&2; exit 1; }

    # Build /usr/bin/curl dynamically linked against libcurl.4.dylib. Compile
    # src/*.c without CURL_STATICLIB and include only the small curlx helpers
    # upstream src/Makefile.am compiles into shared-linked curl binaries.
    make -j$NIX_BUILD_CORES -C src tool_hugehelp.c tool_ca_embed.c
    curl_objs=$(make -s -C src -f Makefile -f - print-curl-objs <<'EOF'
print-curl-objs:
	@echo $(curl_OBJECTS)
EOF
)
    curlx_srcs=$(make -s -C src -f Makefile -f - print-curlx-srcs <<'EOF'
print-curlx-srcs:
	@echo $(CURLX_CFILES)
EOF
)
    make -j$NIX_BUILD_CORES -C src curl_CPPFLAGS="-I../include -I../lib -I." $curl_objs
    mkdir -p src/curlx-objs
    curlx_objs=""
    for rel in $curlx_srcs; do
      base=$(basename "$rel" .c)
      obj="src/curlx-objs/$base.o"
      $CC -DHAVE_CONFIG_H -Iinclude -Ilib -Isrc $CPPFLAGS $CFLAGS -c "src/$rel" -o "$obj"
      curlx_objs="$curlx_objs $obj"
    done
    src_objs=""
    for obj in $curl_objs; do
      src_objs="$src_objs src/$obj"
    done
    $CC -o curl $src_objs $curlx_objs ./libcurl.4.dylib -lz \
      -Wl,-dead_strip -Wl,-dead_strip_dylibs \
      ${linkFlags} -Wl,-dylinker_install_name,/usr/lib/dyld
    $STRIP -x curl
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $out/usr/bin
    cp libcurl.4.dylib $out/usr/lib/
    # the names the SDK links by
    for l in libcurl libcurl.3; do
      ln -s libcurl.4.dylib $out/usr/lib/$l.dylib
    done
    install -m 755 curl $out/usr/bin/curl
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.unix;
  };
}
