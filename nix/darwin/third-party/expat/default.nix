# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/libexpat.1.dylib under macOS's install name, versions and exports, built with
# macOS's features: DTDs, general entities, namespaces and 1024 bytes of context.
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
  srcs = [ "xcs" "xmlparse" "xmltok" "xmlrole" "random_arc4random_buf" "random_dev_urandom" ];
in
stdenv.mkDerivation {
  pname = "kido-expat";
  inherit (pin) version;

  src = fetchFromGitHub {
    owner = "libexpat";
    repo = "libexpat";
    name = "expat-${pin.version}-source";
    tag = "R_${lib.replaceStrings [ "." ] [ "_" ] pin.version}";
    inherit (pin) hash;
    postFetch = "rm -r $out/testdata";
  };

  sourceRoot = "expat-${pin.version}-source/expat/lib";
  outputs = [ "out" "dev" ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    printf '#define %s\n' "BYTEORDER 1234" "HAVE_ARC4RANDOM_BUF 1" "XML_CONTEXT_BYTES 1024" \
      "XML_DEV_URANDOM 1" "XML_DTD 1" "XML_GE 1" "XML_NS 1" > expat_config.h
    for f in ${lib.concatStringsSep " " srcs}; do
      ${t}-clang -arch ${arch} -isysroot "$SDK" -O2 -fno-strict-aliasing -U_FORTIFY_SOURCE \
        -D_FORTIFY_SOURCE=0 -I. -c $f.c -o $f.o
    done
    ${t}-clang -arch ${arch} -dynamiclib -isysroot "$SDK" -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -Wl,-exported_symbols_list,${./libexpat.exports} ${dylib.idFlags ./libexpat.exports} \
      ${lib.concatMapStringsSep " " (f: "${f}.o") srcs} -lSystem -o libexpat.1.dylib
    grep -v '^#' ${./libexpat.exports} | sort > want
    ${t}-nm -gUj libexpat.1.dylib | sort > have
    cmp -s want have || { echo "libexpat exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $dev/include $dev/lib
    cp libexpat.1.dylib $out/usr/lib/
    ln -s libexpat.1.dylib $out/usr/lib/libexpat.dylib
    ln -s $out/usr/lib/libexpat.1.dylib $dev/lib/libexpat.dylib
    cp expat.h expat_external.h $dev/include/
    runHook postInstall
  '';

  dontFixup = true;
}
