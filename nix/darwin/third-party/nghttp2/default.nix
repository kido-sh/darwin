# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/libapple_nghttp2.dylib, nghttp2's HTTP/2 library under macOS's install name, versions
# and exports.
{ stdenv
, lib
, callPackage
, fetchFromGitHub
, darwinCrossToolchain
, nativeLd
, libSystem
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
, appleSdk
}:

let
  pin = lib.importJSON ./source.json;
  dylib = callPackage ../dylib.nix { };
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  parts = map lib.toInt (lib.splitString "." pin.version);
  number = lib.toHexString (builtins.elemAt parts 0 * 65536 + builtins.elemAt parts 1 * 256 + builtins.elemAt parts 2);
  srcs = [
    "nghttp2_pq" "nghttp2_map" "nghttp2_queue" "nghttp2_frame" "nghttp2_buf" "nghttp2_stream"
    "nghttp2_outbound_item" "nghttp2_session" "nghttp2_submit" "nghttp2_helper" "nghttp2_alpn"
    "nghttp2_hd" "nghttp2_hd_huffman" "nghttp2_hd_huffman_data" "nghttp2_version"
    "nghttp2_priority_spec" "nghttp2_option" "nghttp2_callbacks" "nghttp2_mem" "nghttp2_http"
    "nghttp2_rcbuf" "nghttp2_extpri" "nghttp2_ratelim" "nghttp2_time" "nghttp2_debug" "sfparse"
  ];
in
stdenv.mkDerivation {
  pname = "kido-nghttp2";
  inherit (pin) version;

  src = fetchFromGitHub {
    owner = "nghttp2";
    repo = "nghttp2";
    tag = "v${pin.version}";
    inherit (pin) hash;
  };

  sourceRoot = "source/lib";
  outputs = [ "out" "dev" ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    substitute includes/nghttp2/nghttp2ver.h.in includes/nghttp2/nghttp2ver.h \
      --replace-fail @PACKAGE_VERSION@ ${pin.version} --replace-fail @PACKAGE_VERSION_NUM@ 0x${number}
    for f in ${lib.concatStringsSep " " srcs}; do
      ${t}-clang -arch ${arch} -isysroot "$SDK" -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
        -DBUILDING_NGHTTP2 -DHAVE_ARPA_INET_H=1 -DHAVE_NETINET_IN_H=1 -DHAVE_CLOCK_GETTIME=1 \
        -DHAVE_DECL_CLOCK_MONOTONIC=1 -fvisibility=hidden -Iincludes -I. -c $f.c -o $f.o
    done
    grep -v '^#' ${./libapple_nghttp2.exports} | grep -vxF -f ${./libapple_nghttp2.apple-only} | sort > want
    ${t}-clang -arch ${arch} -dynamiclib -isysroot "$SDK" -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -Wl,-exported_symbols_list,want ${dylib.idFlags ./libapple_nghttp2.exports} \
      ${lib.concatMapStringsSep " " (f: "${f}.o") srcs} -lSystem -o libapple_nghttp2.dylib
    ${t}-nm -gUj libapple_nghttp2.dylib | sort > have
    cmp -s want have || { echo "libapple_nghttp2 exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $dev/include/nghttp2 $dev/lib
    cp libapple_nghttp2.dylib $out/usr/lib/
    ln -s $out/usr/lib/libapple_nghttp2.dylib $dev/lib/libnghttp2.dylib
    cp includes/nghttp2/nghttp2.h includes/nghttp2/nghttp2ver.h $dev/include/nghttp2/
    runHook postInstall
  '';

  dontFixup = true;
}
