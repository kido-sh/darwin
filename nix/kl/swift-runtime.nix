# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# kl package recipe for the open-source Swift standard library and core runtime
# dylibs under /usr/lib/swift/ (Apache-2.0 WITH Swift-exception AND
# Apache-2.0 WITH LLVM-exception):
#   - libswiftCore.dylib
#   - libswiftSwiftOnoneSupport.dylib
#   - libswift_Concurrency.dylib
#   - libswiftDarwin.dylib
#   - libswift_RegexParser.dylib
#   - libswift_StringProcessing.dylib
#   - libswiftRegexBuilder.dylib
#   - libswiftObservation.dylib
#   - libswiftDistributed.dylib
#
# Cross-compiled from upstream Swift sources on aarch64-linux using the host
# nixpkgs swift-frontend for Swift compilation, LLVM 21 clang for the C++/ObjC++
# runtime, LLVM 21 ld64.lld for portable arm64 linking, and cctools ld64 for
# arm64e linking against KidoDarwin base image dylibs (libSystem, libc++,
# libc++abi, libobjc).
{ stdenv
, lib
, bash
, cmake
, ninja
, python3
, gawk
, llvmPackages_21
, swiftPackages
, swiftSrc
, swiftStringProcessingSrc
, swiftVersion
, darwinCrossToolchain
, nativeLd
, libSystem
, libobjc
, libcxxDylib
, libcxxabiDylib
, foundation
, libz
, zlibDev
, appleSdk
, targetTriple ? "arm64-apple-darwin20.4"
, arch ? "arm64"
}:

let
  t = "${darwinCrossToolchain}/bin/${targetTriple}";
  sdkBase = "${appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  clang21 = llvmPackages_21.clang-unwrapped;
  lld21 = llvmPackages_21.lld;
  llvm21Dev = llvmPackages_21.libllvm.dev;
  hostSwift = swiftPackages.swift-unwrapped;

  # Portable arm64 targets Apple A10 (strict ARMv8.0-A, zero LSE/PAC/FP16).
  # arm64e targets Apple Silicon pointer authentication ABI.
  clangCpuFlags = lib.optionalString (arch == "arm64") "-mcpu=apple-a10";
  swiftCpuFlags = lib.optionalString (arch == "arm64") "-target-cpu apple-a10";
  swiftDriverCpuFlags = lib.optionalString (arch == "arm64") "-Xfrontend -target-cpu -Xfrontend apple-a10";

  # Portable arm64 links with LLVM 21 ld64.lld; arm64e links with cctools ld64
  # (nativeLd) because ld64.lld does not emit arm64e PAC chained fixups yet.
  useLd64 = arch == "arm64e";
  linkerBin = if useLd64 then "${nativeLd}/bin/ld" else "${lld21}/bin/ld64.lld";

  dylibNames = [
    "libswiftCore.dylib"
    "libswiftSwiftOnoneSupport.dylib"
    "libswift_Concurrency.dylib"
    "libswiftDarwin.dylib"
    "libswift_RegexParser.dylib"
    "libswift_StringProcessing.dylib"
    "libswiftRegexBuilder.dylib"
    "libswiftObservation.dylib"
    "libswiftDistributed.dylib"
  ];

  ninjaTargets = [
    "swiftCore-macosx"
    "swiftSwiftOnoneSupport-macosx"
    "swift_Concurrency-macosx"
    "swiftDarwin-macosx"
    "swift_RegexParser-macosx"
    "swift_StringProcessing-macosx"
    "swiftRegexBuilder-macosx"
    "swiftObservation-macosx"
    "swiftDistributed-macosx"
  ];

  klMeta = {
    name = "swift-runtime";
    version = swiftVersion;
    arch = arch;
    license = "Apache-2.0 WITH Swift-exception AND Apache-2.0 WITH LLVM-exception";
    summary = "Swift ${swiftVersion} standard library and core runtime libraries";
    depends = [ ];
    paths = [
      "usr/lib/swift"
      "usr/share/doc/kl/swift-runtime"
    ];
  };
in
stdenv.mkDerivation {
  pname = "kido-kl-swift-runtime-${arch}";
  version = swiftVersion;

  outputs = [ "out" "dev" ];

  nativeBuildInputs = [ cmake ninja python3 gawk ];

  dontUseCmakeConfigure = true;
  dontUseNinjaBuild = true;
  dontUseNinjaInstall = true;

  unpackPhase = ''
    runHook preUnpack
    mkdir -p "$TMPDIR/src"
    cp -r "${swiftSrc}" "$TMPDIR/src/swift"
    cp -r "${swiftStringProcessingSrc}" "$TMPDIR/src/swift-experimental-string-processing"
    chmod -R u+w "$TMPDIR/src"
    cd "$TMPDIR/src/swift"
    runHook postUnpack
  '';

  postPatch = ''
    # 1. Upstream LLVM 21 Clang supports __attribute__((swiftasynccall)) and
    #    generates identical swifttailcc codegen (clobbering x20), but does not
    #    advertise the Swift-fork-only __has_feature(swiftasynccc) probe.
    #    Enable swiftasynccall whenever __has_attribute(swiftasynccall) holds so
    #    Concurrency callers preserve autorelease pool tokens across async calls.
    substituteInPlace include/swift/Runtime/Config.h \
      --replace-fail '#if __has_feature(swiftasynccc) && __has_attribute(swiftasynccall)' \
                     '#if __has_attribute(swiftasynccall)'

    # 2. Allow arm64e alongside arm64 in the macOS SDK target list.
    substituteInPlace cmake/modules/DarwinSDKs.cmake \
      --replace-fail 'set(SUPPORTED_OSX_ARCHS "x86_64;arm64")' \
                     'set(SUPPORTED_OSX_ARCHS "x86_64;arm64;arm64e")'

    # 3. Cap _swift_stdlib_operatingSystemVersion() to macOS 14.5 (Swift 5.10
    #    ABI ceiling) so @backDeployed(before: macOS 15.0) thunks in Swift 6+
    #    binaries fall back cleanly instead of calling weak-imported macOS 15+
    #    runtime symbols that do not exist in Swift 5.10.1.
    ${python3}/bin/python3 - stdlib/public/stubs/Availability.mm <<'PY'
import sys
p = sys.argv[1]
txt = open(p, "r", encoding="utf-8").read()
old = "  os_system_version_get_current_version(&vers);\n  return vers;"
new = """  os_system_version_get_current_version(&vers);
  if (vers.major > 14 || (vers.major == 14 && vers.minor > 5)) {
    vers.major = 14;
    vers.minor = 5;
    vers.patch = 0;
  }
  return vers;"""
assert old in txt, "Availability.mm getOSVersion pattern not found"
open(p, "w", encoding="utf-8").write(txt.replace(old, new, 1))
PY

    # 4. On portable ARMv8.0-A (arm64 without PAC), Clang emits hint #7
    #    (xpaclri) for __builtin_return_address(0) on Darwin. Read the saved
    #    link register from the frame record instead so arm64 stays strictly
    #    within ARMv8.0-A (zero PAC instructions); arm64e keeps the builtin.
    ${python3}/bin/python3 - stdlib/public/runtime/Exclusivity.cpp <<'PY'
import sys
p = sys.argv[1]
txt = open(p, "r", encoding="utf-8").read()
old = "#elif __GNUC__\n#define get_return_address() __builtin_return_address(0)"
new = """#elif defined(__aarch64__) && !defined(__arm64e__) && !__has_feature(ptrauth_returns)
#define get_return_address() (((void **)__builtin_frame_address(0))[1])
#elif __GNUC__
#define get_return_address() __builtin_return_address(0)"""
assert old in txt, "Exclusivity.cpp get_return_address pattern not found"
open(p, "w", encoding="utf-8").write(txt.replace(old, new, 1))
PY
  '';

  configurePhase = ''
    runHook preConfigure

    # Stage a writable open-source Darwin SDK overlay combining KidoDarwin's
    # appleSdk, Foundation headers, and real KidoDarwin base dylibs.
    SDK="$TMPDIR/sdk/MacOSX.sdk"
    mkdir -p "$SDK/usr/include" "$SDK/usr/lib" "$SDK/System/Library/Frameworks"
    cp -rs "${sdkBase}/usr/include/." "$SDK/usr/include/"
    cp -rs "${sdkBase}/usr/lib/." "$SDK/usr/lib/"
    cp -rs "${sdkBase}/System/Library/Frameworks/." "$SDK/System/Library/Frameworks/"

    rm -rf "$SDK/System/Library/Frameworks/Foundation.framework"
    mkdir -p "$SDK/System/Library/Frameworks/Foundation.framework/Versions/A"
    ln -s "${foundation}/usr/include/Foundation" \
      "$SDK/System/Library/Frameworks/Foundation.framework/Versions/A/Headers"
    ln -s "${foundation}/usr/lib/libFoundation.dylib" \
      "$SDK/System/Library/Frameworks/Foundation.framework/Versions/A/Foundation"
    ln -s A "$SDK/System/Library/Frameworks/Foundation.framework/Versions/Current"
    ln -s Versions/Current/Headers "$SDK/System/Library/Frameworks/Foundation.framework/Headers"
    ln -s Versions/Current/Foundation "$SDK/System/Library/Frameworks/Foundation.framework/Foundation"

    ln -sf "${foundation}/usr/lib/libFoundation.dylib" "$SDK/usr/lib/libFoundation.dylib"
    ln -sf "${libobjc}/usr/lib/libobjc.A.dylib" "$SDK/usr/lib/libobjc.A.dylib"
    ln -sf "${libobjc}/usr/lib/libobjc.dylib" "$SDK/usr/lib/libobjc.dylib"
    ln -sf "${libcxxDylib}/usr/lib/libc++.1.dylib" "$SDK/usr/lib/libc++.1.dylib"
    ln -sf "${libcxxDylib}/usr/lib/libc++.dylib" "$SDK/usr/lib/libc++.dylib"
    ln -sf "${libcxxabiDylib}/usr/lib/libc++abi.dylib" "$SDK/usr/lib/libc++abi.dylib"
    ln -sf "${libSystem}/usr/lib/libSystem.B.dylib" "$SDK/usr/lib/libSystem.B.dylib"
    ln -sf "${libSystem}/usr/lib/libSystem.dylib" "$SDK/usr/lib/libSystem.dylib"

    cat > "$SDK/SDKSettings.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Version</key>
  <string>14.5</string>
  <key>CanonicalName</key>
  <string>macosx14.5</string>
</dict>
</plist>
EOF

    # Generate a clean-room Clang module.modulemap for module Darwin covering
    # the open-source C, POSIX, Mach, and os headers present in appleSdk.
    ${python3}/bin/python3 - "$SDK/usr/include" <<'PY'
import os, sys
inc = sys.argv[1]
groups = [
    ("C", [
        "assert.h", "ctype.h", "errno.h", "fenv.h", "float.h",
        "inttypes.h", "limits.h", "locale.h", "math.h", "setjmp.h",
        "signal.h", "stdarg.h", "stdbool.h", "stddef.h", "stdint.h",
        "stdio.h", "stdlib.h", "string.h", "time.h", "wchar.h", "wctype.h",
    ]),
    ("POSIX", [
        "dirent.h", "dlfcn.h", "fcntl.h", "fnmatch.h", "glob.h", "grp.h",
        "netdb.h", "poll.h", "pthread.h", "pwd.h", "regex.h", "sched.h",
        "semaphore.h", "spawn.h", "strings.h", "syslog.h", "termios.h",
        "unistd.h", "utime.h",
        "arpa/inet.h", "net/if.h", "netinet/in.h", "netinet/tcp.h",
        "sys/errno.h", "sys/fcntl.h", "sys/ioctl.h", "sys/mman.h",
        "sys/param.h", "sys/proc.h", "sys/resource.h", "sys/select.h",
        "sys/signal.h", "sys/socket.h", "sys/stat.h", "sys/sysctl.h",
        "sys/time.h", "sys/types.h", "sys/uio.h", "sys/un.h",
        "sys/utsname.h", "sys/wait.h",
    ]),
    ("Mach", ["mach/mach.h", "mach/kern_return.h"]),
    ("os", ["os/lock.h", "os/base.h"]),
    ("MacTypes", ["MacTypes.h"]),
    ("Availability", ["Availability.h", "TargetConditionals.h"]),
]
out = ["module Darwin [system] {"]
for name, hdrs in groups:
    out.append(f"  module {name} {{")
    for h in hdrs:
        if os.path.exists(os.path.join(inc, h)):
            out.append(f'    header "{h}"')
    out.append("    export *")
    out.append("  }")
out.append("  export *")
out.append("}")
with open(os.path.join(inc, "module.modulemap"), "w", encoding="utf-8") as fh:
    fh.write("\n".join(out) + "\n")
PY

    # Create host tool shims and compiler/swiftc wrappers.
    mkdir -p "$TMPDIR/bin" "$TMPDIR/swift-tools"
    ${lib.optionalString (arch == "arm64e") ''
      # Upstream Clang 21's builtin ptrauth.h omits Swift's __ptrauth_swift_*
      # discriminator macros; prepend Swift's open-source ptrauth.h via -isystem.
      mkdir -p "$TMPDIR/ptrauth-inc"
      ln -s "${hostSwift.lib}/lib/swift/clang/include/ptrauth.h" "$TMPDIR/ptrauth-inc/ptrauth.h"
    ''}

    cat > "$TMPDIR/bin/xcodebuild" <<'EOF'
#!${bash}/bin/bash
case "$*" in
  *ProductBuildVersion*) echo "25F74" ;;
  *) echo "Xcode 15.4"; echo "Build version 15F31d" ;;
esac
EOF
    chmod +x "$TMPDIR/bin/xcodebuild"

    cat > "$TMPDIR/bin/defaults" <<'EOF'
#!${bash}/bin/bash
echo "14.5"
EOF
    chmod +x "$TMPDIR/bin/defaults"

    cat > "$TMPDIR/bin/codesign" <<'EOF'
#!${bash}/bin/bash
exit 0
EOF
    chmod +x "$TMPDIR/bin/codesign"

    cat > "$TMPDIR/bin/cc-driver" <<EOF
#!${bash}/bin/bash
MODE=link
is_cxx=0
case "\$(basename "\$0")" in
  *++*) is_cxx=1 ;;
esac
for a in "\$@"; do
  case "\$a" in
    -c|-E|-S|-fsyntax-only) MODE=compile ;;
  esac
done
if [ "\$is_cxx" = 1 ]; then
  BIN="${clang21}/bin/clang++"
else
  BIN="${clang21}/bin/clang"
fi
if [ "\$MODE" = compile ]; then
  exec "\$BIN" \
    -target ${arch}-apple-macosx14.0 \
    ${clangCpuFlags} \
    -DXNU_PLATFORM_MacOSX=1 \
    -ffile-prefix-map="$TMPDIR/src=." \
    -fmacro-prefix-map="$TMPDIR/src=." \
    -ffile-prefix-map="$TMPDIR/build=." \
    -fmacro-prefix-map="$TMPDIR/build=." \
    ${lib.optionalString (arch == "arm64e") ''-isystem "$TMPDIR/ptrauth-inc" ''} -isysroot "$SDK" \
    -isystem "${libSystem}/usr/include" \
    -isystem "${libobjc}/usr/include" \
    -Wno-elaborated-enum-base \
    -Qunused-arguments \
    "\$@"
else
  # On arm64e (nativeLd / ld64-609), replace libswift_Concurrency.dylib link
  # inputs with an on-the-fly TAPI .tbd stub in $TMPDIR/tbd and prepend
  # -L"$TMPDIR/tbd" so both explicit inputs and LC_LINKER_OPTION -lswift_Concurrency
  # read exports via libtapi without hitting ld64-609's binary export-trie crash.
  mkdir -p "$TMPDIR/tbd"
  link_args=()
  prev_is_o=0
  for a in "\$@"; do
    if [ "\$prev_is_o" = 1 ]; then
      prev_is_o=0
      link_args+=("\$a")
      continue
    fi
    if [ "\$a" = "-o" ]; then
      prev_is_o=1
      link_args+=("\$a")
      continue
    fi
    case "\$a" in
      ${lib.optionalString useLd64 ''
      */libswift_Concurrency.dylib)
        if [ -f "\$a" ]; then
          tbd="$TMPDIR/tbd/libswift_Concurrency.tbd"
          if [ ! -f "\$tbd" ] || [ "\$a" -nt "\$tbd" ]; then
            "${t}-nm" -gUj "\$a" | "${python3}/bin/python3" -c '
import os, sys
tbd_path, install_name, ver = sys.argv[1], sys.argv[2], sys.argv[3]
ld_prefix = chr(36) + "ld" + chr(36)
sq = chr(39)
syms = [l.strip() for l in sys.stdin if l.strip() and not l.strip().startswith(ld_prefix)]
if not syms:
    sys.exit("empty symbol list for " + tbd_path)
q = lambda s: sq + s.replace(sq, sq + sq) + sq
lines = [
    "--- !tapi-tbd",
    "tbd-version: 4",
    "targets: [ arm64-macos, arm64e-macos ]",
    "install-name: " + q(install_name),
    f"current-version: {ver}",
    "compatibility-version: 1",
    "exports:",
    "  - targets: [ arm64-macos, arm64e-macos ]",
    "    symbols: [",
]
for i in range(0, len(syms), 8):
    chunk = ", ".join(q(s) for s in syms[i:i+8])
    lines.append("      " + chunk + ("," if i + 8 < len(syms) else ""))
lines.extend(["    ]", "..."])
tmp = f"{tbd_path}.{os.getpid()}.tmp"
with open(tmp, "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines) + "\n")
os.replace(tmp, tbd_path)
' "\$tbd" "/usr/lib/swift/libswift_Concurrency.dylib" "${swiftVersion}"
          fi
          link_args+=("-lswift_Concurrency")
          continue
        fi
        ;;
      ''}
      *) ;;
    esac
    link_args+=("\$a")
  done
  exec "\$BIN" \
    -target ${arch}-apple-macosx14.0 \
    ${clangCpuFlags} \
    -DXNU_PLATFORM_MacOSX=1 \
    -isysroot "$SDK" \
    --ld-path="${linkerBin}" \
    -nostdlib ${lib.optionalString useLd64 ''-L"$TMPDIR/tbd"''} \
    -L"${libSystem}/usr/lib" \
    -L"${libSystem}/usr/lib/system" \
    -L"${libobjc}/usr/lib" \
    -L"${libcxxDylib}/usr/lib" \
    -L"${libcxxabiDylib}/usr/lib" \
    -F"$SDK/System/Library/Frameworks" ${lib.optionalString useLd64 ''-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:"${libSystem}/usr/lib/system/libdyld.dylib"''} \
    -Wl,-platform_version,macos,26.5,26.5 \
    -Wl,-fixup_chains \
    -Wl,-dead_strip_dylibs \
    -Qunused-arguments \
    "\''${link_args[@]}" \
    -lc++ -lobjc -lSystem
fi
EOF
    chmod +x "$TMPDIR/bin/cc-driver"
    ln -sf cc-driver "$TMPDIR/bin/clang"
    ln -sf cc-driver "$TMPDIR/bin/clang++"

    for tool in swift swiftc swift-frontend swift-autolink-extract swift-api-digester swift-symbolgraph-extract; do
      cat > "$TMPDIR/swift-tools/$tool" <<EOF
#!${bash}/bin/bash
if [ "\''${1:-}" = "-frontend" ]; then
  shift
  exec -a "$tool" "${hostSwift}/bin/swift-frontend" -frontend \
    ${swiftCpuFlags} \
    -file-prefix-map "$TMPDIR/src=." \
    -file-prefix-map "$TMPDIR/build=." \
    -Xcc -DXNU_PLATFORM_MacOSX=1 \
    -Xcc -ffile-prefix-map="$TMPDIR/src=." \
    "\$@"
else
  exec -a "$tool" "${hostSwift}/bin/swift-frontend" \
    ${swiftDriverCpuFlags} \
    -Xfrontend -file-prefix-map -Xfrontend "$TMPDIR/src=." \
    -Xfrontend -file-prefix-map -Xfrontend "$TMPDIR/build=." \
    -Xcc -DXNU_PLATFORM_MacOSX=1 \
    -Xcc -ffile-prefix-map="$TMPDIR/src=." \
    "\$@"
fi
EOF
      chmod +x "$TMPDIR/swift-tools/$tool"
    done

    export PATH="$TMPDIR/bin:${darwinCrossToolchain}/bin:$PATH"
    mkdir -p "$TMPDIR/build"
    cd "$TMPDIR/build"

    cmake -GNinja "$TMPDIR/src/swift" \
      -DCMAKE_MAKE_PROGRAM="${ninja}/bin/ninja" \
      -DCMAKE_SYSTEM_NAME=Darwin \
      -DCMAKE_SYSTEM_PROCESSOR=arm64 \
      -DCMAKE_CROSSCOMPILING=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER="$TMPDIR/bin/clang" \
      -DCMAKE_CXX_COMPILER="$TMPDIR/bin/clang++" \
      -DCMAKE_C_COMPILER_WORKS=TRUE \
      -DCMAKE_CXX_COMPILER_WORKS=TRUE \
      -DCMAKE_ASM_COMPILER="$TMPDIR/bin/clang" \
      -DCMAKE_AR="${t}-ar" \
      -DCMAKE_RANLIB="${t}-ranlib" \
      -DSWIFT_LIPO="${t}-lipo" \
      -DCMAKE_INSTALL_NAME_TOOL="${t}-install_name_tool" \
      -DPython3_EXECUTABLE="${python3}/bin/python3" \
      -DLLVM_DIR="${llvm21Dev}/lib/cmake/llvm" \
      -DLLVM_BUILD_LIBRARY_DIR="${llvm21Dev}/lib" \
      -DLLVM_BUILD_BINARY_DIR="${llvm21Dev}" \
      -DLLVM_BUILD_MAIN_SRC_DIR="${llvm21Dev}" \
      -DLLVM_BUILD_MAIN_INCLUDE_DIR="${llvm21Dev}/include" \
      -DZLIB_LIBRARY="${libz}/usr/lib/libz.1.dylib" \
      -DZLIB_INCLUDE_DIR="${zlibDev}/include" \
      -DLIBXML2_LIBRARY="${libz}/usr/lib/libz.1.dylib" \
      -DLIBXML2_INCLUDE_DIR="${zlibDev}/include" \
      -DSWIFT_COMPILER_VERSION="${swiftVersion}" \
      -DSWIFT_INCLUDE_TOOLS=OFF \
      -DSWIFT_INCLUDE_TESTS=OFF \
      -DSWIFT_INCLUDE_DOCS=OFF \
      -DSWIFT_BUILD_STDLIB=ON \
      -DSWIFT_BUILD_DYNAMIC_STDLIB=ON \
      -DSWIFT_BUILD_STATIC_STDLIB=OFF \
      -DSWIFT_BUILD_SDK_OVERLAY=ON \
      -DSWIFT_BUILD_DYNAMIC_SDK_OVERLAY=ON \
      -DSWIFT_BUILD_STATIC_SDK_OVERLAY=OFF \
      -DSWIFT_BUILD_TEST_SUPPORT_MODULES=OFF \
      -DSWIFT_BUILD_REMOTE_MIRROR=OFF \
      -DSWIFT_BUILD_LIBEXEC=OFF \
      -DSWIFT_BUILD_RUNTIME_WITH_HOST_COMPILER=ON \
      -DSWIFT_NATIVE_SWIFT_TOOLS_PATH="$TMPDIR/swift-tools" \
      -DSWIFT_NATIVE_CLANG_TOOLS_PATH="$TMPDIR/bin" \
      -DSWIFT_HOST_VARIANT_SDK=OSX \
      -DSWIFT_HOST_VARIANT_ARCH="${arch}" \
      -DSWIFT_PRIMARY_VARIANT_SDK=OSX \
      -DSWIFT_PRIMARY_VARIANT_ARCH="${arch}" \
      -DSWIFT_SDKS=OSX \
      -DSWIFT_DARWIN_SUPPORTED_ARCHS="${arch}" \
      -DSWIFT_SDK_OSX_PATH="$SDK" \
      -DSWIFT_DARWIN_DEPLOYMENT_VERSION_OSX=14.0 \
      -DSWIFT_DARWIN_STDLIB_INSTALL_NAME_DIR=/usr/lib/swift \
      -DSWIFT_STDLIB_ENABLE_OBJC_INTEROP=ON \
      -DSWIFT_STDLIB_TRACING=OFF \
      -DSWIFT_STDLIB_CONCURRENCY_TRACING=OFF \
      -DSWIFT_STDLIB_HAS_ASL=OFF \
      -DSWIFT_ENABLE_EXPERIMENTAL_CONCURRENCY=ON \
      -DSWIFT_ENABLE_EXPERIMENTAL_DISTRIBUTED=ON \
      -DSWIFT_ENABLE_EXPERIMENTAL_DIFFERENTIABLE_PROGRAMMING=OFF \
      -DSWIFT_ENABLE_EXPERIMENTAL_STRING_PROCESSING=ON \
      -DSWIFT_PATH_TO_STRING_PROCESSING_SOURCE="$TMPDIR/src/swift-experimental-string-processing" \
      -DSWIFT_ENABLE_EXPERIMENTAL_OBSERVATION=ON \
      -DSWIFT_ENABLE_EXPERIMENTAL_CXX_INTEROP=OFF \
      -DSWIFT_ENABLE_BACKTRACING=OFF \
      -DSWIFT_ENABLE_DISPATCH=OFF \
      -DSWIFT_ENABLE_MACCATALYST=OFF \
      -DSWIFT_ENABLE_MODULE_INTERFACES=OFF

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    export PATH="$TMPDIR/bin:${darwinCrossToolchain}/bin:$PATH"
    cd "$TMPDIR/build"
    ninja -j"$NIX_BUILD_CORES" ${lib.concatStringsSep " " ninjaTargets}

    # Verify core runtime exports across the primary dylibs.
    archLib="$TMPDIR/build/lib/swift/macosx/${arch}"
    "${t}-nm" -gUj "$archLib/libswiftCore.dylib" > core.syms
    for sym in \
      _OBJC_CLASS_\$__TtCs12_SwiftObject \
      _swift_allocObject \
      _swift_retain \
      _swift_release \
      _swift_dynamicCast \
      _swift_conformsToProtocol2 \
      _swift_beginAccess \
      _swift_endAccess \
      _swift_once \
      _\$sSSMn; do
      grep -Fxq "$sym" core.syms || { echo "missing expected libswiftCore symbol $sym" >&2; exit 1; }
    done

    "${t}-nm" -gUj "$archLib/libswift_Concurrency.dylib" > conc.syms
    for sym in \
      _swift_task_create \
      _swift_task_switch \
      _swift_job_run \
      _swift_task_asyncMainDrainQueue; do
      grep -Fxq "$sym" conc.syms || { echo "missing expected libswift_Concurrency symbol $sym" >&2; exit 1; }
    done

    "${t}-nm" -gUj "$archLib/libswiftDarwin.dylib" > darwin.syms
    for sym in \
      '_$s6Darwin5noErrs5Int32Vvg' \
      '_$s6Darwin10MAP_FAILEDSvSgvg'; do
      grep -Fxq "$sym" darwin.syms || { echo "missing expected libswiftDarwin symbol $sym" >&2; exit 1; }
    done
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    archLib="$TMPDIR/build/lib/swift/macosx/${arch}"
    mkdir -p "$out/usr/lib/swift" "$out/usr/share/doc/kl/swift-runtime"

    # Install thin per-architecture dylibs (avoid lipo fat wrappers).
    for d in ${lib.concatStringsSep " " dylibNames}; do
      install -Dm755 "$archLib/$d" "$out/usr/lib/swift/$d"
      if LC_ALL=C grep -qF '/nix/store/' "$out/usr/lib/swift/$d"; then
        echo "ERROR: /nix/store/ path leaked into $d" >&2
        exit 1
      fi
    done

    install -Dm644 "$TMPDIR/src/swift/LICENSE.txt" "$out/usr/share/doc/kl/swift-runtime/LICENSE"
    cat > "$out/usr/share/doc/kl/swift-runtime/CHANGES" <<'EOF'
KidoDarwin packaging changes applied to Swift ${swiftVersion} standard library:
- Enabled __attribute__((swiftasynccall)) in include/swift/Runtime/Config.h
  without requiring the Swift-fork-only __has_feature(swiftasynccc) probe so
  upstream LLVM 21 Clang preserves the swifttailcc ABI across async calls.
- Added arm64e to SUPPORTED_OSX_ARCHS in cmake/modules/DarwinSDKs.cmake.
- Capped _swift_stdlib_operatingSystemVersion() in Availability.mm to macOS
  14.5 (Swift 5.10 ABI ceiling) so @backDeployed(before: macOS 15.0) thunks in
  Swift 6+ binaries fall back cleanly on KidoDarwin.
- Read the saved return address from the frame record in Exclusivity.cpp on
  non-arm64e AArch64 so portable arm64 binaries remain strictly ARMv8.0-A.
EOF
    chmod 644 "$out/usr/share/doc/kl/swift-runtime/CHANGES"

    # Populate dev output with compiled Swift modules and dylib links.
    mkdir -p "$dev/lib/swift/macosx"
    for mod in "$TMPDIR/build/lib/swift/macosx/"*.swiftmodule; do
      [ -d "$mod" ] && cp -a "$mod" "$dev/lib/swift/macosx/"
    done
    for d in ${lib.concatStringsSep " " dylibNames}; do
      ln -s "$out/usr/lib/swift/$d" "$dev/lib/swift/macosx/$d"
    done

    "${bash}/bin/bash" ${./check-gates.sh} \
      --root "$out" \
      --pkg-name swift-runtime \
      --arch "${arch}" \
      --license "${klMeta.license}" \
      --base-dylibs ${./base-dylibs.txt} \
      --otool "${t}-otool" \
      --objdump "${t}-objdump"
    runHook postInstall
  '';

  dontFixup = true;

  passthru = {
    kl = klMeta;
  };

  meta = with lib; {
    description = "kl system package for Swift ${swiftVersion} runtime libraries (${arch})";
    license = licenses.asl20;
    platforms = platforms.unix;
  };
}
