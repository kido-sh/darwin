# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/libutil.dylib from Apple's libutil, under macOS's install name, versions and exports.
# Its tzbootuuid.c serves only tzlinkd, which the image does not have, and is left out.
{ stdenv
, lib
, callPackage
, fetchFromGitHub
, darwinCrossToolchain
, nativeLd
, libSystem
, libcxxDylib
, libcxxabiDylib
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
, appleSdk
}:

let
  pin = lib.importJSON ./source.json;
  dylib = callPackage ../dylib.nix { };
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  csrcs = [ "getmntopts" "humanize_number" "pidfile" "expand_number" "realhostname" "reexec_to_match_kernel"
    "trimdomain" "tzlink" ];
  cxxsrcs = [ "ExtentManager" "wipefs" ];
in
stdenv.mkDerivation {
  pname = "kido-libutil";
  inherit (pin) version;

  src = fetchFromGitHub {
    owner = "apple-oss-distributions";
    repo = "libutil";
    tag = "libutil-${pin.version}";
    inherit (pin) hash;
  };

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    FLAGS="-arch ${arch} -isysroot $SDK -Os -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I."
    for f in ${lib.concatStringsSep " " csrcs}; do
      ${t}-clang $FLAGS -c $f.c -o $f.o
    done
    for f in ${lib.concatStringsSep " " cxxsrcs}; do
      ${t}-clang++ $FLAGS -DkIOStorageUnmapOptionWhole=0x00000001 -c $f.cpp -o $f.o
    done
    ${t}-clang -arch ${arch} -dynamiclib -isysroot "$SDK" -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -Wl,-exported_symbols_list,${./libutil.exports} ${dylib.idFlags ./libutil.exports} \
      ${lib.concatMapStringsSep " " (f: "${f}.o") (csrcs ++ cxxsrcs)} \
      -L${libcxxabiDylib}/usr/lib -L${libcxxDylib}/usr/lib -lc++ -lSystem -o libutil.dylib
    grep -v '^#' ${./libutil.exports} | sort > want
    ${t}-nm -gUj libutil.dylib | sort > have
    cmp -s want have || { echo "libutil exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib
    cp libutil.dylib $out/usr/lib/
    ln -s libutil.dylib $out/usr/lib/libutil1.0.dylib
    runHook postInstall
  '';

  dontFixup = true;
}
