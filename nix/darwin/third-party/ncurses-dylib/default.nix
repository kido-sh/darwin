# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# macOS's ncurses: /usr/lib/libncurses.5.4.dylib with libpanel, libmenu and libform, under macOS's
# install names, versions and exports, and the terminfo database in /usr/share/terminfo. The
# configuration is the ABI the macOS 15 and later SDK headers describe: wide characters with
# ext_color in cchar_t, a 32-bit chtype, a long mmask_t, mouse version 1, and terminfo directories
# named by the hex code of the first letter.
{ stdenv
, lib
, darwinCrossToolchain
, nativeLd
, targetTriple ? "arm64-apple-darwin20.4"
, libSystem
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
  # libpanel, libmenu and libform: file name, the static archive of ncurses' build, the export list
  dylibs = [
    [ "libpanel.5.4.dylib" "lib/libpanelw.a" ./libpanel.exports ]
    [ "libmenu.5.4.dylib" "lib/libmenuw.a" ./libmenu.exports ]
    [ "libform.5.4.dylib" "lib/libformw.a" ./libform.exports ]
  ];
in
stdenv.mkDerivation {
  pname = "kido-ncurses-dylib";
  inherit (ncurses) version src;

  # tic, to compile the terminfo database
  nativeBuildInputs = [ ncurses ];

  postPatch = ''
    cat ${./abi60.h} >> include/curses.wide
    # macOS's libncurses exports this, as ncurses 6.0 did
    substituteInPlace ncurses/tinfo/entries.c \
      --replace-fail $'static ENTRY *\n_nc_delink_entry' $'NCURSES_EXPORT(ENTRY *)\n_nc_delink_entry'
  '';

  configurePhase = ''
    runHook preConfigure
    export DARWIN_SDK_ROOT="${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    export PATH="${darwinCrossToolchain}/bin:$PATH"
    export CC=${t}-clang AR=${t}-ar RANLIB=${t}-ranlib STRIP=${t}-strip NM=${t}-nm
    export CFLAGS="-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"
    export LDFLAGS="${linkFlags} -Wl,-dylinker_install_name,/usr/lib/dyld"
    ./configure \
      --host=${targetTriple} \
      --build=$(cc -dumpmachine) \
      --prefix=/usr \
      --with-abi-version=5 \
      --enable-widec \
      --enable-ext-colors \
      --disable-ext-mouse \
      --with-chtype=int \
      --with-mmask-t=long \
      --disable-lp64 \
      --with-tparm-arg=long \
      --enable-opaque-curses \
      --disable-sp-funcs \
      --disable-interop \
      --enable-termcap \
      --without-shared \
      --with-normal \
      --without-debug \
      --without-cxx \
      --without-cxx-binding \
      --without-ada \
      --without-progs \
      --without-tests \
      --without-manpages \
      --disable-db-install \
      --disable-stripping \
      --disable-mixed-case \
      --with-xterm-kbs=BS \
      --with-terminfo-dirs=/usr/share/terminfo \
      --with-default-terminfo-dir=/usr/share/terminfo
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j$NIX_BUILD_CORES -C include
    for d in ncurses panel menu form; do
      make -j$NIX_BUILD_CORES -C $d
    done

    # nothing in the archives may reach the plain-named entry points, which abi5.c keeps for old callers
    plain=$(sed -n 's/^\(_[A-Za-z0-9_]*\)\$NCURSES60$/\1/p' ${./libncurses.exports})
    for a in lib/libncursesw.a lib/libpanelw.a lib/libmenuw.a lib/libformw.a; do
      if $NM -u $a | grep -xF -e __nc_wacs -e _initscr -e _newterm \
          $(sed -n 's/^extern.*KIDO_ABI60(\(.*\));$/-e _\1/p' include/curses.h); then
        echo "$a calls a plain-named entry point" >&2
        exit 1
      fi
    done

    $CC $CFLAGS -DNCURSES_WIDECHAR=1 -Iinclude -c ${./abi5.c} -o abi5.o
    $CC $CFLAGS -DHAVE_CONFIG_H -DBUILDING_NCURSES -D_DARWIN_C_SOURCE -DNDEBUG -Incurses -Iinclude \
      -c ${./ncurses60.c} -o ncurses60.o
    $NM -gUj lib/libncursesw.a abi5.o | grep '^_' | sort -u > defined
    for s in $plain; do
      grep -qxF "$s\$NCURSES60" defined || echo "$s $s\$NCURSES60"
    done > aliases
    $CC -dynamiclib -o libncurses.5.4.dylib -Wl,-force_load,lib/libncursesw.a abi5.o ncurses60.o \
      -install_name /usr/lib/libncurses.5.4.dylib -compatibility_version 5.4 -current_version 5.4 \
      -Wl,-exported_symbols_list,${./libncurses.exports} -Wl,-alias_list,aliases ${linkFlags}
    ${lib.concatMapStrings (d: ''
      $CC -dynamiclib -o ${builtins.elemAt d 0} -Wl,-force_load,${builtins.elemAt d 1} libncurses.5.4.dylib \
        -install_name /usr/lib/${builtins.elemAt d 0} -compatibility_version 5.4 -current_version 5.4 \
        -Wl,-exported_symbols_list,${builtins.elemAt d 2} ${linkFlags}
    '') dylibs}

    for l in libncurses:${./libncurses.exports} libpanel:${./libpanel.exports} \
             libmenu:${./libmenu.exports} libform:${./libform.exports}; do
      grep -v '^#' ''${l#*:} | sort > want
      $NM -gUj ''${l%%:*}.5.4.dylib | sort > have
      cmp -s want have || { echo "''${l%%:*} exports differ from macOS's:" >&2; diff want have >&2; exit 1; }
    done

    # macOS's layout: aliases link to the entry, the directory is the first letter's hex code
    mkdir terminfo
    tic -x -o "$PWD/terminfo" misc/terminfo.src
    (
      cd terminfo
      for d in ?; do
        mv "$d" "$(printf %02x "'$d")"
      done
      find . -type l | while read -r link; do
        target=$(readlink "$link")
        dir=''${target#.././}
        dir=''${dir%%/*}
        ln -sfn "../$(printf %02x "'$dir")/''${target##*/}" "$link"
      done
    )
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/usr/lib $out/usr/share
    cp libncurses.5.4.dylib libpanel.5.4.dylib libmenu.5.4.dylib libform.5.4.dylib $out/usr/lib/
    # the names the SDK links by
    for l in libncurses.5 libncurses libcurses libtermcap; do
      ln -s libncurses.5.4.dylib $out/usr/lib/$l.dylib
    done
    for l in panel menu form; do
      ln -s lib$l.5.4.dylib $out/usr/lib/lib$l.dylib
    done
    cp -a terminfo $out/usr/share/terminfo
    # headers for the libraries built against these, not installed in the image
    mkdir -p $out/include
    cp include/curses.h include/ncurses_dll.h include/term.h include/termcap.h include/unctrl.h \
      include/term_entry.h panel/panel.h menu/menu.h menu/eti.h form/form.h $out/include/
    runHook postInstall
  '';

  dontFixup = true;

  meta = with lib; {
    platforms = platforms.unix;
  };
}
