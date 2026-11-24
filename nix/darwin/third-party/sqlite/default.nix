# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's /usr/lib/libsqlite3.dylib under macOS's install name, versions and exports, with macOS's
# compile options but its encryption codec and its own mutexes. Built from SQLite's canonical source, as
# the amalgamation's parser lacks UPDATE and DELETE with ORDER BY and LIMIT, with the CSV and intarray
# modules macOS links in.
{ stdenv
, lib
, callPackage
, fetchurl
, unzip
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
  number = toString (builtins.elemAt parts 0 * 1000000 + builtins.elemAt parts 1 * 10000 + builtins.elemAt parts 2 * 100);
  options = [
    "BUG_COMPATIBLE_20160819"
    "DEFAULT_AUTOVACUUM=0"
    "DEFAULT_CACHE_SIZE=2000"
    "DEFAULT_CKPTFULLFSYNC=1"
    "DEFAULT_FILE_FORMAT=4"
    "DEFAULT_JOURNAL_SIZE_LIMIT=32768"
    "DEFAULT_LEGACY_ALTER_TABLE"
    "DEFAULT_LOOKASIDE=1200,102"
    "DEFAULT_MEMSTATUS=0"
    "DEFAULT_MMAP_SIZE=0"
    "DEFAULT_PAGE_SIZE=4096"
    "DEFAULT_PCACHE_INITSZ=20"
    "DEFAULT_RECURSIVE_TRIGGERS=0"
    "DEFAULT_SECTOR_SIZE=4096"
    "DEFAULT_SYNCHRONOUS=2"
    "DEFAULT_WAL_AUTOCHECKPOINT=1000"
    "DEFAULT_WAL_SYNCHRONOUS=1"
    "DEFAULT_WORKER_THREADS=0"
    "DIRECT_OVERFLOW_READ"
    "DQS=3"
    "ENABLE_API_ARMOR"
    "ENABLE_BYTECODE_VTAB"
    "ENABLE_CARRAY"
    "ENABLE_COLUMN_METADATA"
    "ENABLE_DBSTAT_VTAB"
    "ENABLE_FTS3"
    "ENABLE_FTS3_PARENTHESIS"
    "ENABLE_FTS3_TOKENIZER"
    "ENABLE_FTS4"
    "ENABLE_FTS5"
    "ENABLE_LOCKING_STYLE=1"
    "ENABLE_MATH_FUNCTIONS"
    "ENABLE_NORMALIZE"
    "ENABLE_PERCENTILE"
    "ENABLE_PREUPDATE_HOOK"
    "ENABLE_RTREE"
    "ENABLE_SESSION"
    "ENABLE_SETLK_TIMEOUT"
    "ENABLE_SNAPSHOT"
    "ENABLE_STMT_SCANSTATUS"
    "ENABLE_UNKNOWN_SQL_FUNCTION"
    "ENABLE_UPDATE_DELETE_LIMIT"
    "FAST_SECURE_DELETE"
    "MALLOC_SOFT_LIMIT=1024"
    "MAX_ATTACHED=10"
    "MAX_COLUMN=2000"
    "MAX_COMPOUND_SELECT=500"
    "MAX_DEFAULT_PAGE_SIZE=8192"
    "MAX_EXPR_DEPTH=1000"
    "MAX_FUNCTION_ARG=127"
    "MAX_LENGTH=2147483645"
    "MAX_LIKE_PATTERN_LENGTH=50000"
    "MAX_MMAP_SIZE=1073741824"
    "MAX_PAGE_COUNT=1073741823"
    "MAX_PAGE_SIZE=65536"
    "MAX_SQL_LENGTH=1000000000"
    "MAX_TRIGGER_DEPTH=1000"
    "MAX_VARIABLE_NUMBER=500000"
    "MAX_VDBE_OP=250000000"
    "MAX_WORKER_THREADS=8"
    "OMIT_AUTORESET"
    "OMIT_LOAD_EXTENSION"
    "STMTJRNL_SPILL=131072"
    "SYSTEM_MALLOC"
    "TEMP_STORE=1"
    "THREADSAFE=2"
    "USE_URI=1"
  ];
  defines = lib.concatMapStringsSep " " (o: "-DSQLITE_${o}") options;
in
stdenv.mkDerivation {
  pname = "kido-sqlite";
  inherit (pin) version;

  src = fetchurl {
    url = "https://sqlite.org/${pin.year}/sqlite-src-${number}.zip";
    inherit (pin) hash;
  };

  nativeBuildInputs = [ unzip ];
  outputs = [ "out" "dev" ];
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild
    ./configure --disable-tcl --prefix=/unused
    make sqlite3.c OPT_FEATURE_FLAGS="${defines}"
    SDK="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    for f in sqlite3.c ext/misc/csv.c src/test_intarray.c; do
      ${t}-clang -arch ${arch} -isysroot "$SDK" -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
        -DHAVE_ISNAN=1 -DHAVE_LOCALTIME_R=1 -DHAVE_GMTIME_R=1 -DHAVE_STRERROR_R=1 -DHAVE_USLEEP=1 \
        -DSQLITE_CORE ${defines} -I. -c $f -o $(basename $f .c).o
    done
    grep -v '^#' ${./libsqlite3.exports} | grep -vxF -f ${./libsqlite3.apple-only} | sort > want
    ${t}-clang -arch ${arch} -dynamiclib -isysroot "$SDK" -fuse-ld=${nativeLd}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -Wl,-exported_symbols_list,want ${dylib.idFlags ./libsqlite3.exports} \
      sqlite3.o csv.o test_intarray.o -lSystem -o libsqlite3.dylib
    ${t}-nm -gUj libsqlite3.dylib | sort > have
    cmp -s want have || { echo "libsqlite3 exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $dev/include $dev/lib
    cp libsqlite3.dylib $out/usr/lib/
    ln -s libsqlite3.dylib $out/usr/lib/libsqlite3.0.dylib
    ln -s $out/usr/lib/libsqlite3.dylib $dev/lib/libsqlite3.dylib
    cp sqlite3.h sqlite3ext.h $dev/include/
    runHook postInstall
  '';

  dontFixup = true;
}
