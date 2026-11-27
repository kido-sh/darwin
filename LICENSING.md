# Licensing

This repository combines code under many licenses. This file says which license covers which
files, what each license asks of anyone who distributes the repository or an image built from it,
and the decisions behind the less obvious cases.

- LICENSES/ holds one full text per license (REUSE layout, file name = SPDX identifier).
- NOTICE holds the attributions the licenses require.
- docs/upstream.md lists the upstream bases and every file changed from its upstream release.

A license header in a file always wins over this table. `scripts/check-licensing.py` checks that
every file is matched by a row below and that every license named here or in an SPDX line has a
text in LICENSES/.

## Decisions

- **kido's own files** are `Apache-2.0`, "Copyright (c) 2026 The kido authors", with an
  SPDX-License-Identifier line. A change kido makes to an existing file is under that file's
  license.
- **A component's own license file governs its unlabeled files**, as Foundation's LICENSE
  (`MPL-2.0`) does.
- **Other PureDarwin files without a license header** are `BSD-2-Clause`, the license of
  PureDarwin's root PUREDARWIN_LICENSE.txt ("Copyright (c) 2007-Present The PureDarwin Project").
- **Files from Apple's open-source releases without a license line of their own** take their
  release's license, `APSL-2.0` for most and `Apache-2.0` for launchd, libdispatch, mDNSResponder
  and the like, whether PureDarwin left them byte-identical or changed them. Files that name APSL
  1.0, 1.1 or 1.2 are used under APSL 2.0, as the versions clause of each allows.
- **NSS code** in Security's OSX/libsecurity_asn1 is offered under `MPL-1.1`, GPL 2.0 or later,
  or LGPL 2.1 or later. We take it under `MPL-1.1`.
- **Removed, not relicensed:** see the section of that name below.
- **corecrypto** is unaudited; see the section of that name below.

## Which license covers which files

The second column is the license of files without a header of their own. The third column names
the release a directory came from and the other licenses its files carry.

| Path | License | Notes |
|---|---|---|
| `LICENSING.md`, `NOTICE`, `README.md`, `docs/**`, `scripts/**`, `test/**`, `flake.nix`, `flake.lock`, `.gitignore`, `.github/**` | `Apache-2.0` | kido |
| `LICENSES/**` | | The license texts themselves |
| `nix/**` | `BSD-2-Clause` | PureDarwin's Nix code as reorganized by kido. kido's new files carry an `Apache-2.0` SPDX line |
| `nix/darwin/third-party/{curl,expat,libbz2-dylib,libedit,libiconv,liblzma-dylib,libutil,ncurses-dylib,nghttp2,sqlite}/**` | `Apache-2.0` | kido's recipes for macOS's libbz2, libcurl, libedit, libexpat, libiconv, liblzma, libapple_nghttp2, libsqlite3, libutil and ncurses, their source pins, their export lists, taken from the macOS SDK's .tbd files, the ncurses ABI glue and the libraries' tests |
| `nix/darwin/third-party/curl/*.patch` | `curl` | kido's change to curl 8.23.0's lib/vtls/openssl.c, under curl's license |
| `nix/darwin/third-party/sqlite/*.patch` | `blessing` | kido's changes to SQLite 3.54.1's src/main.c and ext/fts3/fts3_tokenizer.c, under SQLite's blessing |
| `nix/darwin/third-party/libiconv/*.patch` | `BSD-2-Clause` | kido's change to the Citrus ISO-2022 converter of Apple's libiconv-115.120.2, under its license |
| `nix/darwin/third-party/libressl/default.nix`, `nix/darwin/third-party/libressl/*.exports` | `Apache-2.0` | kido's recipe for macOS's LibreSSL and its export lists, read from the dyld shared cache of macOS 27.0.1 |
| `nix/darwin/third-party/libressl/*.patch` | `OpenSSL` | LibreSSL's own fixes from its later releases, backported to 3.3.6, under the licenses of the files they change. Also `SSLeay-standalone` and `ISC` |
| `patches/xnu-loader-no-logo.patch` | `BSD-3-Clause` | kido's change to xnu-loader |
| `patches/libtapi-riscv.patch` | `Apache-2.0 WITH LLVM-exception` | PureDarwin's change to LLVM's libtapi |
| `loader/**` | `BSD-3-Clause` | xnu-loader by Vali0004. Its include/boot.h is xnu's pexpert/i386/boot.h as xnu-loader changed it, `APSL-2.0` |
| `kc-tools/**`, `iig-tools/**` | `BSD-3-Clause` | Vali0004's build tools |
| `darwin/CMakeLists.txt`, `darwin/cmake/**`, `darwin/**/CMakeLists.txt`, `darwin/PUREDARWIN_LICENSE.txt` | `BSD-2-Clause` | PureDarwin's CMake build and license file. The CMake files of LLVM and CoreFoundation keep their upstream license |
| `darwin/src/Kernel/xnu/**`, `darwin/src/Kernel/libkmod/**` | `APSL-2.0` | xnu-12377.121.6. Also `APSL-1.0`, `APSL-1.1`, `APSL-1.2`, `BSD-2-Clause`, `BSD-3-Clause`, `BSD-4-Clause`, `BSD-4-Clause-UC`, `CMU-Mach`, `HPND`, `ISC`, `MIT`, `Zlib`, `CDDL-1.0`, `Unicode-3.0`, `Apache-2.0`, `Apache-2.0 WITH LLVM-exception`, `GPL-2.0-or-later WITH Bison-exception-2.2`, `LicenseRef-Public-Domain`. kido's osfmk/prng/hmac_drbg.c and hmac_drbg.h are `Apache-2.0` |
| `darwin/src/Kernel/libfirehose_kernel/**` | `Apache-2.0` | libdispatch-1271.40.12's firehose code |
| `darwin/src/Kernel/Extensions/{AppleFileSystemDriver,IOCDStorageFamily,IODVDStorageFamily,IOGraphicsFamily,IOHIDFamily,IONetworkingFamily,IOPCIFamily,IOSCSIArchitectureModelFamily,IOSerialFamily,IOStorageFamily,IOUSBFamily,pthread}/**` | `APSL-2.0` | Apple's kexts and headers; releases in docs/upstream.md. Also `APSL-1.1` |
| `darwin/src/Kernel/Extensions/{PDArmPCI,PDArmPlatformExpert,PDDiskImages2,PDEcamPCI,PDKeyStore,PDLifs,PDWatchdog,amfi,corecrypto,ext4,Ext4FileSystemDriver}/**` | `BSD-2-Clause` | PureDarwin. Also `MIT` and `MPL-2.0 OR BSD-2-Clause`. Ext4FileSystemDriver.cpp, structured after Apple's AppleFileSystemDriver.cpp, is `APSL-2.0` |
| `darwin/src/Kernel/Extensions/{IOVirtIOBlock,IOVirtIOFamily,IOVirtIONet,Sandbox}/**` | `BSD-2-Clause` | PureDarwin. Files with a header: `MPL-2.0 OR BSD-2-Clause` |
| `darwin/src/Kernel/Extensions/VirtioFS/**` | `Apache-2.0` | kido's virtio-fs client |
| `darwin/src/Kernel/Extensions/VirtioFS/fuse.h` | `BSD-2-Clause` | Linux 7.3's include/uapi/linux/fuse.h, FUSE 7.46, used under its BSD-2-Clause option. The header also offers `GPL-2.0 WITH Linux-syscall-note` |
| `darwin/src/Kernel/Extensions/VirtioMetal/**` | `Apache-2.0` | kido's virtio-metal GPU passthrough driver |
| `darwin/external/corecrypto/**` | `MPL-2.0` | NeoDarwin's corecrypto as forked by PureDarwin. Also `Brian-Gladman-3-Clause` (ccaes/src/gladman), `SSLeay-standalone` (Eric Young's code in cccast, ccrc4, ccsha1), `LicenseRef-Public-Domain` (LibTomCrypt-derived code), `APSL-2.0` (two xnu headers) |
| `darwin/src/Libraries/libSystem/libc/**` | `APSL-2.0` | Libc-1439.141.1 with FreeBSD and NetBSD code. Also `APSL-1.0`, `APSL-1.1`, `BSD-2-Clause`, `BSD-2-Clause-FreeBSD`, `BSD-3-Clause`, `BSD-4-Clause`, `BSD-4-Clause-UC`, `ISC`, `CMU-Mach`, `HPND`, `dtoa`, `CDDL-1.0`, `Apache-2.0`, `MPL-2.0 OR BSD-2-Clause`, `LicenseRef-Public-Domain` |
| `darwin/src/Libraries/libSystem/libc/libm/openlibm/**` | `BSD-2-Clause` | openlibm; see its LICENSE.md. Also `BSD-3-Clause`, `ISC`, `MIT`, `LicenseRef-Public-Domain`. kido's src/kido_fenv_arm64.c is `Apache-2.0` |
| `darwin/src/Libraries/libSystem/libmalloc/**` | `APSL-2.0` | libmalloc-317.140.5. Also `APSL-1.1`, `Apache-2.0`. kido's src/kido_malloc_type.c is `Apache-2.0` |
| `darwin/src/Libraries/libSystem/libplatform/**` | `APSL-2.0` | libplatform-254.40.4. Also `Apache-2.0`, `BSD-2-Clause`, `BSD-3-Clause`, `BSD-4-Clause-UC` |
| `darwin/src/Libraries/libSystem/pthread/**` | `APSL-2.0` | libpthread-454.80.2. Also `Apache-2.0`, `BSD-2-Clause`, `BSD-3-Clause`, `BSD-4-Clause`, `HPND` |
| `darwin/src/Libraries/libSystem/libdispatch/**` | `Apache-2.0` | libdispatch-1271.40.12. Also `Apache-2.0 WITH Swift-exception` |
| `darwin/src/Libraries/libSystem/libsystem_kernel/**` | `APSL-2.0` | xnu-12377.121.6's libsyscall. Also `APSL-1.1`, `BSD-2-Clause`, `BSD-3-Clause`, `BSD-4-Clause-UC`, `CMU-Mach` |
| `darwin/src/Libraries/libSystem/{libdyld,stub}/**` | `BSD-2-Clause` | PureDarwin. Also `MPL-2.0 OR BSD-2-Clause` |
| `darwin/src/Libraries/dyld/**` | `APSL-2.0` | dyld-852.2 under upstream/; PureDarwin's shims beside it are `BSD-2-Clause`. Also `APSL-1.0`, `LicenseRef-Public-Domain` |
| `darwin/src/Libraries/XPC/{launchd,launchctl}/**` | `Apache-2.0` | launchd-842.92.1. Also `APSL-2.0` and `CMU-Mach` (two .defs files) |
| `darwin/src/Libraries/XPC/libxpc/**` | `BSD-2-Clause` | NextBSD's libxpc (iXsystems) as changed by PureDarwin. liblaunch's files from launchd-842 and kido's xpc_abort.c, xpc_codesign.c, xpc_errors.c, include/launch.h and include/xpc/ are `Apache-2.0`; xpc_pipe.c is `MPL-2.0 OR BSD-2-Clause` |
| `darwin/src/Libraries/XPC/libnv/**` | `BSD-2-Clause` | FreeBSD's libnv. Also `BSD-2-Clause-FreeBSD` |
| `darwin/src/Libraries/XPC/libinfo/**` | `APSL-2.0` | Libinfo-542.40.3. Also `APSL-1.1`, `BSD-2-Clause`, `BSD-3-Clause`, `BSD-4-Clause`, `BSD-4-Clause-UC`, `ISC` |
| `darwin/src/Libraries/XPC/notify/**` | `APSL-2.0` | Libnotify-279.40.4 |
| `darwin/src/Libraries/XPC/logd/**` | `BSD-2-Clause` | PureDarwin |
| `darwin/src/Libraries/objc4/**` | `APSL-2.0` | objc4-818.2. Also `NCSA` (the llvm-*.h headers), `Apache-2.0`, `Apache-2.0 WITH LLVM-exception`, `BSD-2-Clause`, `HPND`, `LicenseRef-Public-Domain` |
| `darwin/src/Libraries/Security/apple/**` | `APSL-2.0` | Security-59754.120.12. NSS code in OSX/libsecurity_asn1 is `MPL-1.1` (see Decisions). Also `APSL-1.2`, `BSD-3-Clause`, `HPND` |
| `darwin/src/Libraries/Security/corecrypto/**` | `Apache-2.0` | kido's corecrypto glue for Security |
| `darwin/src/Libraries/Security/{certificates,compat,include,tools}/**`, `darwin/src/Libraries/Security/Security.exports` | `BSD-2-Clause` | PureDarwin. Also `MPL-2.0`, `MPL-2.0 OR BSD-2-Clause`, and `APSL-2.0` (copies of three xnu headers). kido's compat/kido_certificate_legacy.c is `Apache-2.0` |
| `darwin/src/Libraries/CommonCrypto/**` | `APSL-2.0` | CommonCrypto-60178.120.3. PureDarwin's rewrites are `MPL-2.0 OR BSD-2-Clause` |
| `darwin/src/Libraries/libresolv/**` | `APSL-2.0` | libresolv-68.120.2. Also `ISC`, `BSD-2-Clause`, `BSD-3-Clause`, `BSD-4-Clause-UC` |
| `darwin/src/Libraries/syslog/**` | `APSL-2.0` | syslog-385. Also `BSD-3-Clause`, `BSD-4-Clause-UC` |
| `darwin/src/Libraries/{SystemConfiguration,libSystemConfiguration}/**` | `APSL-2.0` | configd-1109.120.1 |
| `darwin/src/Libraries/bootp/**` | `APSL-2.0` | bootp-413.100.4. Also `BSD-4-Clause-UC` |
| `darwin/src/Libraries/IOKit/**` | `APSL-2.0` | IOKitUser-1845.120.6. iokituser/IOCFUnserialize.tab.c is `GPL-2.0-or-later WITH Bison-exception-2.2`; PureDarwin's files are `BSD-2-Clause`, `MPL-2.0` or `MPL-2.0 OR BSD-2-Clause` |
| `darwin/src/Libraries/mDNSResponder/**` | `Apache-2.0` | mDNSResponder-1310.120.71. Also `BSD-3-Clause` and `OpenSSL` (mDNSCore/DNSDigest.c) |
| `darwin/src/Libraries/{architecture,AvailabilityVersions,eap8021x,ppp}/**` | `APSL-2.0` | architecture-279, AvailabilityVersions-157.2, eap8021x-304.100.1 and ppp-884 headers. Also `Apache-2.0`, `BSD-4-Clause-UC`, `CMU-Mach` |
| `darwin/src/Libraries/{CoreServices,CrashReporterClient,SymptomReporter,libDER,libdarwin,libsandbox,libsystem_trace}/**` | `BSD-2-Clause` | PureDarwin. Also `MPL-2.0`, `MPL-2.0 OR BSD-2-Clause`, `MIT` (Zoe Knox's libsystem_trace, whose headers also say `BSD-2-Clause`) and `APSL-2.0` (two Apple headers in libdarwin) |
| `darwin/src/Libraries/{clang_bootstrap,libcxx,libcxxabi,libunwind,llvm-libc}/**` | `Apache-2.0 WITH LLVM-exception` | LLVM 21.1.8. Also `Unicode-3.0` (libcxx's Unicode data), `MIT`, `LicenseRef-Public-Domain` |
| `darwin/src/Frameworks/CoreFoundation/**` | `Apache-2.0 WITH Swift-exception` | swift-corelibs-foundation 5.0.3. PureDarwin's files are `BSD-2-Clause`, `MPL-2.0` or `MPL-2.0 OR BSD-2-Clause` |
| `darwin/src/Frameworks/Foundation/**` | `MPL-2.0` | PureDarwin, under the directory's own LICENSE. Also `MPL-2.0 OR BSD-2-Clause`, `MIT` |
| `darwin/src/Frameworks/Metal/**` | `Apache-2.0` | kido's open-source Metal.framework backed by virtio-metal |
| `darwin/src/Userspace/{diskdev_cmds,killall,ping,pkill,ps,shell_cmds,system_cmds,text_cmds}/**` | `APSL-2.0` | Apple's diskdev_cmds-757, shell_cmds-216.60.1, text_cmds-106, adv_cmds-174.0.1, network_cmds-606.140.1, system_cmds-880.120.1. Most files carry `BSD-2-Clause`, `BSD-3-Clause`, `BSD-4-Clause` or `BSD-4-Clause-UC`; also `APSL-1.0`, `ISC`, `LicenseRef-Public-Domain`. PureDarwin's files are `BSD-2-Clause` or `MPL-2.0 OR BSD-2-Clause` |
| `darwin/src/Userspace/gpt/**` | `BSD-2-Clause` | NetBSD's gpt. Also `BSD-3-Clause`, `BSD-4-Clause` |
| `darwin/src/Userspace/{doubleagentd,ext4tool,ext4util,mdnsd,netsetup,kido-dmesg-save,sh,sw_vers}/**` | `BSD-2-Clause` | PureDarwin. Also `MPL-2.0 OR BSD-2-Clause` |
| `darwin/tools/cctools/**` | `APSL-2.0` | Build only. cctools-port's cctools-949.0.1 and ld64-609 as changed by PureDarwin. Also `APSL-1.1`, `GPL-1.0-or-later` and `GPL-2.0-or-later` (the as assembler, include/coff, include/opcode, otool/arm_disasm.c), `BSD-2-Clause-FreeBSD`, `BSD-3-Clause`, `BSD-4-Clause-UC`, `CMU-Mach`, `CDDL-1.0`, `HPND`, `ISC`, `MIT`, `NCSA`, `Zlib`, `LicenseRef-Public-Domain` |
| `darwin/tools/dtrace_ctf/**` | `CDDL-1.0` | Build only. dtrace-370.40.1's CTF tools. Also `LGPL-2.1-only` (libdwarf), `APSL-1.1`, `Apache-2.0 WITH LLVM-exception`, `NCSA` |
| `darwin/tools/mig/**` | `APSL-2.0` | Build only. bootstrap_cmds-138's mig. Also `APSL-1.0`, `CMU-Mach` |
| `darwin/tools/xar/**` | `BSD-3-Clause` | Build only. xar. Also `BSD-2-Clause` |
| `darwin/tools/unifdef/**` | `BSD-2-Clause` | Build only. Tony Finch's unifdef |
| `darwin/tools/{iig-selfhost,riscv64-macho}/**`, `darwin/tools/apfs_fixture.c` | `BSD-2-Clause` | Build only. PureDarwin |

## What the licenses ask of us

Source means this repository; binaries means a built image or any file from one.

| License | Source | Binaries |
|---|---|---|
| `APSL-2.0` | Keep Apple's notices and a copy of the license. Changed files carry a notice saying so; docs/upstream.md gives the changes and dates. Publish the source of our changes for as long as we ship them, and 12 months at least | A prominent notice, in the code and in the documentation, that the source is available under the APSL and where to get it |
| `Apache-2.0`, `Apache-2.0 WITH LLVM-exception`, `Apache-2.0 WITH Swift-exception` | Keep the notices and a copy of the license. Changed files carry a notice saying so | Give a copy of the license and the attributions in NOTICE. The LLVM and Swift exceptions waive this only for their code that compiling other code embeds in a binary, such as inline header code and compiler-rt builtins |
| `MPL-2.0`, `MPL-1.1` | Keep the notices. MPL 1.1 also wants a file describing our changes and their dates: docs/upstream.md | Make the source available and tell recipients where it is |
| `CDDL-1.0` | Keep the notices; changed files name who changed them | Make the source available and tell recipients where it is |
| `LGPL-2.1-or-later` | Nothing: no source here is under it | gnu-efi's inc/subst/elf.h in the EFI loader, as the `LGPL-3.0-or-later` row says |
| `GPL-1.0-or-later`, `GPL-2.0-or-later`, `LGPL-2.1-only` | Keep the notices; changed files carry a notice and the date | None: these are build-only tools that do not ship |
| `GPL-2.0-or-later WITH Bison-exception-2.2` | Keep the notices | None: the exception lets the Bison-generated parsers ship under their surrounding license |
| `GPL-2.0 WITH Linux-syscall-note` | Keep the notices | None: fuse.h is used under its BSD-2-Clause option |
| `BSD-4-Clause`, `BSD-4-Clause-UC`, `OpenSSL`, `SSLeay-standalone` | Keep the notices | Reproduce the notices, and print the acknowledgements in NOTICE wherever the software is advertised. The University of California withdrew its advertising clause in 1999 |
| `BSD-2-Clause`, `BSD-2-Clause-FreeBSD`, `BSD-2-Clause-Patent`, `BSD-3-Clause`, `0BSD`, `MIT`, `ISC`, `HPND`, `CMU-Mach`, `dtoa`, `NCSA`, `Zlib`, `Brian-Gladman-3-Clause`, `Unicode-3.0`, `X11-distribute-modifications-variant`, `curl`, `LicenseRef-zsh`, `LicenseRef-ICU`, `LicenseRef-gnu-efi-efilib` | Keep the notices | Reproduce the copyright notices and license texts in the documentation (LICENSES/ and NOTICE) |
| `LGPL-3.0-or-later`, `GPL-3.0-or-later` | Nothing: no source here is under them | gnu-efi's inc/subst/elf.h, LGPL-2.1-or-later, reaches the EFI loader under section 3 of LGPL 3.0: ship both texts with it |
| `blessing`, `LicenseRef-Public-Domain` | Nothing | Nothing |

Change notices are one comment near the top of each changed file's license header, with the
years of the changes, such as "Modified by the PureDarwin project in 2021-2026 and the kido authors
in 2026; see docs/upstream.md." PureDarwin's years are those of the commits in its history that
touch the file. Files with Apple's copyright that match no release docs/upstream.md compared say
"May have been modified". These changed files cannot carry a comment and are listed instead:

- property lists: darwin/src/Kernel/Extensions/{AppleFileSystemDriver,IONetworkingFamily,
  IOPCIFamily,IOPCIFamily/ApplePIODMA,IOStorageFamily}/Info.plist,
  darwin/src/Kernel/Extensions/pthread/pthread-Info.plist,
  darwin/src/Libraries/SystemConfiguration/SystemConfiguration.fproj/Info.plist,
  darwin/src/Libraries/SystemConfiguration/configd.tproj/com.apple.configd.plist,
  darwin/src/Libraries/XPC/launchd/SystemStarter/com.apple.SystemStarter.plist and
  darwin/src/Libraries/XPC/notify/com.apple.notifyd.plist;
- the license text darwin/src/Frameworks/CoreFoundation/LICENSE.

## Third-party code in the image

Built from upstream sources, not stored in this repository: nixpkgs as flake.lock pins it, the
sources the recipes in nix/darwin/third-party pin by hash, and kido for its agent.

| Component | Version | License | Where |
|---|---|---|---|
| LibreSSL | 3.3.6 | `OpenSSL`, `SSLeay-standalone`, `ISC`, `BSD-3-Clause`, `LicenseRef-Public-Domain` | /usr/lib/libcrypto.46.dylib, libssl.48.dylib and libtls.20.dylib, /usr/bin/openssl, /usr/bin/ocspcheck, and /etc/ssl/openssl.cnf and x509v3.cnf |
| SQLite | 3.54.1 | `blessing` | /usr/lib/libsqlite3.dylib, which Security.framework links |
| zsh | 5.9.1 | `LicenseRef-zsh` | /bin/zsh. The GPL-2.0 completion functions are removed |
| ncurses | 6.6 | `X11-distribute-modifications-variant` | /usr/lib/libncurses.5.4.dylib, libpanel.5.4, libmenu.5.4 and libform.5.4; the terminfo database in /usr/share/terminfo, compiled from its terminfo.src; and linked into zsh |
| curl | 8.23.0 | `curl` | /usr/lib/libcurl.4.dylib |
| nghttp2 | 1.71.0 | `MIT` | /usr/lib/libapple_nghttp2.dylib |
| Expat | 2.11.0 | `MIT` | /usr/lib/libexpat.1.dylib |
| libedit | 20251016-3.1 | `BSD-3-Clause`, `BSD-2-Clause` | /usr/lib/libedit.3.dylib |
| Apple's libiconv, the Citrus iconv of FreeBSD and NetBSD | libiconv-115.120.2 | `BSD-2-Clause`, `BSD-3-Clause`, `BSD-4-Clause`, `BSD-2-Clause-FreeBSD`, `APSL-2.0`, `Unicode-3.0` | /usr/lib/libiconv.2.dylib, libcharset.1.dylib, the converters in /usr/lib/i18n, /usr/bin/iconv, and the tables in /usr/share/i18n, compiled from the mapping data of Unicode, Inc., Apple, the Citrus Project and others, whose notices NOTICE reproduces |
| Apple's libutil | libutil-73 | `APSL-2.0`, `BSD-2-Clause-FreeBSD`, `BSD-2-Clause`, `BSD-3-Clause` | /usr/lib/libutil.dylib |
| ICU | 76.1 | `LicenseRef-ICU` | /usr/lib/libicucore.A.dylib and the libicu* libraries |
| zlib | 1.3.2 | `Zlib` | /usr/lib/libz.1.dylib, and linked into toybox |
| bzip2 | 1.0.8 | `bzip2-1.0.6` | /usr/lib/libbz2.1.0.dylib |
| xz | 5.8.3 | `0BSD` | /usr/lib/liblzma.5.dylib |
| toybox | 0.8.13 | `0BSD` | /bin/toybox and its links |
| libffi | 3.5.2 | `MIT` | Linked into Foundation |
| libxml2 | 2.15.3 | `MIT` | Linked into Foundation |
| Mozilla CA certificates (NSS) | 3.125 | `MPL-2.0` | /etc/ssl/cert.pem, /System/Library/Security/Certificates.bundle |
| iana-etc | 20251215 | `MIT` | /etc/services, /etc/protocols |
| LLVM compiler-rt builtins | 21.1.8 | `Apache-2.0 WITH LLVM-exception` | Linked into the libraries that need it |
| kido-agent (github.com/kido-sh/kido) | | `Apache-2.0`, Rust standard library `MIT`, `Apache-2.0`, `Apache-2.0 WITH LLVM-exception`, and Cargo crate dependencies (`MIT`, `Apache-2.0`, `BSD-3-Clause`, `Unicode-3.0`) | /usr/libexec/kido-agent |
| gnu-efi | 4.0.2 | `LicenseRef-gnu-efi-efilib`, `BSD-3-Clause` (its GPL-2.0-or-later OR BSD-3-Clause files taken under BSD), `BSD-2-Clause-Patent` | Linked into the EFI loader on the ESP |

xnu-loader (`BSD-3-Clause`) is the boot loader beside the disk and, as BOOTAA64.EFI, on the ESP.
The image carries NOTICE and these texts in /usr/share/doc/kido (licenses/SPDX), beside each
component's own license file.

## Removed, not relicensed

These were in the PureDarwin tree and are not in this repository:

- Darling's GPL-3.0 code: libxpc's xpc_error.c (replaced by kido's xpc_errors.c and clean-room
  helpers and headers), the corecrypto Security.framework built on (replaced by the MPL-2.0
  corecrypto plus kido's glue over LibreSSL), and cmake/circular.cmake.
- xnu's built-in corecrypto under Apple's Internal Use License, which forbids redistribution: 89
  files in osfmk/corecrypto, osfmk/arm64/corecrypto and EXTERNAL_HEADERS/corecrypto. The kernel
  builds the same functions from darwin/external/corecrypto plus kido's HMAC_DRBG.
- The XPC SDK headers that carried "All rights reserved" and no grant, replaced by clean-room
  headers.
- Hardware drivers and firmware the virtual machine never uses, among them the PowerVR driver with
  Imagination's binary-only firmware, PDBcm2835SD, IOGraphicsFamily with its GMetric and GTrace
  sources, the FreeBSD NIC ports, the Wi-Fi drivers, and the fbsdkpi layer with Be Incorporated's
  PCI.h, which carries no grant.
- zsh's GPL-2.0 completion functions, left out of the image, and GNU libiconv, which macOS's own
  iconv from Apple's libiconv replaced.

## corecrypto

darwin/external/corecrypto is PureDarwin's interim fork of NeoDarwin's corecrypto
(github.com/NeoDarwinOS/corecrypto), `MPL-2.0`. Its README says two things that hold here too:

- It has not been audited, fuzzed or checked for side channels and must not be relied on to
  protect anything. The kernel's early random numbers, corecrypto.kext, libSystem's symmetric
  primitives and Security.framework's TLS certificate checks run on it.
- Its changes were written with an LLM and must never be offered to NeoDarwin, merged into its
  repository, or presented as NeoDarwin code. Take anything wanted from NeoDarwin's repository
  directly.
