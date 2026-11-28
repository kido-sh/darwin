# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# kl package recipe for python3 (PSF-2.0): /usr/bin/python3 (+ python3.13) and
# its standard library under /usr/lib/python3.13, linked strictly against
# KidoDarwin base image dylibs and kl's libffi (zero GPL readline/gdbm deps).
{ stdenv
, lib
, bash
, pkg-config
, python3
, darwinCrossToolchain
, nativeLd
, libSystem
, libz
, zlibDev
, libbz2
, bzip2Dev
, liblzma
, xzDev
, sqlite
, klLibffi
, appleSdk
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
}:

let
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  sdk = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  pyVer = python3.pythonVersion;
  klMeta = {
    name = "python3";
    version = python3.version;
    arch = arch;
    license = "PSF-2.0";
    summary = "Python ${pyVer} interpreter and standard library";
    depends = [ "libffi" ];
    paths = [
      "usr/bin/python3"
      "usr/bin/python${pyVer}"
      "usr/lib/python${pyVer}"
      "usr/share/doc/kl/python3"
    ];
  };
in
stdenv.mkDerivation {
  pname = "kido-kl-python3-${arch}";
  inherit (python3) version src;

  nativeBuildInputs = [ pkg-config python3 ];

  # CPython 3.13's configure omits *-apple-darwin* in its cross-build host
  # whitelist (listing only linux, cygwin, ios, vxworks, emscripten, wasi),
  # and leaves ac_sys_release empty for cross builds which breaks Darwin/*
  # _XOPEN_SOURCE checks unless ac_sys_release is set to KidoDarwin's 25.5.0.
  postPatch = ''
    ${python3}/bin/python3 - <<'PY'
p = "configure"
txt = open(p, "r", encoding="utf-8").read()
a = "\t*-apple-ios*)\n\t\tac_sys_system=iOS\n\t\t;;"
a_new = "\t*-apple-darwin*)\n\t\tac_sys_system=Darwin\n\t\t;;\n" + a
b = "\t*-apple-ios*)\n\t\t_host_os=`echo $host | cut -d '-' -f3`"
b_new = "\t*-apple-darwin*)\n\t\t_host_ident=arm64\n\t\t;;\n" + b
c = "\tac_sys_release=\n"
c_new = '\tif test "$ac_sys_system" = Darwin; then ac_sys_release=25.5.0; else ac_sys_release=; fi\n'
assert a in txt, "first cross-host case not found"
assert b in txt, "second cross-host case not found"
assert c in txt, "ac_sys_release reset not found"
txt = txt.replace(a, a_new, 1).replace(b, b_new, 1).replace(c, c_new, 1)
open(p, "w", encoding="utf-8").write(txt)
PY
  '';

  configurePhase = ''
    runHook preConfigure
    export DARWIN_SDK_ROOT="${sdk}"
    export MACOSX_DEPLOYMENT_TARGET=26.5
    export PATH="${darwinCrossToolchain}/bin:$PATH"

    mkdir -p "$TMPDIR/pkgconfig" "$TMPDIR/ccwrap"
    cat > "$TMPDIR/ccwrap/sw_vers" <<'EOF'
#!${bash}/bin/bash
case "''${1:-}" in
  -productVersion) echo "26.5" ;;
  -buildVersion) echo "25F74" ;;
  -productName) echo "KidoDarwin" ;;
  *) echo "ProductName:	KidoDarwin"; echo "ProductVersion:	26.5"; echo "BuildVersion:	25F74" ;;
esac
EOF
    chmod +x "$TMPDIR/ccwrap/sw_vers"
    export PATH="$TMPDIR/ccwrap:$PATH"

    cat > "$TMPDIR/pkgconfig/libffi.pc" <<EOF
prefix=${klLibffi.dev}
libdir=${klLibffi}/usr/lib
includedir=${klLibffi.dev}/include

Name: libffi
Description: Portable Foreign Function Interface library
Version: ${klLibffi.version}
Libs: -L${klLibffi}/usr/lib -lffi
Cflags: -I${klLibffi.dev}/include
EOF
    cat > "$TMPDIR/pkgconfig/zlib.pc" <<EOF
prefix=${zlibDev}
libdir=${libz}/usr/lib
includedir=${zlibDev}/include

Name: zlib
Description: zlib compression library
Version: ${libz.version}
Libs: -L${libz}/usr/lib -lz
Cflags: -I${zlibDev}/include
EOF
    cat > "$TMPDIR/pkgconfig/bzip2.pc" <<EOF
prefix=${bzip2Dev}
libdir=${libbz2}/usr/lib
includedir=${bzip2Dev}/include

Name: bzip2
Description: bzip2 compression library
Version: ${libbz2.version}
Libs: -L${libbz2}/usr/lib -lbz2
Cflags: -I${bzip2Dev}/include
EOF
    cat > "$TMPDIR/pkgconfig/liblzma.pc" <<EOF
prefix=${xzDev}
libdir=${liblzma}/usr/lib
includedir=${xzDev}/include

Name: liblzma
Description: XZ compression library
Version: ${liblzma.version}
Libs: -L${liblzma}/usr/lib -llzma
Cflags: -I${xzDev}/include
EOF
    cat > "$TMPDIR/pkgconfig/sqlite3.pc" <<EOF
prefix=${sqlite.dev}
libdir=${sqlite}/usr/lib
includedir=${sqlite.dev}/include

Name: SQLite
Description: SQL database engine
Version: ${sqlite.version}
Libs: -L${sqlite}/usr/lib -lsqlite3
Cflags: -I${sqlite.dev}/include
EOF

    export PKG_CONFIG_LIBDIR="$TMPDIR/pkgconfig"
    export PKG_CONFIG_PATH="$TMPDIR/pkgconfig"

    # Compiler wrapper that adds KidoDarwin link flags for executables and
    # Mach-O extension bundles while avoiding -dylinker_install_name on bundles.
    cat > "$TMPDIR/ccwrap/cc" <<'EOF'
#!${bash}/bin/bash
mode=link_exe
args=()
skip_next=0
for a in "$@"; do
  if [ "$skip_next" -eq 1 ]; then
    skip_next=0
    continue
  fi
  case "$a" in
    -c|-E|-S|-fsyntax-only) mode=compile ;;
    -bundle|-dynamiclib) mode=link_shared ;;
    -Wl,-rpath,*) continue ;;
    -rpath|-Wl,-rpath) skip_next=1; continue ;;
  esac
  args+=("$a")
done

common=(
  -arch ${arch}
  -isysroot "${sdk}"
  -Qunused-arguments
)

if [ "$mode" = compile ]; then
  exec "${t}-clang" "''${common[@]}" "''${args[@]}"
fi

link=(
  -fuse-ld="${nativeLd}/bin/ld"
  -nostdlib
  -L"${libSystem}/usr/lib"
  -L"${klLibffi}/usr/lib"
  -L"${libz}/usr/lib"
  -L"${libbz2}/usr/lib"
  -L"${liblzma}/usr/lib"
  -L"${sqlite}/usr/lib"
  -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:"${libSystem}/usr/lib/system/libdyld.dylib"
  -Wl,-dylib_file,/usr/lib/libffi.dylib:"${klLibffi}/usr/lib/libffi.dylib"
  -Wl,-dylib_file,/usr/lib/libz.1.dylib:"${libz}/usr/lib/libz.1.dylib"
  -Wl,-dylib_file,/usr/lib/libbz2.1.0.dylib:"${libbz2}/usr/lib/libbz2.1.0.dylib"
  -Wl,-dylib_file,/usr/lib/liblzma.5.dylib:"${liblzma}/usr/lib/liblzma.5.dylib"
  -Wl,-dylib_file,/usr/lib/libsqlite3.dylib:"${sqlite}/usr/lib/libsqlite3.dylib"
  -Wl,-platform_version,macos,26.5,26.5
  -Wl,-fixup_chains
  -Wl,-dead_strip_dylibs
)
if [ "$mode" = link_exe ]; then
  link+=(-Wl,-dylinker_install_name,/usr/lib/dyld)
fi

exec "${t}-clang" "''${common[@]}" "''${link[@]}" "''${args[@]}" -lSystem
EOF
    chmod +x "$TMPDIR/ccwrap/cc"

    export CC="$TMPDIR/ccwrap/cc"
    export AR="${t}-ar"
    export RANLIB="${t}-ranlib"
    export NM="${t}-nm"
    export READELF="true"
    export CFLAGS="-O2 -fPIC -ffile-prefix-map=$TMPDIR=. -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -I${libSystem}/usr/include -I${klLibffi.dev}/include -I${zlibDev}/include -I${bzip2Dev}/include -I${xzDev}/include -I${sqlite.dev}/include"
    export CPPFLAGS="$CFLAGS"
    export LDFLAGS=""
    export LDSHARED="$CC -bundle -undefined dynamic_lookup"
    export LDCXXSHARED="$CC -bundle -undefined dynamic_lookup"

    # Cross-compilation autoconf answers for Darwin arm64.
    export ac_cv_file__dev_ptmx=yes
    export ac_cv_file__dev_ptc=no
    export ac_cv_buggy_getaddrinfo=no
    export ac_cv_working_tzset=yes
    export ac_cv_have_long_long_format=yes
    export ac_cv_aligned_required=no
    export ac_cv_pthread_is_default=yes
    export ac_cv_pthread=yes
    export ac_cv_pthread_system_supported=yes
    export ac_cv_func_explicit_bzero=no
    export ac_cv_func_getentropy=yes
    export ac_cv_func_clock_gettime=yes
    export ac_cv_func_sendfile=yes
    export ac_cv_func_getresuid=no
    export ac_cv_func_getresgid=no
    export ac_cv_func_setresuid=no
    export ac_cv_func_setresgid=no
    export ac_cv_func_lchflags=no
    export ac_cv_func_chflags=no

    # Strictly disable GPL/non-base modules (readline, gdbm, dbm, tkinter, curses,
    # and OpenSSL 1.1.1+ _ssl/_hashlib; builtin pure-C HACL* hash modules remain active).
    export py_cv_module_readline=n/a
    export py_cv_module__gdbm=n/a
    export py_cv_module__dbm=n/a
    export py_cv_module__tkinter=n/a
    export py_cv_module__curses=n/a
    export py_cv_module__curses_panel=n/a
    export py_cv_module__ssl=n/a
    export py_cv_module__hashlib=n/a
    export py_cv_module__uuid=n/a
    export py_cv_module__scproxy=n/a

    ./configure \
      --host=${targetTriple} \
      --build=${stdenv.buildPlatform.config} \
      --with-build-python=${python3}/bin/python3 \
      --prefix=/usr \
      --disable-shared \
      --disable-ipv6 \
      --without-ensurepip \
      --without-readline \
      --disable-test-modules
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    make -j"$NIX_BUILD_CORES"
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    make install DESTDIR="$TMPDIR/stage"

    mkdir -p "$out/usr/bin" "$out/usr/lib" "$out/usr/share/doc/kl/python3"
    cp "$TMPDIR/stage/usr/bin/python${pyVer}" "$out/usr/bin/python${pyVer}"
    chmod 755 "$out/usr/bin/python${pyVer}"
    ln -s "python${pyVer}" "$out/usr/bin/python3"

    cp -a "$TMPDIR/stage/usr/lib/python${pyVer}" "$out/usr/lib/python${pyVer}"
    chmod -R u+w "$out/usr/lib/python${pyVer}"

    # Prune test suites, GUI/Tk, build-time static config, and bytecode caches
    # so the kl package stays lean, deterministic, and free of host store paths.
    rm -rf \
      "$out/usr/lib/python${pyVer}/test" \
      "$out/usr/lib/python${pyVer}/idlelib" \
      "$out/usr/lib/python${pyVer}/tkinter" \
      "$out/usr/lib/python${pyVer}/turtledemo" \
      "$out/usr/lib/python${pyVer}/ensurepip" \
      "$out/usr/lib/python${pyVer}/config-${pyVer}-darwin" \
      "$out/usr/lib/python${pyVer}/__phello__"
    find "$out/usr/lib/python${pyVer}" -type d -name '__pycache__' -prune -exec rm -rf {} +
    find "$out/usr/lib/python${pyVer}" -type f \( -name '*.pyc' -o -name '*.pyo' -o -name '*.a' \) -delete

    # Scrub build-host Nix store and TMPDIR paths from _sysconfigdata_*.py and
    # verify the resulting Python module still parses cleanly.
    for sc in "$out/usr/lib/python${pyVer}"/_sysconfigdata_*.py; do
      [ -f "$sc" ] || continue
      ${python3}/bin/python3 - "$sc" "$TMPDIR" <<'PY'
import os, re, sys
p, tmpdir = sys.argv[1], sys.argv[2]
txt = open(p, "r", encoding="utf-8").read()
if tmpdir:
    txt = txt.replace(tmpdir, "/usr")
txt = re.sub(r"/nix/store/[a-z0-9]{32}-[^\s'\"\\:,;)\]]+", "/usr", txt)
txt = re.sub(r"/build/[^\s'\"\\:,;)\]]+", "/usr", txt)
compile(txt, p, "exec")
open(p, "w", encoding="utf-8").write(txt)
PY
    done

    # Confirm no /nix/store/ references leaked into the installed Python stdlib or binary.
    if grep -rnF '/nix/store/' "$out/usr/lib/python${pyVer}"; then
      echo "ERROR: /nix/store/ path leaked into installed Python stdlib" >&2
      exit 1
    fi
    if LC_ALL=C grep -qF '/nix/store/' "$out/usr/bin/python${pyVer}"; then
      echo "ERROR: /nix/store/ path leaked into python${pyVer} binary" >&2
      exit 1
    fi

    # Confirm _ctypes built and links /usr/lib/libffi.dylib.
    ctypes_so=$(find "$out/usr/lib/python${pyVer}/lib-dynload" -name '_ctypes*.so' | head -n 1)
    [ -f "$ctypes_so" ] || { echo "ERROR: _ctypes extension failed to build" >&2; exit 1; }
    "${t}-otool" -L "$ctypes_so" | grep -q '/usr/lib/libffi.dylib' || {
      echo "ERROR: _ctypes did not link /usr/lib/libffi.dylib:" >&2
      "${t}-otool" -L "$ctypes_so" >&2
      exit 1
    }

    install -Dm644 LICENSE "$out/usr/share/doc/kl/python3/LICENSE"
    install -Dm644 Modules/expat/COPYING "$out/usr/share/doc/kl/python3/EXPAT-COPYING"
    cat > "$out/usr/share/doc/kl/python3/CHANGES" <<'EOF'
KidoDarwin packaging changes applied to CPython ${python3.version}:
- Added *-apple-darwin* host cross-compilation entries in configure.
- Built without GPL readline/gdbm, Tkinter, or OpenSSL _ssl/_hashlib modules.
- Pruned test suites, idlelib, tkinter, turtledemo, ensurepip, static archives,
  and precompiled bytecode; scrubbed build-host paths from _sysconfigdata.
EOF
    chmod 644 "$out/usr/share/doc/kl/python3/CHANGES"

    "${bash}/bin/bash" ${./check-gates.sh} \
      --root "$out" \
      --pkg-name python3 \
      --arch "${arch}" \
      --license "${klMeta.license}" \
      --base-dylibs ${./base-dylibs.txt} \
      --allowed-dep /usr/lib/libffi.dylib \
      --otool "${t}-otool" \
      --objdump "${t}-objdump"
    runHook postInstall
  '';

  dontFixup = true;

  passthru = {
    kl = klMeta;
  };

  meta = with lib; {
    description = "kl system package for Python ${pyVer} (${arch})";
    license = licenses.psfl;
    platforms = platforms.unix;
  };
}
