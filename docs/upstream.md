# Upstream bases

Where this repository's code comes from, what was taken and left out, what kido changed, and which
of Apple's releases each component was compared with. The per-file list of changes is
[upstream-files.md](upstream-files.md); licenses are in [LICENSING.md](../LICENSING.md).

## Bases

| Part | Upstream | Base |
|---|---|---|
| darwin/ | PureDarwin, https://github.com/PureDarwin/PureDarwin | Branch upstream-xnu-12377 at c713d8fb9b048a38e2242d67e29b9957ced8c495 (2026-11-11) and commit a08eb9b6b42f4222b7383c14e9d4ac671fbd5128 (2026-11-13), then 29 kido commits on https://github.com/cocoa-xu/PureDarwin, branch darwin-container, at 0255e0ea33f0b9714c70098fd449de4814c31259 |
| darwin/external/corecrypto | NeoDarwin's corecrypto (https://github.com/NeoDarwinOS/corecrypto) through PureDarwin's interim fork https://github.com/Vali0004/corecrypto | f577aafe4b9a89727968666237d20cc51d2f770d |
| loader/ | xnu-loader, https://github.com/PureDarwin/xnu-loader | 2e0ccce2bd88eec1194e78eaeb46a91601478555 (2026-10-19), then 2 kido commits on https://github.com/cocoa-xu/xnu-loader, branch darwin-container, at 170dc0e68c172aeeb90f95f1d172109cb11da93c; the subset the arm64 Image and the EFI app build |
| kc-tools/, iig-tools/ | https://github.com/PureDarwin/kc-tools and https://github.com/PureDarwin/iig-tools | 05e5ed6a42533cfc4330559484e74b6dedeae51e and 28010c688caab7eb5400b9c98faa5b1e538e0108 |
| Third-party packages | nixpkgs, pinned in flake.lock | Listed in LICENSING.md |

Most of PureDarwin's tree is Apple's open-source code; "Releases compared" below names the release
of each component.

## What was taken

From PureDarwin's tree, what a container on Apple silicon builds and boots: xnu (RELEASE, VIRT)
with libfirehose_kernel and libkmod; the 20 kexts the virtual machine loads; libSystem and the
runtime libraries (dyld, libc++, libunwind, objc4, libxpc and launchd, Libinfo, notify,
mDNSResponder, syslog, libresolv, CommonCrypto); CoreFoundation, Foundation, Security,
SystemConfiguration and CoreServices; the userland a container runs; the build tools (cctools and
ld64, mig, xar, unifdef, the CTF tools); PureDarwin's CMake build; and its Nix code, reorganized
under nix/. corecrypto comes from the pinned fork commit.

## What was left out, and why

- **License blockers**, replaced rather than kept: Darling's GPL-3.0 code (libxpc's xpc_error.c,
  the corecrypto Security.framework used, cmake/circular.cmake), xnu's corecrypto under Apple's
  Internal Use License, and the XPC SDK headers that carried no license grant. LICENSING.md
  describes the replacements.
- **Hardware and boards a virtual machine never uses**: the x86_64 and armv6 builds, the phone and
  single-board-computer board support, and drivers and firmware such as PowerVR (with Imagination's
  binary-only firmware), PDBcm2835SD, IOGraphicsFamily with GMetric and GTrace, the FreeBSD NIC
  ports, the Wi-Fi drivers and the fbsdkpi layer.
- **GUI, graphics, audio, USB and Xorg**, the GUI and Wi-Fi libraries, libapfsrw.
- **Test suites and their data**, among them 151 MiB of MallocBench traces.
- **From the image**: headers, static libraries, configd, ipconfig, the kernel and kext copies,
  zsh's GPL-2.0 completion functions and run-help, and libiconv's GPL-3.0 iconv program.

## kido's changes

All between 2026-11-18 and 2026-11-24, by the kido authors.

On PureDarwin's tree, before the import (cocoa-xu/PureDarwin, branch darwin-container):

- Build on aarch64-linux hosts (xnu Makefile, SETUP/decomment).
- GIC: trust the device tree's gic-v3; put SPIs in Group 1 on GICv3 (PDArmGIC, xnu sleh.c).
- Boot: pd_rtc_wait, pd_quiet_wait and pd_rtc_delay boot-args (IOStartIOKit.cpp, launchctl.c,
  PDArmPlatformExpert).
- Virtualization.framework: no fixed panic-log region, ECAM and window from the device tree,
  VIRTIO_NET_F_MTU (QEMUVIRT.h, PDArmPCI, IOVirtIONet).
- Network: en0 named without configd, RX on INTx with 256-entry rings, netsetup from boot-args,
  mdnsd taking the boot-arg DNS server and reloading resolv.conf on SIGHUP (IOVirtIONet,
  netsetup.c, mDNSPosix.c, PosixDaemon.c).
- libSystem and lookups: vm_page_size from the process's page size, flat files before mdns,
  ___chkstk_darwin keeps its ret (pd_libSystem_init.c, search_module.c, pd_chkstk_darwin.s).
- Container mode: every uid served from the system bootstrap, /tmp kept across boots, no console
  login (launchd core.c, runtime.c, runtime.h, launchctl.c).
- TLS and XPC: XPC data objects described instead of aborting, SecTrustCopyCertificateChain
  (xpc_misc.c, pd_security_macos_trust.c).
- Nix: the arm64 virt container image, its 8 GiB disk, corecrypto from external/corecrypto, no
  implementation files in the SDK, the loader from kido's fork.

In this repository:

- The import of the build set above, a root flake, and the Xorg patches the image build read.
- xnu's corecrypto replaced by the MPL-2.0 corecrypto and kido's HMAC_DRBG
  (osfmk/prng/hmac_drbg.c), with the changes this needed in IOHibernatePrivate.h, der_vm.h,
  ipc_space.h, prng_random.c, osfmk/conf/files, files.arm64, Makefile.template, the sanitizer
  lists and cmake/MakeInc.def.in.
- libxpc without Darling's code: clean xpc_errors.c, xpc_abort.c and xpc_codesign.c, and
  clean-room XPC and launch.h headers.
- Security.framework on the external corecrypto, with kido's glue in Security/corecrypto over
  LibreSSL. Darling's corecrypto is removed.
- Guest branding (KidoDarwin, hostname kido), no PureDarwin logo in the loader, and no GPL
  programs, headers, static libraries, configd or ipconfig in the image.
- kido's names where PureDarwin used its own: the kido_* boot-args, the sh.kido.* launchd jobs,
  bundle identifiers and other identifiers, the kido-* helper programs, the KIDO_* environment
  variables, KidoDarwin in product, policy and driver names and in the EFI panic directory, and log
  messages without a PureDarwin prefix.
- What macOS command-line tools import and the guest lacked: nexttoward, the arm64 fenv with
  macOS's ABI, libplatform's arm64 ucontext functions, getsectdata, ctermid_r, forkpty,
  getattrlistbulk, getitimer, preadv, pwritev, _os_log_send_and_compose_impl, _os_crash_msg, the
  signpost calls, and Security's answers for a system without a default keychain; os_log and
  syslog(3) messages recorded by PureDarwin's logd in /var/log/system.log; tests in test/libsystem.
- macOS's libcurl, libedit and ncurses in /usr/lib: libcurl.4, libedit.3, libncurses.5.4,
  libpanel.5.4, libmenu.5.4 and libform.5.4 with macOS's install names, versions and exports, built
  from nixpkgs' curl 8.21.0 over the image's LibreSSL and /etc/ssl/cert.pem, libedit 20251016-3.1
  and ncurses 6.6; ncurses keeps both of macOS's cchar_t layouts (the plain names and the $NCURSES60
  ones), exports the four ncurses 6.0 internals that later releases dropped or made static, and
  installs its terminfo database; tests in test/syslibs.
- Pointer authentication as macOS has it. dyld, libSystem, libobjc, libc++, libc++abi, libz.1, and
  libncurses.5.4 with libpanel, libmenu and libform are arm64e only. So are three new libraries with
  macOS's install names, versions and exports: libutil, libbz2.1.0 built from bzip2 1.0.8, and
  liblzma.5 built from xz 5.8.3. libutil's humanize_number comes from NetBSD and the rest of libutil
  is new code. Every exec of an arm64e program gets its own IA, IB, DA, DB and GA keys, and a forked
  child keeps the keys of its parent. An arm64 process runs with pointer authentication off, so arm64
  programs work on the arm64e libraries. The kernel loads the keys on the way back to user space. PAC
  instructions run natively on a CPU with FEAT_PAuth instead of being rewritten at page-in, and a
  failed authentication raises EXC_BAD_ACCESS. When the guest starts at EL2, the loader and the
  secondary CPUs let the kernel and user space use pointer authentication. dyld loads any arm64e
  image, and the Blocks runtime signs blocks as the compiler does. libobjc is built with LLVM's
  clang, which does not define __PTRAUTH_INTRINSICS__ the way Apple's clang does, so the build
  defines it by hand. <ptrauth.h> has the names of Apple's arm64e ABI that Darwin's sources use.
  libSystem exports Libc's getbsize and strtonum and macOS's typed allocation calls, which Apple's ls
  and grep import. The tests are in test/arm64e, test/syslibs and test/libsystem.
- The end of the physical aperture on VIRT (arm_vm_init.c): with ARM_LARGE_MEMORY and no PPL,
  physmap_end and the L1 entries the aperture takes below the kernel's address range leave room
  for the block alignment of its segments, as XNU without ARM_LARGE_MEMORY does, so copies through
  the last pages of RAM no longer panic in copy_validate_kernel_addr; the test is in test/kernel.
- The whole panic log on VIRT's console (pe_init.c): without a panic region in the device tree,
  the panic log goes to a buffer in the kernel, so a panic prints its backtrace, the kexts in it
  and the loaded kexts. Then a virtual machine powers off instead of resetting
  (PDArmPlatformExpert), which stops it under VZ as well as QEMU; tested in test/kernel.
- macOS's LibreSSL: libcrypto.46, libssl.48 and libtls.20 in /usr/lib with macOS's install names,
  versions and exports, and the openssl and ocspcheck tools in /usr/bin. They are built from
  LibreSSL 3.3.6, the release macOS ships, with backports of LibreSSL's later security fixes.
  The libraries are arm64e, as on macOS. libcurl weak-links libssl and libcrypto as macOS's does.
  Security.framework's corecrypto glue, its SecureTransport, PKCS #12 import and CMS decoding run on
  them too, so the image carries no OpenSSL. SecureTransport offers TLS 1.0 to 1.2 with macOS's
  cipher list and error codes, as macOS's does. The tests are in test/ssl and test/security, which
  also runs its programs against macOS's own frameworks and compares the results.
- An APSL header on Ext4FileSystemDriver.cpp, which is derived from Apple's AppleFileSystemDriver.
- The Nix code reorganized under nix/ and the tree pruned to what the container builds.
- The licensing files and the change notices.

## Releases compared

Every file under darwin/ was compared, by git blob id, with the release trees below, fetched
read-only from https://github.com/apple-oss-distributions, https://github.com/apple/swift-corelibs-foundation
and https://github.com/llvm/llvm-project. Only xnu, Security, bootp, IOPCIFamily and IOStorageFamily
record their release in the tree; for the others, up to about five candidate tags per component were
compared and the one with the most byte-identical files was taken. Where tags tie, the one
PureDarwin's history names was taken (Libc-1439.141.1, libplatform-254.40.4), otherwise the newer
one.

- A file is unchanged when its bytes equal a file of any compared tag. xnu was also compared with
  xnu-7195.121.3, xnu-7195.141.2, xnu-7195.50.7.100.1 and xnu-6153.61.1, older bases PureDarwin
  used, because many headers elsewhere in the tree are copies of xnu's.
- A file is changed when its release has the same relative path, or a file of the same name that
  shares at least 60% of its lines (a copy).
- A file of which fewer than half the release's lines remain is PureDarwin's replacement, not a
  change, and carries no notice.

| Component | Release | Identical files | Other tags compared |
|---|---|---|---|
| xnu (kernel, libsyscall, libkmod) | xnu-12377.121.6 | 4520 of 5273 (85.7%) | see above |
| Libc | Libc-1439.141.1 | 1142 of 1785 (64.0%) | Libc-1752.120.2, Libc-1506.40.4, Libc-1439.100.3, Libc-1439.40.11 |
| libmalloc | libmalloc-317.140.5 | 63 of 80 (78.8%) | libmalloc-812.100.31, libmalloc-374.40.6, libmalloc-317.121.1 |
| libplatform | libplatform-254.40.4 | 102 of 127 (80.3%) | libplatform-375.120.2, libplatform-273.40.1, libplatform-254.80.2, libplatform-254.60.1 |
| libpthread | libpthread-454.80.2 | 119 of 142 (83.8%) | libpthread-539.100.4, libpthread-485.40.4, libpthread-454.120.2, libpthread-454.100.8 |
| libdispatch | libdispatch-1271.40.12 | 167 of 188 (88.8%) | libdispatch-1542.100.32, libdispatch-1324.41.2, libdispatch-1271.120.2, libdispatch-1271.100.5 |
| dyld | dyld-852.2 | 119 of 177 (67.2%) | dyld-1378, dyld-940, dyld-852, dyld-851.27 |
| Libinfo | Libinfo-542.40.3 | 32 of 49 (65.3%) | Libinfo-600, Libinfo-554, Libinfo-538 |
| Libnotify | Libnotify-279.40.4 | 15 of 25 (60.0%) | Libnotify-348.120.4, Libnotify-301, Libnotify-241.100.2 |
| launchd | launchd-842.92.1 | 21 of 81 (25.9%) | launchd-842.90.1, launchd-842.1.4 |
| objc4 | objc4-818.2 | 110 of 147 (74.8%) | objc4-951.7 |
| Security | Security-59754.120.12 | 615 of 709 (86.7%) | |
| CommonCrypto | CommonCrypto-60178.120.3 | 127 of 151 (84.1%) | CommonCrypto-600035 |
| libresolv | libresolv-68.120.2 | 31 of 39 (79.5%) | libresolv-96, libresolv-79 |
| syslog | syslog-385 | 26 of 40 (65.0%) | syslog-406 |
| configd | configd-1109.120.1 | 277 of 299 (92.6%) | configd-1405.120.5 |
| bootp | bootp-413.100.4 | 159 of 166 (95.8%) | |
| IOKitUser | IOKitUser-1845.120.6 | 91 of 121 (75.2%) | IOKitUser-100231.120.3 |
| mDNSResponder | mDNSResponder-1310.120.71 | 47 of 55 (85.5%) | mDNSResponder-2881.120.11 |
| architecture | architecture-279 | 20 of 22 (90.9%) | architecture-282 |
| AvailabilityVersions | AvailabilityVersions-157.2 | 17 of 30 (56.7%) | AvailabilityVersions-155, AvailabilityVersions-151 |
| ppp (headers) | ppp-1027.121.1 | 3 of 3 (100%) | ppp-884 |
| eap8021x (headers) | eap8021x-304.100.1 | 3 of 3 (100%) | eap8021x-368.120.2.0.1 |
| IOPCIFamily | IOPCIFamily-428.120.4 | 12 of 29 (41.4%) | |
| IOStorageFamily | IOStorageFamily-260.100.1 | 24 of 63 (38.1%) | |
| IONetworkingFamily | IONetworkingFamily-151.40.1 | 67 of 70 (95.7%) | IONetworkingFamily-188, IONetworkingFamily-160.40.1 |
| AppleFileSystemDriver | AppleFileSystemDriver-27 | 1 of 8 (12.5%) | AppleFileSystemDriver-31, -21, -17, -13 |
| IOHIDFamily (headers) | IOHIDFamily-1446.140.2 | 55 of 131 (42.0%) | IOHIDFamily-2238.120.5, -1787.40.10, -1633.120.12, -1633.40.25 |
| IOCDStorageFamily (headers) | IOCDStorageFamily-62 | 7 of 7 (100%) | IOCDStorageFamily-59 |
| IODVDStorageFamily (headers) | IODVDStorageFamily-46 | 6 of 6 (100%) | IODVDStorageFamily-43 |
| IOSCSIArchitectureModelFamily (headers) | IOSCSIArchitectureModelFamily-139.0.2 | 10 of 10 (100%) | IOSCSIArchitectureModelFamily-130.3.8 |
| IOSerialFamily (headers) | IOSerialFamily-93.200.2 | 8 of 8 (100%) | IOSerialFamily-91 |
| IOUSBFamily (headers) | IOUSBFamily-630.4.5 | 13 of 22 (59.1%) | IOUSBFamily-560.4.2, IOUSBFamily-540.4.1 |
| IOGraphics (one header) | IOGraphics-600 | 0 of 1 | IOGraphics-585.1 |
| shell_cmds (with killall) | shell_cmds-216.60.1 | 23 of 33 (69.7%) | shell_cmds-329, -234, -216.40.4, -207.100.1 |
| text_cmds | text_cmds-106 | 31 of 33 (93.9%) | text_cmds-199 |
| system_cmds | system_cmds-880.120.1 | 5 of 13 (38.5%) | system_cmds-1042.120.1, -970.0.4, -880.100.5, -854.100.3 |
| adv_cmds (ps, pkill) | adv_cmds-174.0.1 | 9 of 14 (64.3%) | adv_cmds-237, -178, -176.100.1, -176 |
| network_cmds (ping) | network_cmds-606.140.1 | 5 of 6 (83.3%) | network_cmds-741.100.2, -606.100.3, -596.100.2 |
| diskdev_cmds (mount, umount) | diskdev_cmds-757 | 0 of 3 | diskdev_cmds-667.100.2 |
| cctools (build only, by way of cctools-port) | cctools-949.0.1 | 315 of 814 (38.7%) | cctools-1035.1.102, -973.0.1, -927.0.2, -921 |
| ld64 (build only, by way of cctools-port) | ld64-609 | 59 of 143 (41.3%) | ld64-957.1, -711, -530, -512.4 |
| dtrace (CTF tools, build only) | dtrace-370.40.1 | 133 of 166 (80.1%) | |
| bootstrap_cmds (mig, build only) | bootstrap_cmds-138 | 25 of 29 (86.2%) | bootstrap_cmds-136, -133, -116 |
| xar (build only) | xar-404 | 13 of 51 (25.5%) | xar-503, -452, -420, -417.1 |
| swift-corelibs-foundation (CoreFoundation) | swift-5.0.3-RELEASE | 109 of 235 (46.4%) | swift-6.0.3, 5.10.1, 5.9.2, 5.5.3, 5.3.3 and 4.2.4 RELEASE |
| LLVM (libc++, libc++abi, libunwind, LLVM libc, clang headers) | llvmorg-21.1.8 | 2347 of 2362 (99.4%) | |

Low ratios mean a component is mostly PureDarwin's own files (AppleFileSystemDriver's shims,
diskdev_cmds' mount and umount) or was heavily changed (launchd, the cctools-port tree).

[upstream-files.md](upstream-files.md) lists the 1,291 files changed from these releases, with
the release file, who changed it, the lines added and removed, and whether the file carries a
change notice, then the 153 files with an Apple copyright that no compared release has, which say
"May have been modified", and loader/include/boot.h. Each notice gives the years of the changes:
PureDarwin's are the years of the commits in its public history that touch the file, kido's are
2026. LICENSING.md lists the 11 changed files that cannot carry a comment.
