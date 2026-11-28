# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 The kido authors
#
# Single source of truth for KidoDarwin's base-image component inventory:
#   - versionsJson: compact /usr/share/doc/kido/versions.json baked into the image
#   - sbom: standalone SPDX 2.3 JSON (sbom.spdx.json) flake output (.#sbom), NOT
#     baked into the container rootfs, ready for GHCR OCI attestation and releases.
{ runCommand
, lib
, python3
, rustc
, zsh
, ncurses
, libedit
, toybox
, icu
, zlib
, bzip2
, xz
, libffiReal
, libxml2
, libressl
, cacert
, iana-etc
, gnuEfiVersion
, llvmVersion
, xcselect
, swVers
, pinned
, agent
, kido
, nixpkgsRev
, revision
, created
, productName ? "KidoDarwin"
, productVersion ? "26.5"
, buildVersion ? "25F74"
}:

let
  tree = ../..;
  spdxTexts = builtins.path { path = tree + "/LICENSES"; name = "licenses"; };
  validator = builtins.path { path = tree + "/scripts/validate-sbom.py"; name = "validate-sbom.py"; };

  kidoRev = kido.rev or "unknown";
  darwinRev = revision;
  # For in-tree KidoDarwin components, prefer the git revision when clean, else
  # fall back to the OS version-build pair rather than emitting "unknown".
  inTreeVersion =
    if revision != "unknown" && revision != "" then revision
    else "${productVersion}-${buildVersion}";

  # Metadata for third-party libraries pinned via nix/darwin/third-party/<name>/source.json.
  # Iterating over `pinned` guarantees adding a pinned library never drifts silently.
  pinnedMeta = {
    curl = {
      purpose = "APPLICATION";
      supplier = "Organization: curl";
      downloadLocation = v: "https://github.com/curl/curl/releases/tag/curl-${lib.replaceStrings [ "." ] [ "_" ] v}";
      where = "/usr/lib/libcurl.4.dylib, /usr/bin/curl";
    };
    nghttp2 = {
      purpose = "LIBRARY";
      supplier = "Organization: nghttp2";
      downloadLocation = v: "https://github.com/nghttp2/nghttp2/releases/tag/v${v}";
      where = "/usr/lib/libapple_nghttp2.dylib";
    };
    expat = {
      purpose = "LIBRARY";
      supplier = "Organization: libexpat";
      downloadLocation = _v: "https://github.com/libexpat/libexpat";
      where = "/usr/lib/libexpat.1.dylib";
    };
    sqlite = {
      purpose = "LIBRARY";
      supplier = "Organization: SQLite";
      downloadLocation = _v: "https://sqlite.org/";
      where = "/usr/lib/libsqlite3.dylib";
    };
    libiconv = {
      purpose = "LIBRARY";
      supplier = "Organization: Apple Inc.";
      downloadLocation = _v: "https://github.com/apple-oss-distributions/libiconv";
      where = "/usr/lib/libiconv.2.dylib, /usr/lib/libcharset.1.dylib, /usr/bin/iconv";
    };
    libutil = {
      purpose = "LIBRARY";
      supplier = "Organization: Apple Inc.";
      downloadLocation = _v: "https://github.com/apple-oss-distributions/libutil";
      where = "/usr/lib/libutil.dylib";
    };
  };

  pinnedComponents = lib.mapAttrsToList (name: _pkg:
    let
      pin = lib.importJSON (../darwin/third-party + "/${name}/source.json");
      m = pinnedMeta.${name};
    in {
      inherit name;
      version = pin.version;
      spdx = pin.license;
      inherit (m) purpose supplier where;
      downloadLocation = m.downloadLocation pin.version;
    }
  ) pinned;

  # Every component shipped in the KidoDarwin base image (kernel, kexts,
  # bootloader, core libraries, frameworks, userspace, third-party packages,
  # and guest agent). Licenses align with LICENSING.md and NOTICE.
  components = [
    # Kernel, kexts, bootloader, and kido drivers
    {
      name = "xnu";
      version = "12377.121.6";
      spdx = "APSL-2.0 AND CDDL-1.0 AND Apache-2.0 AND BSD-3-Clause AND MIT";
      purpose = "OPERATING-SYSTEM";
      downloadLocation = "https://github.com/apple-oss-distributions/xnu/tree/xnu-12377.121.6";
      supplier = "Organization: Apple Inc.";
      where = "/System/Library/Kernels/kernel";
    }
    {
      name = "apple-kexts";
      version = inTreeVersion;
      spdx = "APSL-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: Apple Inc.";
      where = "IOPCIFamily, IOStorageFamily, IONetworkingFamily, AppleFileSystemDriver, pthread.kext";
    }
    {
      name = "puredarwin-kexts";
      version = inTreeVersion;
      spdx = "BSD-2-Clause AND MPL-2.0 AND APSL-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: PureDarwin / The kido authors";
      where = "PDArmPlatformExpert, PDArmPCI, PDEcamPCI, IOVirtIO*, ext4, Ext4FileSystemDriver, amfi, Sandbox, PD*";
    }
    {
      name = "corecrypto";
      version = "puredarwin-fork";
      spdx = "MPL-2.0 AND SSLeay-standalone AND Brian-Gladman-3-Clause AND LicenseRef-Public-Domain AND APSL-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: NeoDarwin / PureDarwin";
      where = "corecrypto.kext, libSystem";
    }
    {
      name = "xnu-loader";
      version = inTreeVersion;
      spdx = "BSD-3-Clause AND APSL-2.0";
      purpose = "FIRMWARE";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: Vali0004 / The kido authors";
      where = "EFI/BOOT/BOOTAA64.EFI";
    }
    {
      name = "gnu-efi";
      version = gnuEfiVersion;
      spdx = "LicenseRef-gnu-efi-efilib AND BSD-3-Clause AND BSD-2-Clause-Patent AND LGPL-2.1-or-later AND LGPL-3.0-or-later";
      purpose = "LIBRARY";
      downloadLocation = "https://sourceforge.net/projects/gnu-efi/";
      supplier = "Organization: gnu-efi";
      where = "Linked into BOOTAA64.EFI";
    }
    {
      name = "VirtioFS";
      version = inTreeVersion;
      spdx = "Apache-2.0 AND BSD-2-Clause";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: The kido authors";
      where = "VirtioFS.kext";
    }
    {
      name = "VirtioMetal";
      version = inTreeVersion;
      spdx = "Apache-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: The kido authors";
      where = "VirtioMetal.kext";
    }

    # Apple / Darwin core libraries and frameworks
    {
      name = "Libc";
      version = "1439.141.1";
      spdx = "APSL-2.0 AND BSD-2-Clause AND BSD-3-Clause AND BSD-4-Clause-UC AND CDDL-1.0 AND dtoa";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/Libc/tree/Libc-1439.141.1";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "openlibm";
      version = "0.8";
      spdx = "BSD-2-Clause AND BSD-3-Clause AND ISC AND MIT AND LicenseRef-Public-Domain AND Apache-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/JuliaMath/openlibm";
      supplier = "Organization: JuliaMath";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "libmalloc";
      version = "317.140.5";
      spdx = "APSL-2.0 AND Apache-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/libmalloc/tree/libmalloc-317.140.5";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "libplatform";
      version = "254.40.4";
      spdx = "APSL-2.0 AND Apache-2.0 AND BSD-3-Clause";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/libplatform/tree/libplatform-254.40.4";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "libpthread";
      version = "454.80.2";
      spdx = "APSL-2.0 AND Apache-2.0 AND BSD-3-Clause";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/libpthread/tree/libpthread-454.80.2";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "libdispatch";
      version = "1271.40.12";
      spdx = "Apache-2.0 AND (Apache-2.0 WITH Swift-exception)";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/libdispatch/tree/libdispatch-1271.40.12";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "dyld";
      version = "852.2";
      spdx = "APSL-2.0 AND BSD-2-Clause";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/dyld/tree/dyld-852.2";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/dyld";
    }
    {
      name = "objc4";
      version = "818.2";
      spdx = "APSL-2.0 AND NCSA AND (Apache-2.0 WITH LLVM-exception)";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/objc4/tree/objc4-818.2";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libobjc.A.dylib";
    }
    {
      name = "launchd";
      version = "842.92.1";
      spdx = "Apache-2.0 AND APSL-2.0";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/apple-oss-distributions/launchd/tree/launchd-842.92.1";
      supplier = "Organization: Apple Inc.";
      where = "/sbin/launchd, /bin/launchctl";
    }
    {
      name = "libxpc";
      version = inTreeVersion;
      spdx = "BSD-2-Clause AND Apache-2.0 AND MPL-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: NextBSD / PureDarwin / The kido authors";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "libnv";
      version = inTreeVersion;
      spdx = "BSD-2-Clause AND BSD-2-Clause-FreeBSD";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: FreeBSD";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "libdarwin-shims";
      version = inTreeVersion;
      spdx = "BSD-2-Clause AND MIT AND MPL-2.0 AND APSL-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: PureDarwin";
      where = "libDER, libdarwin, libsandbox, libsystem_trace, CrashReporterClient";
    }
    {
      name = "libmacho";
      version = "949.0.1";
      spdx = "APSL-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/cctools/tree/cctools-949.0.1";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "Libinfo";
      version = "542.40.3";
      spdx = "APSL-2.0 AND BSD-3-Clause AND ISC";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/Libinfo/tree/Libinfo-542.40.3";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "Libnotify";
      version = "279.40.4";
      spdx = "APSL-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/Libnotify/tree/Libnotify-279.40.4";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "Security";
      version = "59754.120.12";
      spdx = "APSL-2.0 AND Apache-2.0 AND MPL-1.1 AND BSD-3-Clause AND HPND";
      purpose = "FRAMEWORK";
      downloadLocation = "https://github.com/apple-oss-distributions/Security/tree/Security-59754.120.12";
      supplier = "Organization: Apple Inc.";
      where = "/System/Library/Frameworks/Security.framework";
    }
    {
      name = "CommonCrypto";
      version = "60178.120.3";
      spdx = "APSL-2.0 AND MPL-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/CommonCrypto/tree/CommonCrypto-60178.120.3";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "libresolv";
      version = "68.120.2";
      spdx = "APSL-2.0 AND ISC AND BSD-3-Clause";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/libresolv/tree/libresolv-68.120.2";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "syslog";
      version = "385";
      spdx = "APSL-2.0 AND BSD-3-Clause";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/apple-oss-distributions/syslog/tree/syslog-385";
      supplier = "Organization: Apple Inc.";
      where = "/usr/lib/libSystem.B.dylib";
    }
    {
      name = "SystemConfiguration";
      version = "1109.120.1";
      spdx = "APSL-2.0";
      purpose = "FRAMEWORK";
      downloadLocation = "https://github.com/apple-oss-distributions/configd/tree/configd-1109.120.1";
      supplier = "Organization: Apple Inc.";
      where = "/System/Library/Frameworks/SystemConfiguration.framework";
    }
    {
      name = "CoreServices";
      version = inTreeVersion;
      spdx = "BSD-2-Clause";
      purpose = "FRAMEWORK";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: PureDarwin";
      where = "/System/Library/Frameworks/CoreServices.framework";
    }
    {
      name = "IOKitUser";
      version = "1845.120.6";
      spdx = "APSL-2.0 AND (GPL-2.0-or-later WITH Bison-exception-2.2) AND BSD-2-Clause AND MPL-2.0";
      purpose = "FRAMEWORK";
      downloadLocation = "https://github.com/apple-oss-distributions/IOKitUser/tree/IOKitUser-1845.120.6";
      supplier = "Organization: Apple Inc.";
      where = "/System/Library/Frameworks/IOKit.framework";
    }
    {
      name = "mDNSResponder";
      version = "1310.120.71";
      spdx = "Apache-2.0 AND BSD-3-Clause AND OpenSSL";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/apple-oss-distributions/mDNSResponder/tree/mDNSResponder-1310.120.71";
      supplier = "Organization: Apple Inc.";
      where = "/usr/sbin/mdnsd";
    }
    {
      name = "CoreFoundation";
      version = "5.0.3";
      spdx = "(Apache-2.0 WITH Swift-exception) AND BSD-2-Clause AND MPL-2.0";
      purpose = "FRAMEWORK";
      downloadLocation = "https://github.com/swiftlang/swift-corelibs-foundation/tree/swift-5.0.3-RELEASE";
      supplier = "Organization: Swift / PureDarwin";
      where = "/System/Library/Frameworks/CoreFoundation.framework";
    }
    {
      name = "Foundation";
      version = inTreeVersion;
      spdx = "MPL-2.0 AND MIT";
      purpose = "FRAMEWORK";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: PureDarwin / The kido authors";
      where = "/System/Library/Frameworks/Foundation.framework";
    }
    {
      name = "Metal";
      version = inTreeVersion;
      spdx = "Apache-2.0";
      purpose = "FRAMEWORK";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: The kido authors";
      where = "/System/Library/Frameworks/Metal.framework";
    }
    {
      name = "libxcselect";
      version = xcselect.version;
      spdx = "Apache-2.0";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: The kido authors";
      where = "/usr/lib/libxcselect.dylib";
    }
    {
      name = "sw_vers";
      version = swVers.version;
      spdx = "Apache-2.0";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: The kido authors";
      where = "/usr/bin/sw_vers";
    }
    {
      name = "llvm-libcxx";
      version = "21.1.8";
      spdx = "(Apache-2.0 WITH LLVM-exception) AND Unicode-3.0 AND MIT AND LicenseRef-Public-Domain";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/llvm/llvm-project/tree/llvmorg-21.1.8";
      supplier = "Organization: LLVM Project";
      where = "/usr/lib/libc++.1.dylib, /usr/lib/libc++abi.dylib, libunwind";
    }
    {
      name = "llvm-compiler-rt";
      version = llvmVersion;
      spdx = "Apache-2.0 WITH LLVM-exception";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/llvm/llvm-project/tree/llvmorg-${llvmVersion}";
      supplier = "Organization: LLVM Project";
      where = "Linked into runtime libraries";
    }

    # Darwin / BSD / PureDarwin base userspace commands
    {
      name = "shell_cmds";
      version = "216.60.1";
      spdx = "APSL-2.0 AND BSD-3-Clause";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/apple-oss-distributions/shell_cmds/tree/shell_cmds-216.60.1";
      supplier = "Organization: Apple Inc.";
      where = "/bin/basename, /bin/echo, /bin/hostname, /bin/kill, /usr/bin/killall, ...";
    }
    {
      name = "text_cmds";
      version = "106";
      spdx = "APSL-2.0 AND BSD-3-Clause";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/apple-oss-distributions/text_cmds/tree/text_cmds-106";
      supplier = "Organization: Apple Inc.";
      where = "/bin/cat, /usr/bin/cut, /usr/bin/head, /usr/bin/tail, /usr/bin/tr, /usr/bin/wc, ...";
    }
    {
      name = "system_cmds";
      version = "880.120.1";
      spdx = "APSL-2.0 AND BSD-3-Clause";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/apple-oss-distributions/system_cmds/tree/system_cmds-880.120.1";
      supplier = "Organization: Apple Inc.";
      where = "/bin/sysctl, /sbin/dmesg, /sbin/reboot, /usr/bin/vm_stat, /usr/bin/hostinfo, ...";
    }
    {
      name = "adv_cmds";
      version = "174.0.1";
      spdx = "APSL-2.0 AND BSD-3-Clause";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/apple-oss-distributions/adv_cmds/tree/adv_cmds-174.0.1";
      supplier = "Organization: Apple Inc.";
      where = "/bin/ps, /usr/bin/pkill";
    }
    {
      name = "network_cmds";
      version = "606.140.1";
      spdx = "APSL-2.0 AND BSD-3-Clause";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/apple-oss-distributions/network_cmds/tree/network_cmds-606.140.1";
      supplier = "Organization: Apple Inc.";
      where = "/sbin/ping";
    }
    {
      name = "diskdev_cmds";
      version = "757";
      spdx = "APSL-2.0 AND BSD-2-Clause";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/apple-oss-distributions/diskdev_cmds/tree/diskdev_cmds-757";
      supplier = "Organization: Apple Inc.";
      where = "/sbin/mount, /sbin/umount";
    }
    {
      name = "gpt";
      version = inTreeVersion;
      spdx = "BSD-2-Clause AND BSD-3-Clause AND BSD-4-Clause";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: NetBSD";
      where = "/usr/sbin/gpt";
    }
    {
      name = "puredarwin-userspace";
      version = inTreeVersion;
      spdx = "BSD-2-Clause AND MPL-2.0";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/kido-sh/darwin";
      supplier = "Organization: PureDarwin / The kido authors";
      where = "/bin/sh, /bin/netsetup, /usr/libexec/kido-dmesg-save, doubleagentd, logd, ext4tool";
    }

    # Third-party libraries and utilities in the base image
    {
      name = "libressl";
      version = libressl.version;
      spdx = "OpenSSL AND SSLeay-standalone AND ISC AND BSD-3-Clause AND LicenseRef-Public-Domain";
      purpose = "LIBRARY";
      downloadLocation = "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-${libressl.version}.tar.gz";
      supplier = "Organization: OpenBSD";
      where = "/usr/lib/libcrypto.46.dylib, /usr/lib/libssl.48.dylib, /usr/lib/libtls.20.dylib, /usr/bin/openssl, /usr/bin/ocspcheck";
    }
  ] ++ pinnedComponents ++ [
    {
      name = "zsh";
      version = zsh.version;
      spdx = "LicenseRef-zsh";
      purpose = "APPLICATION";
      downloadLocation = "https://www.zsh.org/";
      supplier = "Organization: Zsh Development Group";
      where = "/bin/zsh";
    }
    {
      name = "ncurses";
      version = ncurses.version;
      spdx = "X11-distribute-modifications-variant";
      purpose = "LIBRARY";
      downloadLocation = "https://invisible-island.net/ncurses/";
      supplier = "Person: Thomas E. Dickey";
      where = "/usr/lib/libncurses.5.4.dylib, /usr/share/terminfo";
    }
    {
      name = "libedit";
      version = libedit.version;
      spdx = "BSD-3-Clause AND BSD-2-Clause";
      purpose = "LIBRARY";
      downloadLocation = "https://thrysoee.dk/editline/";
      supplier = "Organization: NetBSD / Jess Thrysoee";
      where = "/usr/lib/libedit.3.dylib";
    }
    {
      name = "icu";
      version = icu.version;
      spdx = "LicenseRef-ICU";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/unicode-org/icu";
      supplier = "Organization: Unicode Consortium";
      where = "/usr/lib/libicucore.A.dylib";
    }
    {
      name = "zlib";
      version = zlib.version;
      spdx = "Zlib";
      purpose = "LIBRARY";
      downloadLocation = "https://zlib.net/";
      supplier = "Organization: zlib";
      where = "/usr/lib/libz.1.dylib";
    }
    {
      name = "bzip2";
      version = bzip2.version;
      spdx = "bzip2-1.0.6";
      purpose = "LIBRARY";
      downloadLocation = "https://sourceware.org/bzip2/";
      supplier = "Person: Julian Seward";
      where = "/usr/lib/libbz2.1.0.dylib";
    }
    {
      name = "xz";
      version = xz.version;
      spdx = "0BSD";
      purpose = "LIBRARY";
      downloadLocation = "https://tukaani.org/xz/";
      supplier = "Organization: Tukaani Project";
      where = "/usr/lib/liblzma.5.dylib";
    }
    {
      name = "toybox";
      version = toybox.version;
      spdx = "0BSD";
      purpose = "APPLICATION";
      downloadLocation = "https://landley.net/toybox/";
      supplier = "Person: Rob Landley";
      where = "/bin/toybox";
    }
    {
      name = "libffi-foundation";
      version = libffiReal.version;
      spdx = "MIT";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/libffi/libffi";
      supplier = "Organization: libffi";
      where = "Linked into Foundation.framework";
    }
    {
      name = "libxml2-foundation";
      version = libxml2.version;
      spdx = "MIT";
      purpose = "LIBRARY";
      downloadLocation = "https://gitlab.gnome.org/GNOME/libxml2";
      supplier = "Organization: GNOME";
      where = "Linked into Foundation.framework";
    }
    {
      name = "ca-certificates";
      version = cacert.version;
      spdx = "MPL-2.0";
      purpose = "FILE";
      downloadLocation = "https://firefox-source-docs.mozilla.org/security/nss/index.html";
      supplier = "Organization: Mozilla";
      where = "/etc/ssl/cert.pem, /System/Library/Security/Certificates.bundle";
    }
    {
      name = "iana-etc";
      version = iana-etc.version;
      spdx = "MIT";
      purpose = "FILE";
      downloadLocation = "https://www.iana.org/protocols";
      supplier = "Organization: IANA";
      where = "/etc/services, /etc/protocols";
    }
    {
      name = "kido-agent";
      version = agent.version;
      spdx = "Apache-2.0";
      purpose = "APPLICATION";
      downloadLocation = "https://github.com/kido-sh/kido";
      supplier = "Organization: The kido authors";
      where = "/usr/libexec/kido-agent";
    }
    {
      name = "rust-std";
      version = rustc.version;
      spdx = "MIT AND Apache-2.0 AND (Apache-2.0 WITH LLVM-exception)";
      purpose = "LIBRARY";
      downloadLocation = "https://github.com/rust-lang/rust/releases/tag/${rustc.version}";
      supplier = "Organization: The Rust Project Developers";
      where = "Statically linked into /usr/libexec/kido-agent";
    }
  ];

  manifestData = builtins.toJSON {
    image = {
      inherit productName productVersion buildVersion nixpkgsRev created;
      darwinRevision = darwinRev;
      kidoRev = kidoRev;
    };
    inherit components;
  };

  versionsData = builtins.toJSON {
    schemaVersion = 1;
    image = {
      inherit productName productVersion buildVersion nixpkgsRev created;
      darwinRevision = darwinRev;
      kidoRev = kidoRev;
    };
    components = map (c: {
      inherit (c) name version spdx;
    }) components;
  };

  versionsJson = runCommand "kido-versions-json" {
    nativeBuildInputs = [ python3 ];
    passAsFile = [ "rawJson" ];
    rawJson = versionsData;
  } ''
    python3 - "$rawJsonPath" "$out" <<'PY'
import json, sys
data = json.load(open(sys.argv[1], encoding="utf-8"))
with open(sys.argv[2], "w", encoding="utf-8") as f:
    json.dump(data, f, separators=(",", ":"), ensure_ascii=True)
    f.write("\n")
PY
    chmod 644 "$out"
  '';

  sbom = runCommand "kido-sbom-${productVersion}-${buildVersion}" {
    nativeBuildInputs = [ python3 ];
    passAsFile = [ "manifestJson" ];
    manifestJson = manifestData;
  } ''
    mkdir -p "$out"
    python3 - "$manifestJsonPath" "${agent.cargoDeps}" "${spdxTexts}" "$out/sbom.spdx.json" <<'PY'
import hashlib
import json
import re
import sys
import tomllib
import urllib.parse
from pathlib import Path

manifest = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
cargo_vendor = Path(sys.argv[2])
licenses_dir = Path(sys.argv[3])
out_path = Path(sys.argv[4])

img = manifest["image"]
comps = manifest["components"]


def sanitize_spdx_id(s: str) -> str:
    return re.sub(r"[^A-Za-z0-9.-]", "-", s)


def normalize_cargo_license(raw: str) -> str:
    # Normalize legacy Cargo slash syntax (e.g. "MIT/Apache-2.0") to SPDX "OR".
    return re.sub(r"\s*/\s*", " OR ", raw.strip())


def extract_license_refs(expr: str) -> set[str]:
    spaced = expr.replace("(", " ( ").replace(")", " ) ")
    return {tok for tok in spaced.split() if tok.startswith("LicenseRef-")}


root_spdx_id = "SPDXRef-KidoDarwin-Image"
packages = [
    {
        "SPDXID": root_spdx_id,
        "name": img["productName"],
        "versionInfo": f"{img['productVersion']}-{img['buildVersion']}",
        "downloadLocation": "https://github.com/kido-sh/darwin",
        "filesAnalyzed": False,
        "licenseConcluded": "NOASSERTION",
        "licenseDeclared": "NOASSERTION",
        "copyrightText": "Copyright (c) 2026 The kido authors",
        "supplier": "Organization: The kido authors",
        "primaryPackagePurpose": "OPERATING-SYSTEM",
        "comment": (
            f"darwin={img['darwinRevision']} nixpkgs={img['nixpkgsRev']} "
            f"kido={img['kidoRev']}"
        ),
    }
]

relationships = [
    {
        "spdxElementId": "SPDXRef-DOCUMENT",
        "relationshipType": "DESCRIBES",
        "relatedSpdxElement": root_spdx_id,
    }
]

referenced_license_refs: set[str] = set()

for c in comps:
    sid = f"SPDXRef-pkg-{sanitize_spdx_id(c['name'])}"
    lic = c["spdx"]
    referenced_license_refs |= extract_license_refs(lic)
    pkg = {
        "SPDXID": sid,
        "name": c["name"],
        "versionInfo": c["version"],
        "downloadLocation": c["downloadLocation"],
        "filesAnalyzed": False,
        "licenseConcluded": lic,
        "licenseDeclared": lic,
        "copyrightText": "NOASSERTION",
        "supplier": c["supplier"],
        "primaryPackagePurpose": c["purpose"],
        "comment": f"Installed at: {c['where']}",
    }
    packages.append(pkg)
    relationships.append(
        {
            "spdxElementId": root_spdx_id,
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": sid,
        }
    )

# Collect vendored Cargo crates from all source-* subdirectories under cargoDeps.
crate_dirs = []
for src_group in sorted(p for p in cargo_vendor.iterdir() if p.is_dir() and p.name.startswith("source-")):
    for cdir in sorted(p for p in src_group.iterdir() if p.is_dir()):
        if (cdir / "Cargo.toml").is_file():
            crate_dirs.append(cdir)
if not crate_dirs:
    raise SystemExit(f"ERROR: no vendored Cargo crates found under {cargo_vendor}")

seen_cargo_ids: set[str] = set()
non_darwin_targets = ("wasi", "windows-", "windows_", "winapi", "redox_")

for cdir in crate_dirs:
    parsed = tomllib.loads((cdir / "Cargo.toml").read_text(encoding="utf-8"))
    meta = parsed.get("package", {})
    cname = meta["name"]
    cver = meta["version"]
    raw_lic = meta.get("license")
    if not raw_lic:
        raise SystemExit(f"ERROR: Cargo crate {cname}-{cver} has no SPDX license field")
    clic = normalize_cargo_license(raw_lic)
    referenced_license_refs |= extract_license_refs(clic)

    base_sid = f"SPDXRef-cargo-{sanitize_spdx_id(cname)}-{sanitize_spdx_id(cver)}"
    sid = base_sid
    if sid in seen_cargo_ids:
        short = hashlib.sha256(f"{cname}@{cver}".encode()).hexdigest()[:8]
        sid = f"{base_sid}-{short}"
    seen_cargo_ids.add(sid)

    purl_name = urllib.parse.quote(cname, safe="")
    purl_ver = urllib.parse.quote(cver, safe="")
    is_proc_macro = bool(parsed.get("lib", {}).get("proc-macro", False))
    is_non_darwin = cname == "wasi" or cname.startswith(non_darwin_targets)

    packages.append(
        {
            "SPDXID": sid,
            "name": cname,
            "versionInfo": cver,
            "downloadLocation": f"https://crates.io/api/v1/crates/{purl_name}/{purl_ver}/download",
            "filesAnalyzed": False,
            "licenseConcluded": clic,
            "licenseDeclared": clic,
            "copyrightText": "NOASSERTION",
            "supplier": "NOASSERTION",
            "primaryPackagePurpose": "LIBRARY",
            "externalRefs": [
                {
                    "referenceCategory": "PACKAGE-MANAGER",
                    "referenceType": "purl",
                    "referenceLocator": f"pkg:cargo/{purl_name}@{purl_ver}",
                }
            ],
        }
    )

    if is_proc_macro:
        relationships.append(
            {
                "spdxElementId": sid,
                "relationshipType": "BUILD_DEPENDENCY_OF",
                "relatedSpdxElement": "SPDXRef-pkg-kido-agent",
            }
        )
    elif is_non_darwin:
        relationships.append(
            {
                "spdxElementId": sid,
                "relationshipType": "OPTIONAL_DEPENDENCY_OF",
                "relatedSpdxElement": "SPDXRef-pkg-kido-agent",
            }
        )
    else:
        relationships.append(
            {
                "spdxElementId": "SPDXRef-pkg-kido-agent",
                "relationshipType": "CONTAINS",
                "relatedSpdxElement": sid,
            }
        )

extracted_infos = []
for ref in sorted(referenced_license_refs):
    txt_path = licenses_dir / f"{ref}.txt"
    if not txt_path.is_file():
        raise SystemExit(f"ERROR: missing license text file {txt_path} for {ref}")
    extracted_infos.append(
        {
            "licenseId": ref,
            "name": ref,
            "extractedText": txt_path.read_text(encoding="utf-8"),
        }
    )

# Deterministic content digest ensures documentNamespace is globally unique per
# distinct SBOM even for dirty/path-flake builds while staying reproducible.
digest_input = json.dumps(
    {"image": img, "packages": packages, "relationships": relationships},
    sort_keys=True,
    separators=(",", ":"),
).encode("utf-8")
content_id = hashlib.sha256(digest_input).hexdigest()[:16]

doc = {
    "spdxVersion": "SPDX-2.3",
    "dataLicense": "CC0-1.0",
    "SPDXID": "SPDXRef-DOCUMENT",
    "name": f"{img['productName']}-{img['productVersion']}-{img['buildVersion']}",
    "documentNamespace": (
        f"https://github.com/kido-sh/darwin/spdx/"
        f"{img['productName']}-{img['productVersion']}-{img['buildVersion']}-{content_id}"
    ),
    "creationInfo": {
        "created": img["created"],
        "creators": [
            "Tool: kido-darwin-nix-sbom",
            "Organization: The kido authors",
        ],
        "licenseListVersion": "3.25",
    },
    "packages": packages,
    "hasExtractedLicensingInfos": extracted_infos,
    "relationships": relationships,
}

out_path.write_text(json.dumps(doc, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
PY

    python3 "${validator}" "${versionsJson}" "$out/sbom.spdx.json"
    chmod 644 "$out/sbom.spdx.json"
  '';
in
{
  inherit versionsJson sbom;
}
