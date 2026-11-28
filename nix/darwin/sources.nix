# Source trees for the components built from this repository, plus the
# upstream tarballs that need patching before use.
#
# sourceWith filters the repo down to just the prefixes a component needs, so
# editing an unrelated file does not invalidate its build.
{ frameworkHeadersOnly
, libSystemSourcePaths
, pkgs
, sourceWith
, sourceWithExtraFilter
}:

let
  # Every consumer that pulls in src/Kernel/xnu/osfmk (directly, or via
  # libSystemSourcePaths) uses it purely as a header search path (-I), never
  # as compiled sources - grep confirms no CMakeLists.txt under src/Libraries
  # or src/Userspace adds osfmk/*.c to target_sources (only kernelSource and
  # kextsSource, which are left untouched, actually compile it). Stripping
  # implementation files from osfmk here means an unrelated kernel-only
  # change (e.g. osfmk/ipc/ipc_policy.c) doesn't invalidate every downstream
  # userland derivation that transitively pulls in one of these sources.
  stripOsfmkImpl = src:
    pkgs.lib.cleanSourceWith {
      inherit src;
      filter = path: type:
        let
          # nested filters are handed paths under the original root, so strip that prefix
          rel = pkgs.lib.removePrefix "${toString src.origSrc}/" (toString path);
          isOsfmkImpl =
            type == "regular"
            && pkgs.lib.hasPrefix "src/Kernel/xnu/osfmk/" rel
            && pkgs.lib.any (ext: pkgs.lib.hasSuffix ext rel) [ ".c" ".cpp" ".cc" ".m" ".mm" ".s" ".S" ];
        in
          !isOsfmkImpl;
    };
  kernelSource = sourceWith "kido-kernel-source" [
    # osfmk/conf/files compiles the kernel's built-in corecrypto from it
    "external/corecrypto"
    "src/Kernel/CMakeLists.txt"
    "src/Kernel/xnu"
    "src/Kernel/libfirehose_kernel"
    "src/Libraries/CMakeLists.txt"
    "src/Libraries/AvailabilityVersions"
    "src/Libraries/libSystem/CMakeLists.txt"
    "src/Libraries/libSystem/libplatform"
    "src/Libraries/libSystem/pthread"
    "tools"
  ];
  libSystemSource = stripOsfmkImpl (sourceWithExtraFilter "kido-libsystem-source" libSystemSourcePaths frameworkHeadersOnly);
  fs = pkgs.lib.fileset;
  headersIn = dir: fs.fileFilter (f: f.hasExt "h") dir;
  # xnu without the .c, .cpp and .s files its header install never reads
  xnuHeaderTree = fs.difference ../../darwin/src/Kernel/xnu (fs.unions (map
    (d: fs.fileFilter (f: f.hasExt "c" || f.hasExt "cpp" || f.hasExt "s") (../../darwin/src/Kernel/xnu + "/${d}"))
    [ "osfmk" "bsd" "iokit" "libkern" "libsa" "pexpert" "security" "san" ]));
  # what xnu_headers.extproj reads, so a kernel .c edit leaves the headers alone
  xnuHeadersSource = fs.toSource {
    root = ../../darwin;
    fileset = fs.unions [
      ../../darwin/CMakeLists.txt
      ../../darwin/src/CMakeLists.txt
      ../../darwin/cmake
      ../../darwin/tools
      ../../darwin/src/Kernel/CMakeLists.txt
      xnuHeaderTree
      ../../darwin/src/Kernel/libfirehose_kernel
      ../../darwin/src/Libraries/CMakeLists.txt
      ../../darwin/src/Libraries/AvailabilityVersions
      ../../darwin/src/Libraries/libSystem/CMakeLists.txt
      ../../darwin/src/Libraries/libSystem/libplatform
      ../../darwin/src/Libraries/libSystem/pthread
    ];
  };
  # the kexts with PUREDARWIN_KEXTS_ONLY and prebuilt xnu headers: xnu and the libraries as headers only
  kextsOnlyFiles = fs.unions [
    ../../darwin/CMakeLists.txt
    ../../darwin/src/CMakeLists.txt
    ../../darwin/cmake
    ../../darwin/src/Kernel/CMakeLists.txt
    ../../darwin/src/Kernel/libkmod
    ../../darwin/src/Kernel/xnu/CMakeLists.txt
    ../../darwin/src/Kernel/xnu/EXTERNAL_HEADERS
    (headersIn ../../darwin/src/Kernel/xnu)
    ../../darwin/src/Libraries/CMakeLists.txt
    ../../darwin/src/Libraries/AvailabilityVersions
    ../../darwin/src/Libraries/libSystem/pthread/CMakeLists.txt
    ../../darwin/src/Libraries/libSystem/pthread/include
    ../../darwin/src/Libraries/libSystem/pthread/private
  ];
  kextsOnlySource = fs.toSource {
    root = ../../darwin;
    fileset = fs.unions [
      kextsOnlyFiles
      ../../darwin/src/Kernel/Extensions
      (headersIn ../../darwin/src/Libraries/libsandbox)
    ];
  };
  userlandSource = stripOsfmkImpl (sourceWith "kido-userland-source" [
    # pkill uses os_assert from libc's private os/assumes.h, which includes _simple.h
    "src/Libraries/libSystem/libc/os/assumes.h"
    "src/Libraries/libSystem/libplatform/private/_simple.h"
    # nohup detaches from the console session through _vprocmgr_detach_from_console;
    # vproc_priv.h is private and not in the SDK.
    "src/Libraries/XPC/libxpc/include"
    "src/Kernel/xnu/osfmk"
    # ping(8) from network_cmds needs <netinet/ip_var.h>,
    # <netinet/in_systm.h>, <netinet/ip_icmp.h> and the SO_TC_* socket
    # options in <sys/socket.h>'s PRIVATE block.
    "src/Kernel/xnu/bsd"
    "src/Libraries/IOKit"
    "src/Libraries/libSystem/libmalloc/compat-include"
    "src/Libraries/libSystem/libsystem_kernel/mach"
    "src/Libraries/CommonCrypto/include"
    # mdnsd is built from src/Userspace/mdnsd but its sources are
    # vendored alongside the libdns_sd client.
    "src/Libraries/mDNSResponder"
    "src/Userspace"
    "tools/mig"
  ]);
  coreFoundationSource = sourceWith "kido-corefoundation-source" [
    "src/Frameworks/CoreFoundation"
    "src/Libraries/libSystem/libc/pd-compat-include"
  ];
  coreServicesSource = sourceWith "kido-coreservices-source" [
    "src/Libraries/CoreServices"
  ];
  securitySource = stripOsfmkImpl (sourceWith "kido-security-source"
    (libSystemSourcePaths ++ [
      "src/Libraries/Security"
      "src/Libraries/libDER"
      "src/Libraries/CommonCrypto"
      "src/Libraries/XPC/libxpc/include"
      # debugging.h includes <asl.h>
      "src/Libraries/syslog/libsystem_asl.tproj/include"
      # SecCFWrappers.h includes <IOKit/IOReturn.h>
      "src/Kernel/xnu/iokit"
      # SecItem.c/SecTrust.c include <os/activity.h>
      "src/Libraries/libsystem_trace"
      # SecPolicyLeafCallbacks.c includes <dlfcn.h>
      "src/Libraries/dyld/upstream/include"
      # SecCFError.c includes <notify.h>
      "src/Libraries/XPC/notify"
      # CommonCrypto reaches <mach-o/dyld.h> -> <architecture/byte_order.h>
      "src/Libraries/architecture"
      # SecTask.c includes <IOKit/IOKitLib.h> and <bsm/libbsm.h>
      "src/Libraries/IOKit"
      "src/Libraries/libdarwin/pd-compat-include"
    ]));
  systemConfigurationSource = stripOsfmkImpl (sourceWith "kido-systemconfiguration-source"
    (libSystemSourcePaths ++ [
      "src/Libraries/SystemConfiguration"
      # configd links bootp's IPConfiguration plugin
      "src/Libraries/bootp"
      # report_symptoms.c includes <SymptomReporter/SymptomReporter.h>
      "src/Libraries/SymptomReporter"
      # SCPreferences.h includes <Security/Security.h>, and
      # SCNetworkConfigurationPrivate.h <IOKit/IOKitLib.h>.
      "src/Libraries/Security"
      "src/Libraries/IOKit"
      # SCNetworkInterface.c reads 802.1X config keys; ip_plugin.c reads
      # PPP link states.
      "src/Libraries/eap8021x"
      "src/Libraries/ppp"
      # configd's session.c needs IOKit/IOReturn.h; SCNetworkInterface.c
      # and InterfaceNamer need the IONetworkingFamily headers.
      "src/Kernel/xnu/iokit"
      "src/Kernel/Extensions/IONetworkingFamily/include"
      "src/Kernel/Extensions/IOStorageFamily/include"
      "src/Kernel/Extensions/IOSerialFamily/include"
      "src/Kernel/Extensions/IOUSBFamily/include"
      "src/Libraries/dyld/upstream/include"
    ]));
  iokitCFSource = stripOsfmkImpl (sourceWith "kido-iokitcf-source"
    (libSystemSourcePaths ++ [
      "src/Kernel/xnu/iokit"
      # IOHIDKeys.h and friends, shared between IOHIDLib and the kext. The
      # include/ tree is symlinks into IOHIDFamily/, so both are needed.
      "src/Kernel/Extensions/IOHIDFamily/include"
      "src/Kernel/Extensions/IOHIDFamily/IOHIDFamily"
      "src/Kernel/Extensions/IOHIDFamily/IOHIDSystem/IOKit"
    ]));
  symptomReporterSource = stripOsfmkImpl (sourceWith "kido-symptomreporter-source"
    (libSystemSourcePaths ++ [
      "src/Libraries/SymptomReporter"
    ]));
  objcSource = sourceWith "kido-objc-source" [
    "src/Libraries/objc4"
    "src/Libraries/libunwind"
    "src/Libraries/clang_bootstrap/ptrauth.h"
  ];
  libcxxDylibSource = sourceWith "kido-libcxx-dylib-source" [
    "src/Libraries/libcxxabi"
    "src/Libraries/libcxx"
    "src/Libraries/libunwind"
    "src/Libraries/llvm-libc"
  ];
  foundationSource = sourceWith "kido-foundation-source" [
    "src/Frameworks/Foundation"
  ];
  metalSource = sourceWith "kido-metal-source" [
    "src/Frameworks/Metal"
  ];
  xcselectSource = sourceWith "kido-xcselect-source" [
    "src/Libraries/xcselect"
  ];

in {
  inherit
    coreFoundationSource
    coreServicesSource
    foundationSource
    iokitCFSource
    kernelSource
    kextsOnlySource
    libSystemSource
    libcxxDylibSource
    metalSource
    objcSource
    securitySource
    symptomReporterSource
    systemConfigurationSource
    userlandSource
    xcselectSource
    xnuHeadersSource
    ;
}
