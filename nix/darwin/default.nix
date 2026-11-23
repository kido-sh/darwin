# The Darwin tree's builds for arm64: the source sets, the CMake build (build.nix), libSystem, the
# kernel, the kexts, the frameworks, the userland and the third-party packages.
{ toolchain }:

let
  inherit (toolchain)
    pkgs
    corecryptoSrc
    appleSdk
    iig
    nativeLd
    darwinCrossToolchain
    nativeUnifdef
    nativeMigcom
    compilerRt
    ;
  lib = pkgs.lib;
  # Component source trees (see sources.nix).
  sources = import ./sources.nix {
    inherit pkgs sourceWith sourceWithExtraFilter frameworkHeadersOnly libSystemSourcePaths;
  };
  inherit (sources)
    coreFoundationSource
    coreServicesSource
    foundationSource
    iokitCFSource
    kernelSource
    kextsOnlySource
    libSystemSource
    libcxxDylibSource
    objcSource
    securitySource
    symptomReporterSource
    systemConfigurationSource
    userlandSource
    xnuHeadersSource
    ;
  sourceWithExtraFilter = name: prefixes: extraFilter:
    lib.cleanSourceWith {
      src = ../../darwin;
      filter = path: type:
        let
          rel = lib.removePrefix "${toString ../../darwin}/" (toString path);
          isParentOfPrefix = prefix:
            lib.hasPrefix "${rel}/" prefix;
        in
          extraFilter rel type
          && (rel == "CMakeLists.txt"
          || rel == "src/CMakeLists.txt"
          || rel == "cmake"
          || lib.hasPrefix "cmake/" rel
          || lib.any (prefix:
            rel == prefix
            || lib.hasPrefix "${prefix}/" rel
            || (type == "directory" && isParentOfPrefix prefix)
          ) prefixes);
    };
  sourceWith = name: prefixes:
    sourceWithExtraFilter name prefixes (rel: type: true);
  # The top-level CMakeLists.txt globs *.h out of the CoreFoundation and
  # Foundation trees for a header search path, and nothing built from
  # libSystemSource compiles their sources. Dropping the implementation
  # files keeps a .m edit in either framework from invalidating
  # libSystem - and so from rebuilding LLVM, which depends on it.
  frameworkHeadersOnly = rel: type:
    !(type == "regular"
      && (lib.hasPrefix "src/Frameworks/CoreFoundation/" rel
          || lib.hasPrefix "src/Frameworks/Foundation/" rel)
      && !(lib.hasSuffix ".h" rel));
  libSystemSourcePaths = [
    "src/Kernel/xnu/EXTERNAL_HEADERS"
    "src/Kernel/xnu/osfmk"
    "src/Kernel/xnu/libkern/libkern"
    "src/Kernel/xnu/libkern/libkern/arm"
    "src/Kernel/xnu/libkern/os"
    # os/log_private.h includes <firehose/tracepoint_private.h>
    "src/Kernel/xnu/libkern/firehose"
    "src/Kernel/xnu/bsd/arm"
    "src/Kernel/xnu/bsd/i386"
    "src/Kernel/xnu/bsd/bsm"
    "src/Kernel/xnu/bsd/machine"
    "src/Kernel/xnu/bsd/net"
    "src/Kernel/xnu/bsd/netinet"
    "src/Kernel/xnu/bsd/netinet6"
    "src/Kernel/xnu/bsd/pthread"
    "src/Kernel/xnu/bsd/sys"
    "src/Kernel/xnu/bsd/sys_private"
    "src/Kernel/xnu/bsd/uuid"
    "src/Kernel/xnu/bsd/kern/makesyscalls.sh"
    "src/Kernel/xnu/bsd/kern/syscalls.master"
    "src/Frameworks/CoreFoundation"
    "src/Frameworks/Foundation"
    "src/Libraries"
    "src/Libraries/libSystem/libmalloc/compat-include"
    "tools/mig"
    # libobjc needs the mach-o getsection helpers compiled into libSystem.
    "tools/cctools/libmacho/getsecbyname.c"
    # NXGetArchInfo* is part of libSystem on Darwin, same libmacho source.
    "tools/cctools/libmacho/arch.c"
    # mach-o/utils.h (macOS 13) arch-name helpers, over that same table.
    "tools/cctools/libmacho/utils.c"
    "tools/cctools/include/mach-o/utils.h"
    "tools/cctools/include/mach-o/arch.h"
    "tools/cctools/include/stuff/openstep_mach.h"
    "tools/cctools/include/mach/machine.h"
  ];
  mkPureDarwinBuild = args: pkgs.callPackage ./build.nix ({
    inherit appleSdk darwinCrossToolchain nativeLd nativeUnifdef nativeMigcom iig compilerRt;
    # the kernel builds its corecrypto from kernelSource, keep its hash independent of this copy
    corecryptoSrc = if lib.hasPrefix "kido-kernel" (args.pname or "") || lib.hasPrefix "kido-xnu-headers" (args.pname or "")
      then null else corecryptoSrc;
  } // args);
  # a recipe under apple/ or third-party/, with the toolchain, libSystem and the C++ runtime it asks for
  mkArm64Build = file: deps:
    let
      f = import file;
      common = {
        inherit darwinCrossToolchain nativeLd;
        libSystem = libSystemArm64Build;
        # c++ recipes that link with -nostdlib take the runtime explicitly
        libcxxDylib = libcxxDylibArm64Build;
        libcxxabiDylib = libcxxabiDylibArm64Build;
      };
    in
    pkgs.callPackage f (builtins.intersectAttrs (builtins.functionArgs f) common // deps);

  libSystemArm64Build = mkPureDarwinBuild {
    pname = "kido-libsystem";
    src = libSystemSource;
    buildTargets = [ "libSystem_B_stub" "libDER_static" "dyld" "libsystem_kernel_static" "libdispatch_static" "XPC_libnv_static" "XPC_libinfo_static" "XPC_libxpc_static" "XPC_launchd_static" "XPC_launchd_mig_static" "XPC_notify_client_static" "notifyd" "logd" ];
    enableUserspace = false;
    enableKernel = false;
    installUserland = false;
    installKernel = false;
    installLibSystem = true;
  };
  libcxxabiDylibArm64Build = mkArm64Build ./apple/libcxxabi-dylib.nix { src = libcxxDylibSource; };
  libcxxDylibArm64Build = mkArm64Build ./apple/libcxx-dylib.nix { src = libcxxDylibSource; };
  icuCoreArm64Build = mkArm64Build ./apple/icucore.nix { icuSrc = pkgs.icu.src; };
  libobjcArm64Build = mkArm64Build ./apple/libobjc.nix { src = objcSource; };
  coreFoundationArm64Build = mkArm64Build ./apple/corefoundation.nix {
    icu = icuCoreArm64Build;
    src = "${coreFoundationSource}/src/Frameworks/CoreFoundation";
    pdCompatInclude = "${coreFoundationSource}/src/Libraries/libSystem/libc/pd-compat-include";
    libobjc = libobjcArm64Build;
    foundationSrc = "${foundationSource}/src/Frameworks/Foundation";
  };
  foundationArm64Build = mkArm64Build ./apple/foundation.nix {
    libobjc = libobjcArm64Build;
    corefoundation = coreFoundationArm64Build;
    libffi = libffiArm64Build;
    # NSXML is implemented over libxml2, as Apple's is.
    libxml2 = libxml2Arm64Build;
    # libSystem exports the DNSService* API but installs no header.
    dnssdInclude = ../../darwin/src/Libraries/mDNSResponder/mDNSShared;
    # Likewise notify_post/notify_register_dispatch.
    notifyInclude = ../../darwin/src/Libraries/XPC/notify;
    src = "${foundationSource}/src/Frameworks/Foundation";
  };
  # IOKitUser's CoreFoundation-based IOKitLib. Built apart from libSystem
  # because it links CoreFoundation, which itself links libSystem.
  iokitCFStaticArm64Build = (mkPureDarwinBuild {
      pname = "kido-iokitcf-static";
      src = iokitCFSource;
      buildTargets = [ "IOKitCF" ];
      enableProjects = false;
      enableKernel = false;
      enableUserspace = false;
      installUserland = false;
      installKernel = false;
      extraCmakeFlags = [
        "-DPUREDARWIN_ENABLE_IOKITCF=ON"
        "-DPUREDARWIN_COREFOUNDATION_PREFIX=${coreFoundationArm64Build}"
        "-DPUREDARWIN_LIBOBJC_PREFIX=${libobjcArm64Build}"
        "-DPUREDARWIN_FOUNDATION_PREFIX=${foundationArm64Build}"
      ];
    }).overrideAttrs (old: {
      installPhase = ''
        runHook preInstall
        mkdir -p "$out/usr/lib/system"
        cp build-nix/src/Libraries/IOKit/libIOKitCF.a "$out/usr/lib/system/"
        mkdir -p "$out/include"
        cp -a src/Libraries/IOKit/iokituser/include/IOKit "$out/include/"
        runHook postInstall
      '';
    });
  iokitArm64Build = mkArm64Build ./apple/iokit.nix {
    corefoundation = coreFoundationArm64Build;
    iokitCFStatic = iokitCFStaticArm64Build;
    # IOKitCF's hid.subproj has ObjC (HIDDeviceBase.m et al) subclassing NSObject
    libobjc = libobjcArm64Build;
    foundation = foundationArm64Build;
  };
  launchdArm64Build = mkArm64Build ./apple/launchd.nix {
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
    src = libSystemSource;
  };
  launchctlArm64Build = mkArm64Build ./apple/launchctl.nix {
    corefoundation = coreFoundationArm64Build;
    iokit = iokitArm64Build;
    src = libSystemSource;
  };
  libzDylibArm64Build = mkArm64Build ./third-party/libz-dylib.nix {
    inherit (pkgs) zlib;
  };
  coreServicesArm64Build = mkArm64Build ./apple/coreservices.nix {
    src = coreServicesSource;
    corefoundation = coreFoundationArm64Build;
  };
  opensslArm64Build = mkArm64Build ./third-party/openssl.nix { };
  opensslThreadsArm64Build = opensslArm64Build.override { withThreads = true; };
  securityArm64Build = mkArm64Build ./apple/security.nix {
    pname = "kido-security-arm64";
    inherit mkPureDarwinBuild;
    corefoundation = coreFoundationArm64Build;
    libobjc = libobjcArm64Build;
    foundation = foundationArm64Build;
    sqlite = sqliteArm64Build;
    src = securitySource;
    openssl = opensslThreadsArm64Build;
    libz = libzDylibArm64Build;
  };
  # Apple's SymptomReporter is a private framework with no source and
  # no SDK header; this is PureDarwin's own, recording symptoms through
  # os_log. bootp's IPConfiguration plugin is the consumer.
  symptomReporterArm64Build =
    (mkPureDarwinBuild {
      pname = "kido-symptomreporter";
      src = symptomReporterSource;
      buildTargets = [ "SymptomReporter" ];
      enableProjects = false;
      enableKernel = false;
      enableUserspace = false;
      installUserland = false;
      installKernel = false;
      extraCmakeFlags = [
        "-DPUREDARWIN_ENABLE_SYMPTOMREPORTER=ON"
      ];
    }).overrideAttrs (old: {
      installPhase = ''
        runHook preInstall
        fw="$out/System/Library/PrivateFrameworks/SymptomReporter.framework"
        mkdir -p "$fw/Versions/A/Headers"
        cp build-nix/src/Libraries/SymptomReporter/libSymptomReporter.dylib \
          "$fw/Versions/A/SymptomReporter"
        cp -a src/Libraries/SymptomReporter/include/SymptomReporter/. \
          "$fw/Versions/A/Headers/"
        ln -s A "$fw/Versions/Current"
        ln -s Versions/Current/SymptomReporter "$fw/SymptomReporter"
        ln -s Versions/Current/Headers "$fw/Headers"
        mkdir -p "$out/include"
        cp -a src/Libraries/SymptomReporter/include/SymptomReporter "$out/include/"
        runHook postInstall
      '';
    });
  systemConfigurationArm64Build = (mkPureDarwinBuild {
      pname = "kido-systemconfiguration";
      src = systemConfigurationSource;
      buildTargets = [ "SystemConfiguration" "configd" "bootplib_static" "ipconfiguration_static" "ipconfig" ];
      enableProjects = false;
      enableKernel = false;
      enableUserspace = false;
      installUserland = false;
      installKernel = false;
      extraCmakeFlags = [
        "-DPUREDARWIN_ENABLE_SYSTEMCONFIGURATION=ON"
        "-DPUREDARWIN_COREFOUNDATION_PREFIX=${coreFoundationArm64Build}"
        "-DPUREDARWIN_LIBOBJC_PREFIX=${libobjcArm64Build}"
        "-DPUREDARWIN_SECURITY_PREFIX=${securityArm64Build}"
        # InterfaceNamer drives the interface-naming user client through
        # IOKitUser's CF-based IOKitLib.
        "-DPUREDARWIN_IOKIT_PREFIX=${iokitArm64Build}"
        "-DPUREDARWIN_SYMPTOMREPORTER_PREFIX=${symptomReporterArm64Build}"
      ];
    }).overrideAttrs (old: {
      installPhase = ''
        runHook preInstall
        fw="$out/System/Library/Frameworks/SystemConfiguration.framework"
        mkdir -p "$fw/Versions/A/Headers"
        cp build-nix/src/Libraries/SystemConfiguration/libSystemConfiguration.dylib \
          "$fw/Versions/A/SystemConfiguration"
        cp -a src/Libraries/SystemConfiguration/include/SystemConfiguration/. \
          "$fw/Versions/A/Headers/"
        # Resources the framework looks up at runtime: the localized
        # interface-name strings and NetworkConfiguration.plist, which
        # InterfaceNamer consults while naming. Without them every
        # lookup logs "failed to get resource url" and falls back to an
        # empty strings table.
        mkdir -p "$fw/Versions/A/Resources"
        cp src/Libraries/SystemConfiguration/SystemConfiguration.fproj/NetworkConfiguration.plist \
          "$fw/Versions/A/Resources/"
        # CFBundleGetBundleWithIdentifier needs the identifier from Info.plist
        cp src/Libraries/SystemConfiguration/SystemConfiguration.fproj/Info.plist \
          "$fw/Versions/A/Resources/"
        cp -a src/Libraries/SystemConfiguration/SystemConfiguration.fproj/en.lproj \
          "$fw/Versions/A/Resources/"
        ln -s A "$fw/Versions/Current"
        ln -s Versions/Current/SystemConfiguration "$fw/SystemConfiguration"
        ln -s Versions/Current/Headers "$fw/Headers"
        ln -s Versions/Current/Resources "$fw/Resources"
        # Flat dylib alias, as Security.framework and OpenGL have.
        mkdir -p "$out/usr/lib"
        ln -s "../../System/Library/Frameworks/SystemConfiguration.framework/Versions/A/SystemConfiguration" \
          "$out/usr/lib/libSystemConfiguration.dylib"
        mkdir -p "$out/include"
        cp -a src/Libraries/SystemConfiguration/include/SystemConfiguration "$out/include/"
        # configd, the SCDynamicStore server, plus its launchd job.
        mkdir -p "$out/usr/libexec" "$out/System/Library/LaunchDaemons"
        cp build-nix/src/Libraries/SystemConfiguration/configd.tproj/configd \
          "$out/usr/libexec/configd"
        cp src/Libraries/SystemConfiguration/configd.tproj/com.apple.configd.plist \
          "$out/System/Library/LaunchDaemons/"
        # Builtin plugins live inside configd, but plugin_support.c still
        # discovers them by walking /System/Library/SystemConfiguration
        # for bundles, so each one needs its Info.plist installed.
        # IPConfiguration is builtin too, but its sources live in the
        # bootp project rather than under Plugins/, and its plist is
        # generated by src/Libraries/bootp/CMakeLists.txt.
        for p in PreferencesMonitor LinkConfiguration KernelEventMonitor IPMonitor InterfaceNamer; do
          d="$out/System/Library/SystemConfiguration/$p.bundle/Contents"
          mkdir -p "$d"
          # The build-dir copy, not the source one: these plists set
          # CFBundleIdentifier to Xcode's $(PRODUCT_BUNDLE_IDENTIFIER),
          # and configd skips any bundle whose identifier does not
          # resolve. The build substitutes it; the source cannot.
          cp "build-nix/src/Libraries/SystemConfiguration/configd.tproj/plugin-plists/$p/Info.plist" "$d/"
        done
        d="$out/System/Library/SystemConfiguration/IPConfiguration.bundle/Contents"
        mkdir -p "$d"
        cp "build-nix/src/Libraries/bootp/plugin-plists/IPConfiguration/Info.plist" "$d/"
        # ipconfig(8): SystemStarter runs `ipconfig waitall` to block
        # until every interface has finished configuring.
        mkdir -p "$out/usr/sbin"
        cp build-nix/src/Libraries/bootp/ipconfig "$out/usr/sbin/ipconfig"
        runHook postInstall
      '';
    });
  certificatesBundleBuild =
    pkgs.callPackage ./apple/certificates-bundle.nix {
      src = ../../darwin/src/Libraries/Security/certificates;
    };

  xvfbZlibArm64Build = mkArm64Build ./third-party/xvfb-zlib.nix { inherit (pkgs) zlib; };
  toyboxArm64Build = mkArm64Build ./third-party/toybox.nix { zlib = xvfbZlibArm64Build; };
  sqliteArm64Build = mkArm64Build ./third-party/xorg-cross-lib.nix {
    pname = "kido-sqlite";
    inherit (pkgs.sqlite) version;
    # nixpkgs' sqlite src is a .zip the default unpacker cannot read.
    src = pkgs.runCommand "sqlite-src-${pkgs.sqlite.version}" { nativeBuildInputs = [ pkgs.unzip ]; } ''
      unzip -q ${pkgs.sqlite.src}
      mkdir -p $out
      cp -R sqlite-src-*/. $out/
      chmod -R u+w $out
    '';
    configureFlags = [ "--disable-readline" "--disable-editline" ];
  };
  libffiArm64Build = mkArm64Build ./third-party/xorg-cross-lib.nix {
    pname = "kido-libffi";
    version = pkgs.libffiReal.version;
    src = pkgs.libffiReal.src;
    configureFlags = [
      "--disable-docs"
      "--disable-multi-os-directory"
    ];
  };
  libxml2Arm64Build = mkArm64Build ./third-party/libxml2.nix {
    inherit (pkgs) libxml2 meson ninja python3 git;
  };
  libiconvArm64Build = mkArm64Build ./third-party/libiconv.nix { libiconvReal = pkgs.libiconvReal; };
  ncursesArm64Build = mkArm64Build ./third-party/ncurses.nix { ncurses = pkgs.ncurses; };
  ncursesDylibArm64Build = mkArm64Build ./third-party/ncurses-dylib { ncurses = pkgs.ncurses; };
  libeditArm64Build = mkArm64Build ./third-party/libedit {
    inherit (pkgs) libedit;
    ncurses = ncursesDylibArm64Build;
  };
  zshArm64Build = mkArm64Build ./third-party/zsh.nix {
    zsh = pkgs.zsh;
    ncurses = ncursesArm64Build;
    libiconv = libiconvArm64Build;
  };
  userlandArm64Build = mkPureDarwinBuild {
    pname = "kido-userland";
    src = userlandSource;
    buildTargets = [ "sw_vers" "doubleagentd" "ps" "killall" "pkill" "mkfile" "sync" "sysctl" "vm_stat" "hostinfo" "dmesg" "purge" "reboot" "halt" "poweroff" "shutdown" "netsetup" "ping" "mount" "umount" "ext4tool" "ext4_util" "mdnsd" "kido-dmesg-save" "sh" "gpt" ]
      # shell_cmds (+ tsort/uuencode/uudecode)
      ++ [ "basename" "chown" "dirname" "echo" "false" "getopt" "hostname" "jot" "kill" "logname" "mktemp" "nice" "nohup" "passwd" "printenv" "pwd" "renice" "seq" "shlock" "sleep" "tee" "test_cmd" "true" "tsort" "uname" "yes" "uuencode" "uudecode" ]
      # text_cmds
      ++ [ "banner" "cat" "colrm" "comm" "cut" "expand" "fold" "head" "lam" "look" "nl" "paste" "rev" "split" "tail" "tr" "unexpand" "uniq" "wc" ];
    enableProjects = false;
    enableKernel = false;
    enableLibraries = false;
    enableTools = false;
    installUserland = true;
    installKernel = false;
    prebuiltLibSystem = libSystemArm64Build;
  };

  # the kasan and kcov denylists name .c files the headers source leaves out
  xnuHeadersPostPatch = old: {
    postPatch = (old.postPatch or "") + ''
      printf '#!/bin/sh\nexit 0\n' > src/Kernel/xnu/san/tools/validate_denylist.sh
    '';
  };
  xnuHeadersArm64Build = (mkPureDarwinBuild {
    pname = "kido-xnu-headers-arm64";
    src = xnuHeadersSource;
    buildTargets = [ "xnu_headers.extproj" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = false;
    installXnuHeaders = true;
    xnuKernelConfig = "RELEASE";
  }).overrideAttrs xnuHeadersPostPatch;
  # kext builds configure only the kexts and take the xnu headers prebuilt
  kextsOnlyArgs = xnuHeaders: {
    enableTools = false;
    extraCmakeFlags = [
      "-DPUREDARWIN_KEXTS_ONLY=ON"
      "-DPUREDARWIN_PREBUILT_XNU_HEADERS=${xnuHeaders}"
    ];
  };
  kernelArm64VirtBuild = mkPureDarwinBuild {
    pname = "kido-kernel-arm64-virt";
    src = kernelSource;
    buildTargets = [ "xnu" ];
    enableUserspace = false;
    installUserland = false;
    installKernel = true;
    xnuKernelConfig = "RELEASE";
    extraCmakeFlags = [ "-DPUREDARWIN_ARM64_MACHINE_CONFIG=VIRT" ];
  };
  kextsArm64Build = mkPureDarwinBuild ({
    pname = "kido-kexts-arm64";
    src = kextsOnlySource;
    buildTargets = [
      "PDArmPlatformExpert" "PDArmPCI" "PDEcamPCI" "IOPCIFamily" "IOStorageFamily"
      "IOVirtIOFamily.kext" "IOVirtIONet.kext" "IOVirtIOBlock.kext" "IONetworkingFamily.kext"
      "corecrypto.kext" "ext4.kext" "AppleFileSystemDriver.kext" "Ext4FileSystemDriver.kext"
      "pthread.kext" "amfi.kext" "Sandbox.kext" "PDWatchdog.kext" "PDDiskImages2.kext"
      "PDLifs.kext" "PDKeyStore.kext"
    ];
    enableUserspace = false;
    installUserland = false;
    installKernel = false;
    installKexts = true;
    installKextNames = [
      "PDArmPlatformExpert.kext" "PDArmPCI.kext" "PDEcamPCI.kext" "IOPCIFamily.kext"
      "IOStorageFamily.kext" "IOVirtIOFamily.kext" "IOVirtIONet.kext" "IOVirtIOBlock.kext"
      "IONetworkingFamily.kext" "corecrypto.kext" "ext4.kext" "AppleFileSystemDriver.kext"
      "Ext4FileSystemDriver.kext" "pthread.kext" "amfi.kext" "Sandbox.kext" "PDWatchdog.kext"
      "PDDiskImages2.kext" "PDLifs.kext" "PDKeyStore.kext"
    ];
  } // kextsOnlyArgs xnuHeadersArm64Build);
  splitBaseSystemArm64VirtMinimalRelease = pkgs.runCommand "kido-basesystem-arm64-virt-minimal-release-0.1" { } ''
    mkdir -p "$out"
    cp -a ${libSystemArm64Build}/. "$out/"
    chmod -R u+w "$out"
    # Keep the release image's runtime closure identical to debug.
    cp -a ${icuCoreArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libcxxabiDylibArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libcxxDylibArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${libobjcArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${coreFoundationArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${foundationArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${iokitArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${launchdArm64Build}/. "$out/"
    chmod -R u+w "$out"
    cp -a ${launchctlArm64Build}/. "$out/"
    chmod -R u+w "$out"
    if [ -e "$out/pd-sbin/launchd" ]; then
      mkdir -p "$out/sbin"
      cp "$out/pd-sbin/launchd" "$out/sbin/launchd"
      rm -rf "$out/pd-sbin"
    fi
    cp -a ${userlandArm64Build}/. "$out/"
    chmod -R u+w "$out"
  '';

in
{
  inherit
    kernelSource
    certificatesBundleBuild
    coreServicesArm64Build
    kernelArm64VirtBuild
    kextsArm64Build
    libeditArm64Build
    libiconvArm64Build
    libzDylibArm64Build
    ncursesDylibArm64Build
    securityArm64Build
    splitBaseSystemArm64VirtMinimalRelease
    systemConfigurationArm64Build
    toyboxArm64Build
    zshArm64Build
    ;
}
