# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's iconv: Apple's libiconv, the FreeBSD and NetBSD Citrus implementation, as
# /usr/lib/libiconv.2.dylib and libcharset.1.dylib, its converter modules in /usr/lib/i18n, its tables
# in /usr/share/i18n and /usr/bin/iconv, under macOS's install names, versions and exports.
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
  variantHeader = lib.fileset.toSource {
    root = ../../../../darwin/src/Libraries/libSystem/libc;
    fileset = ../../../../darwin/src/Libraries/libSystem/libc/os/variant_private.h;
  };
  core = [
    "__iconv_get_list" "__iconv_free_list" "__iconv" "bsd_iconv" "citrus_bcs_strtol" "citrus_bcs_strtoul"
    "citrus_bcs" "citrus_csmapper" "citrus_db" "citrus_db_factory" "citrus_db_hash" "citrus_esdb"
    "citrus_hash" "citrus_iconv" "citrus_lookup_factory" "citrus_lookup" "citrus_mapper"
    "citrus_memstream" "citrus_mmap" "citrus_module" "citrus_none" "citrus_pivot_factory" "citrus_prop"
    "citrus_stdenc" "iconv_canonicalize" "iconv_close" "iconv_compat" "iconv_open_into" "iconv_open"
    "iconv_set_relocation_prefix" "iconvctl" "iconvlist" "iconv"
  ];
  modules = map (m: lib.removePrefix "lib" (lib.removeSuffix ".exports" m))
    (builtins.attrNames (builtins.readDir ./i18n));
  moduleSource = m: let dir = if m == "mapper_parallel" then "mapper_serial" else m;
    in "libiconv_modules/${dir}/citrus_${lib.toLower dir}.c";
  libDefines = "-DHAVE_CONFIG_H -DBUILDING_DLL -DENABLE_RELOCATABLE -DIN_LIBRARY -DNO_XMALLOC -DPIC";
in
stdenv.mkDerivation {
  pname = "kido-libiconv";
  inherit (pin) version;

  src = fetchFromGitHub {
    owner = "apple-oss-distributions";
    repo = "libiconv";
    tag = "libiconv-${pin.version}";
    inherit (pin) hash;
    postFetch = "rm -r $out/tests";
  };

  outputs = [ "out" "dev" ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    CC="${t}-clang -arch ${arch} -isysroot $SDK -Os -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    LINK="${t}-clang -arch ${arch} -isysroot $SDK -fuse-ld=${nativeLd}/bin/ld -nostdlib -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -Wl,-dylib_file,/usr/lib/libcharset.1.dylib:$PWD/libcharset.1.dylib"
    check() {
      grep -v '^#' "$2" | sort > want
      ${t}-nm -gUj "$1" | sort > have
      cmp -s want have || { echo "$1 exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    }
    mkdir -p obj/core obj/i18n
    MODINC=$(for d in libiconv_modules/*; do printf -- '-I%s ' $d; done)

    $CC -std=gnu89 ${libDefines} -I${variantHeader} -c libcharset/libcharset.c -o obj/libcharset.o
    $LINK -dynamiclib -Wl,-exported_symbols_list,${./libcharset.exports} ${dylib.idFlags ./libcharset.exports} \
      obj/libcharset.o -lSystem -o libcharset.1.dylib
    check libcharset.1.dylib ${./libcharset.exports}

    for f in ${lib.concatStringsSep " " core}; do
      $CC -std=c99 ${libDefines} -DBUILDING_LIBICONV -Drelocate=libiconv_relocate \
        -Dset_relocation_prefix=libiconv_set_relocation_prefix '-D_PATH_I18NMODULE="/usr/lib/i18n"' \
        '-D_PATH_ESDB="/usr/share/i18n/esdb"' '-D_PATH_CSMAPPER="/usr/share/i18n/csmapper"' \
        -Icitrus -Ilibcharset -c citrus/$f.c -o obj/core/$f.o
    done
    $LINK -dynamiclib -Wl,-exported_symbols_list,${./libiconv.exports} ${dylib.idFlags ./libiconv.exports} \
      obj/core/*.o -Wl,-reexport_library,libcharset.1.dylib -lSystem -o libiconv.2.dylib
    check libiconv.2.dylib ${./libiconv.exports}

    ${lib.concatMapStrings (m: ''
      $CC -std=gnu89 ${libDefines} -Icitrus -Ilibcharset $MODINC -c ${moduleSource m} -o obj/i18n/${m}.o
      $LINK -dynamiclib -Wl,-exported_symbols_list,${./i18n + "/lib${m}.exports"} \
        ${dylib.idFlags (./i18n + "/lib${m}.exports")} obj/i18n/${m}.o \
        ${lib.optionalString (m != "iconv_none") "libiconv.2.dylib"} -lSystem -o lib${m}.dylib
      check lib${m}.dylib ${./i18n + "/lib${m}.exports"}
    '') modules}

    $CC -std=gnu89 -DHAVE_CONFIG_H -DDEPENDS_ON_LIBICONV -Icitrus -Ilibcharset -c iconv/iconv.c -o obj/iconv.o
    $LINK obj/iconv.o libiconv.2.dylib -lSystem -Wl,-dylinker_install_name,/usr/lib/dyld -o iconv-tool
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib/i18n $out/usr/bin $out/usr/share/i18n $dev/include $dev/lib
    cp libiconv.2.dylib libcharset.1.dylib $out/usr/lib/
    ln -s libiconv.2.dylib $out/usr/lib/libiconv.dylib
    ln -s libcharset.1.dylib $out/usr/lib/libcharset.dylib
    cp ${lib.concatMapStringsSep " " (m: "lib${m}.dylib") modules} $out/usr/lib/i18n/
    cp iconv-tool $out/usr/bin/iconv
    cp -R i18n/csmapper i18n/esdb $out/usr/share/i18n/
    ln -s $out/usr/lib/libiconv.2.dylib $dev/lib/libiconv.dylib
    ln -s $out/usr/lib/libcharset.1.dylib $dev/lib/libcharset.dylib
    cp citrus/iconv.h libcharset/libcharset.h libcharset/localcharset.h $dev/include/
    runHook postInstall
  '';

  dontFixup = true;
}
