# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# Clean-room Apache-2.0 /usr/bin/sw_vers reading SystemVersion.plist and linking
# only /usr/lib/libSystem.B.dylib.
{ stdenv
, lib
, darwinCrossToolchain
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
, nativeLd
, libSystem
, src
, appleSdk
}:

let
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
in
stdenv.mkDerivation {
  pname = "kido-sw_vers";
  version = "1.0.0";

  inherit src;

  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    export DARWIN_SDK_ROOT="${sdk}"
    ${t}-clang -arch ${arch} -isysroot "${sdk}" -std=c11 -Os -Wall -Wextra -Werror \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
      -ffile-prefix-map="$PWD=." \
      -c src/Userspace/sw_vers/sw_vers.c -o sw_vers.o
    ${t}-clang -arch ${arch} -isysroot "${sdk}" --ld-path=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylinker_install_name,/usr/lib/dyld \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-fixup_chains -Wl,-dead_strip -Wl,-dead_strip_dylibs \
      sw_vers.o -lSystem -o sw_vers
    ${t}-strip -x sw_vers
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    install -Dm755 sw_vers "$out/usr/bin/sw_vers"

    # Verify linkage closure: must link ONLY /usr/lib/libSystem.B.dylib.
    deps=$(${t}-otool -L "$out/usr/bin/sw_vers" | tail -n +2 | awk '{ print $1 }' | LC_ALL=C sort)
    if [ "$deps" != "/usr/lib/libSystem.B.dylib" ]; then
      echo "ERROR: unexpected dynamic library dependencies in sw_vers:" >&2
      ${t}-otool -L "$out/usr/bin/sw_vers" >&2
      exit 1
    fi
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    description = "Clean-room Apache-2.0 implementation of /usr/bin/sw_vers";
    license = licenses.asl20;
    platforms = platforms.unix;
  };
}
