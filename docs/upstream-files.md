# Files changed from their upstream release

Generated from the tree at the commit that added it. For every file whose upstream release has it (by path, or by name and at least 60% of its lines), the table gives the release file, who changed it, the lines added and removed against the release (not counting the notice), and whether the file carries a change notice. Dates are those of the last PureDarwin commit before the base and of the kido commits that changed the file; the notices give the years. docs/upstream.md says how the releases were matched and lists kido's changes.

1291 files in 39 releases; 1185 carry a notice.


## adv_cmds-174.0.1 (https://github.com/apple-oss-distributions/adv_cmds)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Userspace/ps/tasks.c | ps/tasks.c | PureDarwin (last 2026-08-30) | +28/-7 | notice |

## AppleFileSystemDriver-27 (https://github.com/apple-oss-distributions/AppleFileSystemDriver)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/Extensions/AppleFileSystemDriver/AppleFileSystemDriver.cpp | AppleFileSystemDriver.cpp | PureDarwin (last 2026-07-08) | +11/-7 | notice |
| darwin/src/Kernel/Extensions/AppleFileSystemDriver/Info.plist | Info.plist | PureDarwin (last 2026-07-08) | +4/-4 | no comment syntax for .plist |

## architecture-279 (https://github.com/apple-oss-distributions/architecture)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/tools/cctools/include/foreign/architecture/alignment.h | alignment.h (copy, 92% alike) | cctools-port and PureDarwin | +4/-3 | notice |
| darwin/tools/cctools/include/foreign/architecture/byte_order.h | byte_order.h (copy, 98% alike) | cctools-port and PureDarwin | +11/-11 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/alignment.h | i386/alignment.h (copy, 93% alike) | cctools-port and PureDarwin | +5/-5 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/asm_help.h | i386/asm_help.h (copy, 99% alike) | cctools-port and PureDarwin | +4/-4 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/byte_order.h | i386/byte_order.h (copy, 91% alike) | cctools-port and PureDarwin | +3/-3 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/cpu.h | i386/cpu.h (copy, 95% alike) | cctools-port and PureDarwin | +4/-4 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/desc.h | i386/desc.h (copy, 95% alike) | cctools-port and PureDarwin | +7/-7 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/io.h | i386/io.h (copy, 93% alike) | cctools-port and PureDarwin | +3/-3 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/pio.h | i386/pio.h (copy, 90% alike) | cctools-port and PureDarwin | +10/-10 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/reg_help.h | i386/reg_help.h (copy, 97% alike) | cctools-port and PureDarwin | +3/-3 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/sel.h | i386/sel.h (copy, 94% alike) | cctools-port and PureDarwin | +3/-3 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/table.h | i386/table.h (copy, 97% alike) | cctools-port and PureDarwin | +3/-3 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/tss.h | i386/tss.h (copy, 97% alike) | cctools-port and PureDarwin | +4/-4 | notice |

## AvailabilityVersions-157.2 (https://github.com/apple-oss-distributions/AvailabilityVersions)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/AvailabilityVersions/CMakeLists.txt | CMakeLists.txt | replaced by PureDarwin's own file | +44/-132 | none needed |
| darwin/src/Libraries/AvailabilityVersions/availability.dsl | availability.dsl | PureDarwin (last 2026-09-05) | +25/-0 | notice |
| darwin/src/Libraries/AvailabilityVersions/include/Availability.h | templates/Availability.h | PureDarwin (last 2026-09-05) | +74/-18 | notice |
| darwin/src/Libraries/AvailabilityVersions/include/AvailabilityInternal.h | templates/AvailabilityInternal.h | PureDarwin (last 2026-09-05) | +349/-14 | notice |
| darwin/src/Libraries/AvailabilityVersions/include/AvailabilityInternalPrivate.h | templates/AvailabilityInternalPrivate.h | PureDarwin (last 2026-09-05) | +10/-8 | notice |
| darwin/src/Libraries/AvailabilityVersions/include/AvailabilityVersions.h | templates/AvailabilityVersions.h | PureDarwin (last 2026-09-05) | +371/-3 | notice |

## bootp-413.100.4 (https://github.com/apple-oss-distributions/bootp)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/bootp/IPConfiguration.bproj/CGA.c | IPConfiguration.bproj/CGA.c | PureDarwin (last 2026-10-28) | +14/-3 | notice |
| darwin/src/Libraries/bootp/IPConfiguration.bproj/ipconfigd.c | IPConfiguration.bproj/ipconfigd.c | PureDarwin (last 2026-10-28) | +16/-13 | notice |
| darwin/src/Libraries/bootp/IPConfiguration.bproj/server.c | IPConfiguration.bproj/server.c | PureDarwin (last 2026-10-28) | +7/-7 | notice |

## bootstrap_cmds-138 (https://github.com/apple-oss-distributions/bootstrap_cmds)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/tools/mig/mig.sh | migcom.tproj/mig.sh | PureDarwin (last 2026-08-30) | +69/-42 | notice |

## cctools-949.0.1 (https://github.com/apple-oss-distributions/cctools)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/libdarwin/pd-compat-include/mach-o/getsect.h | include/mach-o/getsect.h (copy, 84% alike) | PureDarwin (last 2026-07-30) | +35/-14 | notice |
| darwin/tools/cctools/ar/archive.c | ar/archive.c | cctools-port and PureDarwin | +25/-4 | notice |
| darwin/tools/cctools/ar/archive.h | ar/archive.h | cctools-port and PureDarwin | +11/-3 | notice |
| darwin/tools/cctools/ar/contents.c | ar/contents.c | cctools-port and PureDarwin | +5/-2 | notice |
| darwin/tools/cctools/ar/misc.c | ar/misc.c | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/as/arm.c | as/arm.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/as/bignum.h | as/bignum.h | cctools-port and PureDarwin | +6/-0 | notice |
| darwin/tools/cctools/as/driver.c | as/driver.c | cctools-port and PureDarwin | +37/-9 | notice |
| darwin/tools/cctools/as/dwarf2dbg.c | as/dwarf2dbg.c | cctools-port and PureDarwin | +2/-1 | notice |
| darwin/tools/cctools/as/frags.c | as/frags.c | cctools-port and PureDarwin | +4/-2 | notice |
| darwin/tools/cctools/as/i386.c | as/i386.c | cctools-port and PureDarwin | +15/-0 | notice |
| darwin/tools/cctools/as/input-file.c | as/input-file.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/as/input-scrub.c | as/input-scrub.c | cctools-port and PureDarwin | +9/-6 | notice |
| darwin/tools/cctools/as/m88k.c | as/m88k.c | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/as/messages.c | as/messages.c | cctools-port and PureDarwin | +3/-2 | notice |
| darwin/tools/cctools/as/obstack.c | as/obstack.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/as/relax.h | as/relax.h | cctools-port and PureDarwin | +3/-0 | notice |
| darwin/tools/cctools/as/sections.c | as/sections.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/as/sparc.c | as/sparc.c | cctools-port and PureDarwin | +14/-0 | notice |
| darwin/tools/cctools/as/symbols.c | as/symbols.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/as/write_object.c | as/write_object.c | cctools-port and PureDarwin | +6/-0 | notice |
| darwin/tools/cctools/efitools/mtor.c | efitools/mtor.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/include/architecture/sparc/reg.h | include/architecture/sparc/reg.h | cctools-port and PureDarwin | +1/-1 | permissive needs no change notice |
| darwin/tools/cctools/include/foreign/architecture/i386/fpu.h | include/architecture/i386/fpu.h | cctools-port and PureDarwin | +20/-3 | notice |
| darwin/tools/cctools/include/foreign/architecture/i386/frame.h | include/architecture/i386/frame.h | cctools-port and PureDarwin | +9/-4 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/_structs.h | include/mach/i386/_structs.h | cctools-port and PureDarwin | +44/-484 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/fp_reg.h | include/mach/i386/fp_reg.h | cctools-port and PureDarwin | +0/-71 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/thread_state.h | include/mach/i386/thread_state.h | cctools-port and PureDarwin | +1/-1 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/thread_status.h | include/mach/i386/thread_status.h | cctools-port and PureDarwin | +26/-182 | notice |
| darwin/tools/cctools/include/foreign/mach/machine.h | include/mach/machine.h | cctools-port and PureDarwin | +194/-255 | notice |
| darwin/tools/cctools/include/foreign/mach/ppc/thread_state.h | include/mach/i386/thread_state.h (copy, 83% alike) | cctools-port and PureDarwin | +7/-7 | notice |
| darwin/tools/cctools/include/foreign/mach/ppc/thread_status.h | include/mach/ppc/thread_status.h | cctools-port and PureDarwin | +0/-6 | notice |
| darwin/tools/cctools/include/mach-o/loader.h | include/mach-o/loader.h | cctools-port and PureDarwin | +20/-0 | notice |
| darwin/tools/cctools/include/mach-o/swap.h | include/mach-o/swap.h | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/include/mach/arm/_structs.h | include/mach/arm/_structs.h | cctools-port and PureDarwin | +2/-2 | notice |
| darwin/tools/cctools/include/mach/arm/thread_state.h | include/mach/arm/thread_state.h | cctools-port and PureDarwin | +1/-1 | notice |
| darwin/tools/cctools/include/mach/arm/thread_status.h | include/mach/arm/thread_status.h | cctools-port and PureDarwin | +3/-3 | notice |
| darwin/tools/cctools/include/mach/machine.h | include/mach/machine.h | cctools-port and PureDarwin | +13/-2 | notice |
| darwin/tools/cctools/include/mach/ppc/_structs.h | include/mach/ppc/_structs.h | cctools-port and PureDarwin | +8/-4 | notice |
| darwin/tools/cctools/include/mach/ppc/thread_status.h | include/mach/ppc/thread_status.h | cctools-port and PureDarwin | +4/-2 | notice |
| darwin/tools/cctools/include/stuff/bytesex.h | include/stuff/bytesex.h | cctools-port and PureDarwin | +10/-1 | permissive needs no change notice |
| darwin/tools/cctools/include/stuff/write64.h | include/stuff/write64.h | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/libmacho/arch.c | libmacho/arch.c | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/libmacho/swap.c | libmacho/swap.c | cctools-port and PureDarwin | +13/-0 | notice |
| darwin/tools/cctools/libstuff/apple_version.c | libstuff/apple_version.c | replaced by PureDarwin's own file | +2/-12 | none needed |
| darwin/tools/cctools/libstuff/args.c | libstuff/args.c | cctools-port and PureDarwin | +2/-0 | notice |
| darwin/tools/cctools/libstuff/bytesex.c | libstuff/bytesex.c | cctools-port and PureDarwin | +21/-1 | permissive needs no change notice |
| darwin/tools/cctools/libstuff/checkout.c | libstuff/checkout.c | cctools-port and PureDarwin | +2/-1 | notice |
| darwin/tools/cctools/libstuff/dylib_roots.c | libstuff/dylib_roots.c | cctools-port and PureDarwin | +4/-2 | notice |
| darwin/tools/cctools/libstuff/execute.c | libstuff/execute.c | cctools-port and PureDarwin | +20/-0 | notice |
| darwin/tools/cctools/libstuff/get_arch_from_host.c | libstuff/get_arch_from_host.c | cctools-port and PureDarwin | +1/-1 | notice |
| darwin/tools/cctools/libstuff/llvm.c | libstuff/llvm.c | cctools-port and PureDarwin | +18/-0 | notice |
| darwin/tools/cctools/libstuff/lto.c | libstuff/lto.c | cctools-port and PureDarwin | +21/-2 | notice |
| darwin/tools/cctools/libstuff/macosx_deployment_target.c | libstuff/macosx_deployment_target.c | cctools-port and PureDarwin | +6/-0 | notice |
| darwin/tools/cctools/libstuff/ofile.c | libstuff/ofile.c | cctools-port and PureDarwin | +38/-2 | notice |
| darwin/tools/cctools/libstuff/seg_addr_table.c | libstuff/seg_addr_table.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/libstuff/swap_headers.c | libstuff/swap_headers.c | cctools-port and PureDarwin | +5/-0 | notice |
| darwin/tools/cctools/libstuff/write64.c | libstuff/write64.c | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/libstuff/writeout.c | libstuff/writeout.c | cctools-port and PureDarwin | +25/-3 | notice |
| darwin/tools/cctools/misc/bitcode_strip.c | misc/bitcode_strip.c | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/misc/install_name_tool.c | misc/install_name_tool.c | cctools-port and PureDarwin | +28/-2 | notice |
| darwin/tools/cctools/misc/libtool.c | misc/libtool.c | cctools-port and PureDarwin | +59/-1 | notice |
| darwin/tools/cctools/misc/lipo.c | misc/lipo.c | cctools-port and PureDarwin | +32/-2 | notice |
| darwin/tools/cctools/misc/nm.c | misc/nm.c | cctools-port and PureDarwin | +7/-0 | notice |
| darwin/tools/cctools/misc/redo_prebinding.c | misc/redo_prebinding.c | cctools-port and PureDarwin | +2/-1 | notice |
| darwin/tools/cctools/misc/segedit.c | misc/segedit.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/misc/strip.c | misc/strip.c | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/misc/vtool.c | misc/vtool.c | cctools-port and PureDarwin | +10/-1 | notice |
| darwin/tools/cctools/otool/arm64_disasm.c | otool/arm64_disasm.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/otool/dyld_bind_info.c | otool/dyld_bind_info.c | cctools-port and PureDarwin | +7/-0 | notice |
| darwin/tools/cctools/otool/i386_disasm.c | otool/i386_disasm.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/otool/main.c | otool/main.c | cctools-port and PureDarwin | +32/-7 | notice |
| darwin/tools/cctools/otool/ofile_print.c | otool/ofile_print.c | cctools-port and PureDarwin | +75/-0 | notice |
| darwin/tools/cctools/otool/ofile_print.h | otool/ofile_print.h | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/otool/print_bitcode.c | otool/print_bitcode.c | cctools-port and PureDarwin | +10/-3 | notice |
| darwin/tools/cctools/otool/print_objc.c | otool/print_objc.c | cctools-port and PureDarwin | +11/-5 | notice |
| darwin/tools/cctools/otool/print_objc2_64bit.c | otool/print_objc2_64bit.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/xar/include/xar.h | include/xar/xar.h (copy, 86% alike) | PureDarwin (last 2021-03-29) | +53/-20 | permissive needs no change notice |

## CommonCrypto-60178.120.3 (https://github.com/apple-oss-distributions/CommonCrypto)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/CommonCrypto/include/Private/CommonCrypto/CommonCollabKeyGen.h | include/Private/CommonCollabKeyGen.h | replaced by PureDarwin's own file | +41/-232 | none needed |
| darwin/src/Libraries/CommonCrypto/include/Private/CommonCrypto/CommonKeyDerivationSPI.h | include/Private/CommonKeyDerivationSPI.h | replaced by PureDarwin's own file | +42/-242 | none needed |
| darwin/src/Libraries/CommonCrypto/include/Private/CommonCrypto/CommonRandomSPI.h | include/Private/CommonRandomSPI.h | PureDarwin (last 2026-10-29) | +18/-82 | MPL-2.0 needs no change notice |
| darwin/src/Libraries/CommonCrypto/lib/CCCryptorReset_internal.h | lib/CCCryptorReset_internal.h | PureDarwin (last 2024-11-29) | +20/-1 | notice |
| darwin/src/Libraries/CommonCrypto/lib/CommonCMAC.c | lib/CommonCMAC.c | PureDarwin (last 2024-11-29) | +2/-0 | notice |
| darwin/src/Libraries/CommonCrypto/lib/CommonCollabKeyGen.c | lib/CommonCollabKeyGen.c | replaced by PureDarwin's own file | +74/-200 | none needed |
| darwin/src/Libraries/CommonCrypto/lib/CommonCryptorGCM.c | lib/CommonCryptorGCM.c | PureDarwin (last 2024-11-29) | +1/-0 | notice |
| darwin/src/Libraries/CommonCrypto/lib/CommonDH.c | lib/CommonDH.c | PureDarwin (last 2024-11-29) | +2/-0 | notice |
| darwin/src/Libraries/CommonCrypto/lib/CommonECCryptor.c | lib/CommonECCryptor.c | PureDarwin (last 2026-07-08) | +3/-2 | notice |
| darwin/src/Libraries/CommonCrypto/lib/CommonHMAC.c | lib/CommonHMAC.c | PureDarwin (last 2024-11-29) | +1/-0 | notice |
| darwin/src/Libraries/CommonCrypto/lib/CommonKeyDerivationSPI.c | lib/CommonKeyDerivationSPI.c | PureDarwin (last 2026-10-29) | +200/-332 | MPL-2.0 needs no change notice |
| darwin/src/Libraries/CommonCrypto/lib/CommonRSACryptor.c | lib/CommonRSACryptor.c | PureDarwin (last 2026-07-08) | +4/-2 | notice |
| darwin/src/Libraries/CommonCrypto/lib/CommonSymmetricKeywrap.c | lib/CommonSymmetricKeywrap.c | PureDarwin (last 2024-11-29) | +1/-0 | notice |
| darwin/src/Libraries/CommonCrypto/lib/ccGlobals.h | lib/ccGlobals.h | PureDarwin (last 2026-10-29) | +3/-1 | notice |
| darwin/src/Libraries/CommonCrypto/libcn/CommonBaseXX.c | libcn/CommonBaseXX.c | PureDarwin (last 2024-11-29) | +2/-2 | notice |
| darwin/src/Libraries/CommonCrypto/libcn/CommonBuffering.c | libcn/CommonBuffering.c | PureDarwin (last 2026-10-29) | +78/-76 | MPL-2.0 needs no change notice |
| darwin/src/Libraries/CommonCrypto/libcn/CommonCRC.c | libcn/CommonCRC.c | PureDarwin (last 2024-11-29) | +1/-1 | notice |

## configd-1109.120.1 (https://github.com/apple-oss-distributions/configd)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/SystemConfiguration/Plugins/InterfaceNamer/ifnamer.c | Plugins/InterfaceNamer/ifnamer.c | PureDarwin (last 2026-09-19) | +6/-6 | notice |
| darwin/src/Libraries/SystemConfiguration/SystemConfiguration.fproj/Info.plist | SystemConfiguration.fproj/Info.plist | PureDarwin (last 2026-10-28) | +3/-5 | no comment syntax for .plist |
| darwin/src/Libraries/SystemConfiguration/SystemConfiguration.fproj/SCDNotifierInformViaFD.c | SystemConfiguration.fproj/SCDNotifierInformViaFD.c | PureDarwin (last 2026-08-30) | +1/-0 | notice |
| darwin/src/Libraries/SystemConfiguration/configd.tproj/com.apple.configd.plist | configd.tproj/com.apple.configd.plist | PureDarwin (last 2026-11-10) | +16/-0 | no comment syntax for .plist |
| darwin/src/Libraries/SystemConfiguration/configd.tproj/configd.m | configd.tproj/configd.m | PureDarwin (last 2026-10-28) | +2/-2 | notice |
| darwin/src/Libraries/SystemConfiguration/configd.tproj/plugin_support.c | configd.tproj/plugin_support.c | PureDarwin (last 2026-10-28) | +49/-4 | notice |
| darwin/src/Libraries/libSystemConfiguration/dnsinfo/dnsinfo_copy.c | dnsinfo/dnsinfo_copy.c | PureDarwin (last 2026-08-08) | +16/-13 | notice |

## dtrace-370.40.1 (https://github.com/apple-oss-distributions/dtrace)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/tools/dtrace_ctf/include/gelf.h | lib/libelf/gelf.h (copy, 99% alike) | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/include/libelf.h | lib/libelf/libelf.h (copy, 99% alike) | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/include/sys/ctf_api.h | compat/opensolaris/sys/ctf_api.h | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_create.c | lib/libctf/common/ctf_create.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_decl.c | lib/libctf/common/ctf_decl.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_error.c | lib/libctf/common/ctf_error.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_hash.c | lib/libctf/common/ctf_hash.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_labels.c | lib/libctf/common/ctf_labels.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_lib.c | lib/libctf/common/ctf_lib.c | PureDarwin (last 2021-01-21) | +2/-1 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_lookup.c | lib/libctf/common/ctf_lookup.c | PureDarwin (last 2021-01-21) | +5/-5 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_open.c | lib/libctf/common/ctf_open.c | PureDarwin (last 2021-01-21) | +12/-12 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_subr.c | lib/libctf/common/ctf_subr.c | PureDarwin (last 2021-01-21) | +2/-2 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_types.c | lib/libctf/common/ctf_types.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libctf/ctf_util.c | lib/libctf/common/ctf_util.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libdwarf/dwarf_addr_finder.c | lib/libdwarf/dwarf_addr_finder.c | PureDarwin (last 2021-01-21) | +2/-2 | notice |
| darwin/tools/dtrace_ctf/libdwarf/dwarf_incl.h | lib/libdwarf/dwarf_incl.h | PureDarwin (last 2021-01-21) | +2/-2 | notice |
| darwin/tools/dtrace_ctf/libdwarf/dwarf_line.c | lib/libdwarf/dwarf_line.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libelf/begin.c | lib/libelf/begin.c | PureDarwin (last 2021-01-21) | +23/-23 | notice |
| darwin/tools/dtrace_ctf/libelf/cook.c | lib/libelf/cook.c | PureDarwin (last 2021-01-21) | +2/-2 | notice |
| darwin/tools/dtrace_ctf/libelf/data.c | lib/libelf/data.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libelf/decl.h | lib/libelf/decl.h | PureDarwin (last 2021-01-21) | +4/-4 | notice |
| darwin/tools/dtrace_ctf/libelf/error.c | lib/libelf/error.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libelf/getdata.c | lib/libelf/getdata.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libelf/getshstrndx.c | lib/libelf/getshstrndx.c | PureDarwin (last 2021-01-21) | +3/-3 | notice |
| darwin/tools/dtrace_ctf/libelf/input.c | lib/libelf/input.c | PureDarwin (last 2021-01-21) | +1/-1 | notice |
| darwin/tools/dtrace_ctf/libelf/xlate.c | lib/libelf/xlate.c | PureDarwin (last 2021-01-21) | +4/-4 | notice |
| darwin/tools/dtrace_ctf/libelf/xlate64.c | lib/libelf/xlate64.c | PureDarwin (last 2021-01-21) | +4/-4 | notice |
| darwin/tools/dtrace_ctf/tools/barrier.h | tools/ctfconvert/barrier.h | PureDarwin (last 2026-07-08) | +1/-1 | notice |

## dyld-852.2 (https://github.com/apple-oss-distributions/dyld)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/dyld/upstream/dyld3/APIs.cpp | dyld3/APIs.cpp | PureDarwin (last 2026-09-28) | +51/-5 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/Array.h | dyld3/Array.h | PureDarwin (last 2026-07-09) | +2/-1 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/ClosureFileSystemPhysical.cpp | dyld3/ClosureFileSystemPhysical.cpp | PureDarwin (last 2026-07-09) | +35/-2 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/Diagnostics.cpp | dyld3/Diagnostics.cpp | PureDarwin (last 2026-07-09) | +0/-1 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/MachOAnalyzer.cpp | dyld3/MachOAnalyzer.cpp | PureDarwin (last 2026-07-09) | +13/-11 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/MachOAppCache.cpp | dyld3/MachOAppCache.cpp | PureDarwin (last 2026-07-09) | +2/-0 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/MachOAppCache.h | dyld3/MachOAppCache.h | PureDarwin (last 2026-07-09) | +1/-0 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/MachOFile.cpp | dyld3/MachOFile.cpp | PureDarwin (last 2026-09-28) | +29/-9 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/MachOFile.h | dyld3/MachOFile.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/MachOLoaded.cpp | dyld3/MachOLoaded.cpp | PureDarwin (last 2026-07-09) | +14/-2 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/SharedCacheRuntime.cpp | dyld3/SharedCacheRuntime.cpp | PureDarwin (last 2026-09-28) | +6/-1 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/StartGlue.h | dyld3/StartGlue.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/SupportedArchs.h | dyld3/SupportedArchs.h | PureDarwin (last 2026-07-09) | +4/-1 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/shared-cache/FileUtils.cpp | dyld3/shared-cache/FileUtils.cpp | PureDarwin (last 2026-07-09) | +12/-4 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/shared-cache/IMPCaches.hpp | dyld3/shared-cache/IMPCaches.hpp | PureDarwin (last 2026-07-09) | +15/-9 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/shared-cache/OptimizerBranches.cpp | dyld3/shared-cache/OptimizerBranches.cpp | PureDarwin (last 2026-07-09) | +9/-7 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/shared-cache/StringUtils.h | dyld3/shared-cache/StringUtils.h | PureDarwin (last 2026-07-09) | +1/-1 | notice |
| darwin/src/Libraries/dyld/upstream/dyld3/shared-cache/kernel_collection_builder.h | dyld3/shared-cache/kernel_collection_builder.h | PureDarwin (last 2026-07-09) | +4/-0 | notice |
| darwin/src/Libraries/dyld/upstream/src/ImageLoader.cpp | src/ImageLoader.cpp | PureDarwin (last 2026-11-10) | +6/-1 | notice |
| darwin/src/Libraries/dyld/upstream/src/ImageLoader.h | src/ImageLoader.h | PureDarwin (last 2026-11-10) | +8/-16 | notice |
| darwin/src/Libraries/dyld/upstream/src/ImageLoaderMachO.cpp | src/ImageLoaderMachO.cpp | PureDarwin (last 2026-09-28) | +7/-1 | notice |
| darwin/src/Libraries/dyld/upstream/src/ImageLoaderMachOCompressed.cpp | src/ImageLoaderMachOCompressed.cpp | PureDarwin (last 2026-11-10) | +13/-0 | notice |
| darwin/src/Libraries/dyld/upstream/src/dyld2.cpp | src/dyld2.cpp | PureDarwin (last 2026-11-10) | +85/-4 | notice |
| darwin/src/Libraries/dyld/upstream/src/dyldAPIsInLibSystem.cpp | src/dyldAPIsInLibSystem.cpp | PureDarwin (last 2026-08-22) | +42/-13 | notice |
| darwin/src/Libraries/dyld/upstream/src/dyldExceptions.c | src/dyldExceptions.c | PureDarwin (last 2026-09-28) | +6/-0 | notice |
| darwin/src/Libraries/dyld/upstream/src/dyldInitialization.cpp | src/dyldInitialization.cpp | PureDarwin (last 2026-08-22) | +133/-2 | notice |
| darwin/src/Libraries/dyld/upstream/src/dyldLibSystemGlue.c | src/dyldLibSystemGlue.c | PureDarwin (last 2026-07-09) | +8/-1 | notice |
| darwin/src/Libraries/dyld/upstream/src/dyldStartup.s | src/dyldStartup.s | PureDarwin (last 2026-09-28) | +44/-0 | notice |
| darwin/src/Libraries/dyld/upstream/src/glue.c | src/glue.c | PureDarwin (last 2026-08-22) | +44/-5 | notice |
| darwin/src/Libraries/dyld/upstream/src/start_glue.s | src/start_glue.s | PureDarwin (last 2026-09-28) | +18/-1 | notice |
| darwin/src/Libraries/dyld/upstream/src/threadLocalHelpers.s | src/threadLocalHelpers.s | PureDarwin (last 2026-09-28) | +102/-0 | notice |
| darwin/src/Libraries/objc4/macosx.internal/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/mach-o/dyld_priv.h | include/mach-o/dyld_priv.h (copy, 85% alike) | PureDarwin (last 2026-07-24) | +82/-94 | notice |

## IOGraphics-600 (https://github.com/apple-oss-distributions/IOGraphics)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/Extensions/IOGraphicsFamily/IOKit/graphics/IOGraphicsTypes.h | IOGraphicsFamily/IOKit/graphics/IOGraphicsTypes.h (copy, 99% alike) | PureDarwin (last 2026-07-13) | +7/-20 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/include/IOKit/graphics/IOGraphicsTypes.h | IOGraphicsFamily/IOKit/graphics/IOGraphicsTypes.h (copy, 99% alike) | PureDarwin (last 2026-07-10) | +7/-20 | notice |

## IOHIDFamily-1446.140.2 (https://github.com/apple-oss-distributions/IOHIDFamily)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/AppleHIDUsageTables.h | IOHIDFamily/AppleHIDUsageTables.h | PureDarwin (last 2026-07-10) | +246/-0 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDDevice.h | IOHIDFamily/IOHIDDevice.h | PureDarwin (last 2026-07-13) | +1/-1 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDEventMacroDefs.h | IOHIDFamily/IOHIDEventMacroDefs.h | PureDarwin (last 2026-09-28) | +36/-36 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDEventService.h | IOHIDFamily/IOHIDEventService.h | PureDarwin (last 2026-07-13) | +1/-1 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDEventServiceFastPathUserClient.h | IOHIDFamily/IOHIDEventServiceFastPathUserClient.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDEventStructDefs.h | IOHIDFamily/IOHIDEventStructDefs.h | PureDarwin (last 2026-09-28) | +6/-6 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDFamilyPrivate.h | IOHIDFamily/IOHIDFamilyPrivate.h | PureDarwin (last 2026-08-13) | +3/-1 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDInterface.h | IOHIDFamily/IOHIDInterface.h | PureDarwin (last 2026-07-13) | +1/-1 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDLibUserClient.h | IOHIDFamily/IOHIDLibUserClient.h | PureDarwin (last 2026-08-31) | +29/-1 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDPrivateKeys.h | IOHIDFamily/IOHIDPrivateKeys.h | PureDarwin (last 2026-07-10) | +12/-0 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDFamily/IOHIDProperties.h | IOHIDFamily/IOHIDProperties.h | PureDarwin (last 2026-07-10) | +9/-0 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDSystem/IOKit/hidsystem/IOHIDDescriptorParser.h | IOHIDSystem/IOKit/hidsystem/IOHIDDescriptorParser.h | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/Extensions/IOHIDFamily/IOHIDSystem/IOKit/hidsystem/IOHIDSystem.h | IOHIDSystem/IOKit/hidsystem/IOHIDSystem.h | PureDarwin (last 2026-07-20) | +13/-0 | notice |

## IOKitUser-1845.120.6 (https://github.com/apple-oss-distributions/IOKitUser)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/IOKit/iokituser/IOKitLib.c | IOKitLib.c | PureDarwin (last 2026-11-10) | +66/-0 | notice |
| darwin/src/Libraries/IOKit/iokituser/hid.subproj/IOHIDLibPrivate.c | hid.subproj/IOHIDLibPrivate.c | PureDarwin (last 2026-08-31) | +12/-0 | notice |
| darwin/src/Libraries/IOKit/iokituser/hid.subproj/IOHIDManager.c | hid.subproj/IOHIDManager.c | PureDarwin (last 2026-11-10) | +8/-0 | notice |
| darwin/src/Libraries/IOKit/iokituser/include/IOKit/IOKitLib.h | IOKitLib.h | PureDarwin (last 2026-11-10) | +7/-0 | notice |
| darwin/src/Libraries/IOKit/iokituser/include/IOKit/hid/HIDDeviceIvar.h | hid.subproj/HIDDeviceIvar.h | PureDarwin (last 2026-08-31) | +1/-0 | notice |
| darwin/src/Libraries/IOKit/iokituser/include/IOKit/hid/IOHIDEvent.h | hid.subproj/IOHIDEvent.h | replaced by PureDarwin's own file | +16/-0 | none needed |
| darwin/src/Libraries/IOKit/iokituser/include/IOKit/hid/IOHIDLibPrivate.h | hid.subproj/IOHIDLibPrivate.h | PureDarwin (last 2026-08-31) | +17/-0 | notice |
| darwin/src/Libraries/IOKit/iokituser/include/IOKit/hid/IOHIDManagerPersistentProperties.h | hid.subproj/IOHIDManagerPersistentProperties.h | replaced by PureDarwin's own file | +88/-0 | none needed |
| darwin/src/Libraries/IOKit/iokituser/include/IOKit/hid/hidobjcbase.h | hid.subproj/hidobjcbase.h | PureDarwin (last 2026-08-31) | +1/-0 | notice |

## IONetworkingFamily-151.40.1 (https://github.com/apple-oss-distributions/IONetworkingFamily)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/Extensions/IONetworkingFamily/IONetworkStack.cpp | IONetworkStack.cpp | PureDarwin (last 2026-08-15) | +9/-0 | notice |
| darwin/src/Kernel/Extensions/IONetworkingFamily/Info.plist | Info.plist | PureDarwin (last 2026-07-15) | +1/-1 | no comment syntax for .plist |

## IOPCIFamily-428.120.4 (https://github.com/apple-oss-distributions/IOPCIFamily)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/Extensions/IOPCIFamily/ApplePIODMA/Info.plist | ApplePIODMA/Info.plist | PureDarwin (last 2026-07-08) | +1/-1 | no comment syntax for .plist |
| darwin/src/Kernel/Extensions/IOPCIFamily/IOPCIBridge.cpp | IOPCIBridge.cpp | PureDarwin (last 2026-09-28) | +30/-2 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/IOPCIConfigurator.cpp | IOPCIConfigurator.cpp | PureDarwin (last 2026-10-19) | +192/-28 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/IOPCIDevice.cpp | IOPCIDevice.cpp | PureDarwin (last 2021-11-03) | +2/-3 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/IOPCIMessagedInterruptController.cpp | IOPCIMessagedInterruptController.cpp | PureDarwin (last 2026-07-15) | +7/-1 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/IOPCIRange.cpp | IOPCIRange.cpp | PureDarwin (last 2026-07-15) | +8/-0 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/include/IOKit/apiodma/ApplePIODMADebug.h | ApplePIODMA/ApplePIODMADebug.h | PureDarwin (last 2021-11-03) | +2/-2 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/include/IOKit/apiodma/ApplePIODMADefinitions.h | ApplePIODMA/ApplePIODMADefinitions.h | PureDarwin (last 2025-06-13) | +3/-3 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/include/IOKit/pci/IOAGPDevice.h | IOKit/pci/IOAGPDevice.h | PureDarwin (last 2021-11-03) | +1/-1 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/include/IOKit/pci/IOPCIBridge.h | IOKit/pci/IOPCIBridge.h | PureDarwin (last 2026-09-28) | +3/-3 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/include/IOKit/pci/IOPCIConfigurator.h | IOKit/pci/IOPCIConfigurator.h | PureDarwin (last 2026-08-15) | +7/-0 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/include/IOKit/pci/IOPCIDevice.h | IOKit/pci/IOPCIDevice.h | PureDarwin (last 2021-11-03) | +2/-10 | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/include/IOKit/pci/IOPCIPrivate.h | IOKit/pci/IOPCIPrivate.h | PureDarwin (last 2026-07-15) | +8/-1 | notice |

## IOStorageFamily-260.100.1 (https://github.com/apple-oss-distributions/IOStorageFamily)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/Extensions/IOStorageFamily/IOFDiskPartitionScheme.cpp | IOFDiskPartitionScheme.cpp | PureDarwin (last 2026-08-22) | +35/-4 | notice |
| darwin/src/Kernel/Extensions/IOStorageFamily/IOGUIDPartitionScheme.cpp | IOGUIDPartitionScheme.cpp | PureDarwin (last 2026-07-08) | +4/-0 | notice |
| darwin/src/Kernel/Extensions/IOStorageFamily/IOMedia.cpp | IOMedia.cpp | PureDarwin (last 2026-10-11) | +27/-0 | notice |
| darwin/src/Kernel/Extensions/IOStorageFamily/IOMediaBSDClient.cpp | IOMediaBSDClient.cpp | PureDarwin (last 2026-08-30) | +257/-47 | notice |
| darwin/src/Kernel/Extensions/IOStorageFamily/Info.plist | Info.plist | PureDarwin (last 2026-10-11) | +5/-5 | no comment syntax for .plist |
| darwin/src/Kernel/Extensions/IOStorageFamily/include/IOKit/storage/IOMediaBSDClient.h | IOMediaBSDClient.h | PureDarwin (last 2026-07-18) | +16/-3 | notice |

## IOUSBFamily-630.4.5 (https://github.com/apple-oss-distributions/IOUSBFamily)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/Extensions/IOUSBFamily/include/IOKit/usb/IOUSBCommand.h | IOUSBFamily/Headers/IOUSBCommand.h | PureDarwin (last 2026-07-15) | +2/-0 | notice |
| darwin/src/Kernel/Extensions/IOUSBFamily/include/IOKit/usb/IOUSBController.h | IOUSBFamily/Headers/IOUSBController.h | replaced by PureDarwin's own file | +117/-0 | none needed |
| darwin/src/Kernel/Extensions/IOUSBFamily/include/IOKit/usb/IOUSBControllerV3.h | IOUSBFamily/Headers/IOUSBControllerV3.h | replaced by PureDarwin's own file | +6/-0 | none needed |
| darwin/src/Kernel/Extensions/IOUSBFamily/include/IOKit/usb/IOUSBHubDevice.h | IOUSBFamily/Headers/IOUSBHubDevice.h | PureDarwin (last 2026-07-16) | +1/-2 | notice |
| darwin/src/Kernel/Extensions/IOUSBFamily/include/IOKit/usb/IOUSBInterface.h | IOUSBFamily/Headers/IOUSBInterface.h | PureDarwin (last 2026-07-16) | +1/-0 | notice |
| darwin/src/Kernel/Extensions/IOUSBFamily/include/IOKit/usb/IOUSBLog.h | IOUSBFamily/Headers/IOUSBLog.h | replaced by PureDarwin's own file | +30/-0 | none needed |
| darwin/src/Kernel/Extensions/IOUSBFamily/include/IOKit/usb/USB.h | IOUSBFamily/Headers/USB.h | replaced by PureDarwin's own file | +302/-0 | none needed |
| darwin/src/Kernel/Extensions/IOUSBFamily/include/IOKit/usb/USBSpec.h | IOUSBFamily/Headers/USBSpec.h | PureDarwin (last 2026-07-15) | +1/-0 | notice |

## launchd-842.92.1 (https://github.com/apple-oss-distributions/launchd)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/XPC/launchctl/launchctl.c | support/launchctl.c (copy, 93% alike) | PureDarwin (last 2026-10-11); kido (2026-11-18, 2026-11-22) | +453/-201 | notice |
| darwin/src/Libraries/XPC/launchd/SystemStarter/SystemStarter.c | SystemStarter/SystemStarter.c | PureDarwin (last 2026-07-22) | +13/-26 | notice |
| darwin/src/Libraries/XPC/launchd/SystemStarter/com.apple.SystemStarter.plist | SystemStarter/com.apple.SystemStarter.plist | PureDarwin (last 2026-07-22) | +4/-0 | no comment syntax for .plist |
| darwin/src/Libraries/XPC/launchd/config.h | src/config.h | PureDarwin (last 2026-07-22) | +2/-0 | notice |
| darwin/src/Libraries/XPC/launchd/core.c | src/core.c | PureDarwin (last 2026-11-10); kido (2026-11-22) | +215/-28 | notice |
| darwin/src/Libraries/XPC/launchd/core.h | src/core.h | PureDarwin (last 2026-07-22) | +5/-0 | notice |
| darwin/src/Libraries/XPC/launchd/ipc.c | src/ipc.c | PureDarwin (last 2026-07-22) | +7/-0 | notice |
| darwin/src/Libraries/XPC/launchd/job.defs | src/job.defs | PureDarwin (last 2026-07-21) | +1/-0 | notice |
| darwin/src/Libraries/XPC/launchd/job_reply.defs | src/job_reply.defs | PureDarwin (last 2026-07-21) | +3/-3 | notice |
| darwin/src/Libraries/XPC/launchd/kill2.c | src/kill2.c | PureDarwin (last 2026-07-22) | +4/-1 | notice |
| darwin/src/Libraries/XPC/launchd/ktrace.c | src/ktrace.c | PureDarwin (last 2026-07-22) | +4/-6 | notice |
| darwin/src/Libraries/XPC/launchd/launchd.c | src/launchd.c | PureDarwin (last 2026-08-13) | +48/-17 | notice |
| darwin/src/Libraries/XPC/launchd/launchd.h | src/launchd.h | PureDarwin (last 2026-07-22) | +4/-2 | notice |
| darwin/src/Libraries/XPC/launchd/runtime.c | src/runtime.c | PureDarwin (last 2026-07-24); kido (2026-11-22) | +68/-4 | notice |
| darwin/src/Libraries/XPC/launchd/runtime.h | src/runtime.h | PureDarwin (last 2026-08-25); kido (2026-11-22) | +8/-1 | notice |
| darwin/src/Libraries/XPC/libxpc/helper.defs | src/helper.defs (copy, 89% alike) | PureDarwin (last 2026-07-21) | +2/-2 | notice |
| darwin/src/Libraries/XPC/libxpc/include/launch.h | liblaunch/launch.h | replaced by a clean-room kido file | +172/-304 | none needed |
| darwin/src/Libraries/XPC/libxpc/include/launch_internal.h | liblaunch/launch_internal.h | PureDarwin (last 2026-07-21) | +4/-2 | notice |
| darwin/src/Libraries/XPC/libxpc/libbootstrap.c | liblaunch/libbootstrap.c (copy, 98% alike) | PureDarwin (last 2026-07-21) | +8/-6 | notice |
| darwin/src/Libraries/XPC/libxpc/liblaunch.c | liblaunch/liblaunch.c (copy, 78% alike) | PureDarwin (last 2026-08-30) | +308/-364 | notice |
| darwin/src/Libraries/XPC/libxpc/libvproc.c | liblaunch/libvproc.c (copy, 98% alike) | PureDarwin (last 2026-11-10) | +33/-12 | notice |
| darwin/src/Libraries/XPC/libxpc/vproc_internal.h | liblaunch/vproc_internal.h (copy, 94% alike) | PureDarwin (last 2026-07-21) | +10/-5 | notice |

## ld64-609 (https://github.com/apple-oss-distributions/ld64)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/tools/cctools/ld64/src/abstraction/MachOFileAbstraction.hpp | src/abstraction/MachOFileAbstraction.hpp | cctools-port and PureDarwin | +33/-0 | notice |
| darwin/tools/cctools/ld64/src/create_configure | src/create_configure | cctools-port and PureDarwin | +2/-2 | notice |
| darwin/tools/cctools/ld64/src/ld/Architectures.hpp | src/ld/Architectures.hpp | cctools-port and PureDarwin | +10/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/HeaderAndLoadCommands.hpp | src/ld/HeaderAndLoadCommands.hpp | cctools-port and PureDarwin | +86/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/InputFiles.cpp | src/ld/InputFiles.cpp | cctools-port and PureDarwin | +38/-4 | notice |
| darwin/tools/cctools/ld64/src/ld/InputFiles.h | src/ld/InputFiles.h | cctools-port and PureDarwin | +2/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/LinkEdit.hpp | src/ld/LinkEdit.hpp | cctools-port and PureDarwin | +40/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/LinkEditClassic.hpp | src/ld/LinkEditClassic.hpp | cctools-port and PureDarwin | +229/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/Options.cpp | src/ld/Options.cpp | cctools-port and PureDarwin | +167/-24 | notice |
| darwin/tools/cctools/ld64/src/ld/Options.h | src/ld/Options.h | cctools-port and PureDarwin | +21/-9 | notice |
| darwin/tools/cctools/ld64/src/ld/OutputFile.cpp | src/ld/OutputFile.cpp | cctools-port and PureDarwin | +444/-1 | notice |
| darwin/tools/cctools/ld64/src/ld/OutputFile.h | src/ld/OutputFile.h | cctools-port and PureDarwin | +9/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/Resolver.cpp | src/ld/Resolver.cpp | cctools-port and PureDarwin | +10/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/Resolver.h | src/ld/Resolver.h | cctools-port and PureDarwin | +2/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/ResponseFiles.cpp | src/ld/ResponseFiles.cpp | cctools-port and PureDarwin | +11/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/Snapshot.cpp | src/ld/Snapshot.cpp | cctools-port and PureDarwin | +37/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/Snapshot.h | src/ld/Snapshot.h | cctools-port and PureDarwin | +4/-1 | notice |
| darwin/tools/cctools/ld64/src/ld/SymbolTable.cpp | src/ld/SymbolTable.cpp | cctools-port and PureDarwin | +15/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/SymbolTable.h | src/ld/SymbolTable.h | cctools-port and PureDarwin | +2/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/code-sign-blobs/blob.h | src/ld/code-sign-blobs/blob.h | cctools-port and PureDarwin | +12/-1 | notice |
| darwin/tools/cctools/ld64/src/ld/code-sign-blobs/endian.h | src/ld/code-sign-blobs/endian.h | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/code-sign-blobs/memutils.h | src/ld/code-sign-blobs/memutils.h | cctools-port and PureDarwin | +4/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/debugline.c | src/ld/debugline.c | cctools-port and PureDarwin | +1/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/ld.cpp | src/ld/ld.cpp | cctools-port and PureDarwin | +15/-2 | notice |
| darwin/tools/cctools/ld64/src/ld/ld.hpp | src/ld/ld.hpp | cctools-port and PureDarwin | +36/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/libcodedirectory.c | src/ld/libcodedirectory.c | cctools-port and PureDarwin | +7/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/libcodedirectory.h | src/ld/libcodedirectory.h | cctools-port and PureDarwin | +6/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/archive_file.cpp | src/ld/parsers/archive_file.cpp | cctools-port and PureDarwin | +31/-1 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/generic_dylib_file.hpp | src/ld/parsers/generic_dylib_file.hpp | cctools-port and PureDarwin | +3/-2 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/libunwind/AddressSpace.hpp | src/ld/parsers/libunwind/AddressSpace.hpp | cctools-port and PureDarwin | +7/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/libunwind/DwarfInstructions.hpp | src/ld/parsers/libunwind/DwarfInstructions.hpp | cctools-port and PureDarwin | +7/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/libunwind/InternalMacros.h | src/ld/parsers/libunwind/InternalMacros.h | cctools-port and PureDarwin | +7/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/libunwind/Registers.hpp | src/ld/parsers/libunwind/Registers.hpp | cctools-port and PureDarwin | +50/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/lto_file.cpp | src/ld/parsers/lto_file.cpp | cctools-port and PureDarwin | +58/-4 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/macho_dylib_file.cpp | src/ld/parsers/macho_dylib_file.cpp | cctools-port and PureDarwin | +100/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/macho_relocatable_file.cpp | src/ld/parsers/macho_relocatable_file.cpp | cctools-port and PureDarwin | +624/-8 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/textstub_dylib_file.cpp | src/ld/parsers/textstub_dylib_file.cpp | cctools-port and PureDarwin | +31/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/parsers/textstub_dylib_file.hpp | src/ld/parsers/textstub_dylib_file.hpp | cctools-port and PureDarwin | +3/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/bitcode_bundle.cpp | src/ld/passes/bitcode_bundle.cpp | cctools-port and PureDarwin | +26/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/branch_island.cpp | src/ld/passes/branch_island.cpp | cctools-port and PureDarwin | +93/-7 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/compact_unwind.cpp | src/ld/passes/compact_unwind.cpp | cctools-port and PureDarwin | +7/-1 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/dtrace_dof.cpp | src/ld/passes/dtrace_dof.cpp | cctools-port and PureDarwin | +10/-2 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/got.cpp | src/ld/passes/got.cpp | cctools-port and PureDarwin | +58/-1 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/huge.cpp | src/ld/passes/huge.cpp | cctools-port and PureDarwin | +6/-0 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/inits.cpp | src/ld/passes/inits.cpp | cctools-port and PureDarwin | +2/-1 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/objc.cpp | src/ld/passes/objc.cpp | cctools-port and PureDarwin | +12/-1 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/stubs/stubs.cpp | src/ld/passes/stubs/stubs.cpp | cctools-port and PureDarwin | +37/-1 | notice |
| darwin/tools/cctools/ld64/src/ld/passes/tlvp.cpp | src/ld/passes/tlvp.cpp | cctools-port and PureDarwin | +24/-1 | notice |
| darwin/tools/cctools/ld64/src/other/ObjectDump.cpp | src/other/ObjectDump.cpp | cctools-port and PureDarwin | +11/-24 | notice |
| darwin/tools/cctools/ld64/src/other/unwinddump.cpp | src/other/unwinddump.cpp | cctools-port and PureDarwin | +4/-135 | notice |

## Libc-1439.141.1 (https://github.com/apple-oss-distributions/Libc)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/AvailabilityVersions/include/limits.h | include/limits.h (copy, 99% alike) | PureDarwin (last 2021-11-03) | +0/-2 | notice |
| darwin/src/Libraries/libSystem/libc/fbsdcompat/_fbsd_compat_.h | fbsdcompat/_fbsd_compat_.h | PureDarwin (last 2026-07-10) | +19/-0 | notice |
| darwin/src/Libraries/libSystem/libc/fbsdcompat/_fpmath.h | fbsdcompat/_fpmath.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libc/gdtoa/FreeBSD/_ldtoa.c | gdtoa/FreeBSD/_ldtoa.c | PureDarwin (last 2026-09-28) | +1/-1 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/gdtoa/FreeBSD/gdtoa-hexnan.c | gdtoa/FreeBSD/gdtoa-hexnan.c | PureDarwin (last 2026-09-28) | +1/-1 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/gdtoa/arith.h | gdtoa/arith.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libc/gdtoa/gd_qnan.h | gdtoa/gd_qnan.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libc/gen/FreeBSD/arc4random.c | gen/FreeBSD/arc4random.c | PureDarwin (last 2026-07-21) | +11/-0 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/gen/FreeBSD/daemon.c | gen/FreeBSD/daemon.c | PureDarwin (last 2026-07-21) | +6/-6 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/gen/FreeBSD/mkpath_np.c | util/mkpath_np.c (copy, 78% alike) | PureDarwin (last 2026-07-18) | +158/-191 | notice |
| darwin/src/Libraries/libSystem/libc/gen/FreeBSD/popen.c | gen/FreeBSD/popen.c | PureDarwin (last 2026-07-18) | +1/-0 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/gen/FreeBSD/termios.c | gen/FreeBSD/termios.c | PureDarwin (last 2026-07-16) | +10/-2 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/gen/FreeBSD/wait3.c | gen/FreeBSD/wait3.c | PureDarwin (last 2026-07-16) | +13/-1 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/gen/FreeBSD/wordexp.c | gen/FreeBSD/wordexp.c | PureDarwin (last 2026-07-17) | +10/-10 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/gen/clock_gettime.c | gen/clock_gettime.c | PureDarwin (last 2026-07-14) | +1/-0 | notice |
| darwin/src/Libraries/libSystem/libc/gen/crypt.c | gen/crypt.c | PureDarwin (last 2026-10-11) | +6/-0 | notice |
| darwin/src/Libraries/libSystem/libc/gen/thread_stack_pcs.c | gen/thread_stack_pcs.c | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/_ctype.h | include/_ctype.h | PureDarwin (last 2026-09-14) | +19/-10 | notice |
| darwin/src/Libraries/libSystem/libc/include/sys/cdefs.h | include/sys/cdefs.h | PureDarwin (last 2026-09-28) | +5/-6 | notice |
| darwin/src/Libraries/libSystem/libc/include/unistd.h | include/unistd.h | PureDarwin (last 2026-09-19) | +107/-91 | notice |
| darwin/src/Libraries/libSystem/libc/include/xlocale/_ctype.h | include/xlocale/_ctype.h | PureDarwin (last 2026-09-14) | +5/-2 | notice |
| darwin/src/Libraries/libSystem/libc/libdarwin/exception.c | libdarwin/exception.c | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/libm/openlibm/src/fpmath.h | fbsdcompat/fpmath.h (copy, 65% alike) | PureDarwin (last 2026-09-28) | +58/-6 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/locale/FreeBSD/mblocal.h | locale/FreeBSD/mblocal.h | PureDarwin (last 2026-09-14) | +2/-0 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/locale/xlocale_private.h | locale/xlocale_private.h | PureDarwin (last 2026-09-14) | +4/-1 | notice |
| darwin/src/Libraries/libSystem/libc/os/assumes.h | os/assumes.h | PureDarwin (last 2026-07-08) | +1/-0 | notice |
| darwin/src/Libraries/libSystem/libc/os/variant_private.h | os/variant_private.h | PureDarwin (last 2026-07-22) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/scripts/generate_features.pl | xcodescripts/generate_features.pl | PureDarwin (last 2026-09-28) | +4/-1 | notice |
| darwin/src/Libraries/libSystem/libc/scripts/headers.sh | xcodescripts/headers.sh | PureDarwin (last 2026-09-28) | +300/-12 | notice |
| darwin/src/Libraries/libSystem/libc/stdio/FreeBSD/vfprintf.c | stdio/FreeBSD/vfprintf.c | PureDarwin (last 2026-08-13) | +6/-3 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/stdlib/FreeBSD/abort.c | stdlib/FreeBSD/abort.c | PureDarwin (last 2026-07-22) | +1/-0 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/stdlib/FreeBSD/exit.c | stdlib/FreeBSD/exit.c | PureDarwin (last 2026-07-10) | +1/-1 | permissive needs no change notice |
| darwin/src/Libraries/libSystem/libc/sys/crt_externs.c | sys/crt_externs.c | PureDarwin (last 2026-08-22) | +18/-1 | notice |
| darwin/src/Libraries/objc4/macosx.internal/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/os/variant_private.h | os/variant_private.h (copy, 99% alike) | PureDarwin (last 2026-07-24) | +4/-4 | notice |
| darwin/tools/cctools/ld64/src/3rd/mkpath_np.c | util/mkpath_np.c (copy, 78% alike) | cctools-port and PureDarwin | +159/-192 | notice |

## libdispatch-1271.40.12 (https://github.com/apple-oss-distributions/libdispatch)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/libfirehose_kernel/src/firehose_buffer.c | src/firehose/firehose_buffer.c | PureDarwin (last 2026-07-08) | +14/-4 | notice |
| darwin/src/Kernel/libfirehose_kernel/src/firehose_inline_internal.h | src/firehose/firehose_inline_internal.h | PureDarwin (last 2026-07-09) | +3/-1 | notice |
| darwin/src/Libraries/libSystem/libdispatch/CMakeLists.txt | CMakeLists.txt | replaced by PureDarwin's own file | +141/-337 | none needed |
| darwin/src/Libraries/libSystem/libdispatch/config/config.h | config/config.h | PureDarwin (last 2026-07-24) | +11/-2 | notice |
| darwin/src/Libraries/libSystem/libdispatch/src/firehose/src/firehose_buffer.c | src/firehose/firehose_buffer.c | PureDarwin (last 2026-09-28) | +6/-3 | notice |
| darwin/src/Libraries/libSystem/libdispatch/src/internal.h | src/internal.h | PureDarwin (last 2026-09-07) | +14/-0 | notice |
| darwin/src/Libraries/libSystem/libdispatch/src/object_internal.h | src/object_internal.h | PureDarwin (last 2026-07-24) | +17/-0 | notice |
| darwin/src/Libraries/libSystem/libdispatch/src/queue_internal.h | src/queue_internal.h | PureDarwin (last 2026-07-17) | +11/-7 | notice |
| darwin/src/Libraries/libSystem/libdispatch/src/shims/atomic.h | src/shims/atomic.h | PureDarwin (last 2026-09-28) | +32/-1 | notice |
| darwin/src/Libraries/libSystem/libdispatch/src/shims/time.h | src/shims/time.h | PureDarwin (last 2026-09-07) | +7/-0 | notice |
| darwin/src/Libraries/libSystem/libdispatch/src/shims/yield.h | src/shims/yield.h | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Libraries/libSystem/libdispatch/src/voucher_internal.h | src/voucher_internal.h | PureDarwin (last 2026-07-24) | +26/-0 | notice |

## Libinfo-542.40.3 (https://github.com/apple-oss-distributions/Libinfo)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/XPC/libinfo/aliasdb.h | lookup.subproj/aliasdb.h | PureDarwin (last 2026-08-25) | +27/-36 | notice |
| darwin/src/Libraries/XPC/libinfo/file_module.c | lookup.subproj/file_module.c | PureDarwin (last 2026-11-10) | +367/-4 | notice |
| darwin/src/Libraries/XPC/libinfo/libinfo.c | lookup.subproj/libinfo.c | PureDarwin (last 2026-08-08) | +3/-1 | notice |
| darwin/src/Libraries/XPC/libinfo/printerdb.h | lookup.subproj/printerdb.h | PureDarwin (last 2026-08-25) | +14/-37 | notice |
| darwin/src/Libraries/XPC/libinfo/search_module.c | lookup.subproj/search_module.c | kido (2026-11-20) | +1/-1 | notice |

## libmalloc-317.140.5 (https://github.com/apple-oss-distributions/libmalloc)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/libSystem/libmalloc/include/malloc/_malloc.h | include/malloc/_malloc.h | PureDarwin (last 2026-09-14) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libmalloc/private/stack_logging.h | private/stack_logging.h | PureDarwin (last 2023-09-30) | +8/-8 | notice |
| darwin/src/Libraries/libSystem/libmalloc/src/base.h | src/base.h | PureDarwin (last 2026-09-28) | +2/-2 | notice |
| darwin/src/Libraries/libSystem/libmalloc/src/malloc.c | src/malloc.c | PureDarwin (last 2026-09-11) | +8/-1 | notice |
| darwin/src/Libraries/libSystem/libmalloc/src/platform.h | src/platform.h | PureDarwin (last 2026-09-28) | +2/-2 | notice |
| darwin/src/Libraries/libSystem/libmalloc/src/purgeable_malloc.c | src/purgeable_malloc.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libmalloc/src/vm.c | src/vm.c | PureDarwin (last 2026-09-28) | +12/-1 | notice |

## Libnotify-279.40.4 (https://github.com/apple-oss-distributions/Libnotify)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/XPC/notify/com.apple.notifyd.plist | notifyd/com.apple.notifyd.plist | PureDarwin (last 2026-07-24) | +8/-17 | no comment syntax for .plist |
| darwin/src/Libraries/XPC/notify/libnotify.h | libnotify.h | PureDarwin (last 2026-07-22) | +2/-0 | notice |
| darwin/src/Libraries/XPC/notify/notify_client.c | notify_client.c | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Libraries/XPC/notify/notify_proc.c | notifyd/notify_proc.c | PureDarwin (last 2026-08-30) | +1/-1 | notice |

## libplatform-254.40.4 (https://github.com/apple-oss-distributions/libplatform)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/libSystem/libplatform/include/setjmp.h | include/setjmp.h | PureDarwin (last 2026-09-28) | +8/-0 | notice |
| darwin/src/Libraries/libSystem/libplatform/internal/os/yield.h | internal/os/yield.h | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Libraries/libSystem/libplatform/private/os/crashlog_private.h | private/os/crashlog_private.h | PureDarwin (last 2026-08-17) | +4/-1 | notice |
| darwin/src/Libraries/libSystem/libplatform/private/platform/string.h | private/platform/string.h | PureDarwin (last 2026-10-11) | +11/-1 | notice |
| darwin/src/Libraries/libSystem/libplatform/src/atomics/x86_64/pfz.s | src/atomics/x86_64/pfz.s | PureDarwin (last 2026-07-14) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libplatform/src/cachecontrol/riscv/cache.s | src/cachecontrol/arm/cache.s (copy, 64% alike) | PureDarwin (last 2026-09-28) | +16/-16 | notice |
| darwin/src/Libraries/libSystem/libplatform/src/os/atomic.c | src/os/atomic.c | PureDarwin (last 2026-09-28) | +60/-2 | notice |
| darwin/src/Libraries/libSystem/libplatform/src/os/lock.c | src/os/lock.c | PureDarwin (last 2026-08-17) | +4/-1 | notice |
| darwin/src/Libraries/libSystem/libplatform/src/ucontext/generic/getmcontext.c | src/ucontext/generic/getmcontext.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libplatform/src/ucontext/generic/makecontext.c | src/ucontext/generic/makecontext.c | PureDarwin (last 2026-09-28) | +83/-0 | notice |
| darwin/src/Libraries/libSystem/libplatform/src/ucontext/generic/setcontext.c | src/ucontext/generic/setcontext.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Libraries/objc4/macosx.internal/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/os/lock_private.h | private/os/lock_private.h (copy, 92% alike) | PureDarwin (last 2026-07-24) | +70/-73 | notice |
| darwin/tools/cctools/include/foreign/i386/setjmp.h | include/setjmp.h (copy, 60% alike) | cctools-port and PureDarwin | +25/-47 | notice |
| darwin/tools/cctools/include/foreign/libkern/OSCacheControl.h | include/libkern/OSCacheControl.h (copy, 95% alike) | cctools-port and PureDarwin | +3/-4 | notice |
| darwin/tools/cctools/ld64/src/3rd/strlcat.c | src/string/generic/strlcat.c (copy, 69% alike) | cctools-port and PureDarwin | +16/-15 | notice |
| darwin/tools/cctools/ld64/src/3rd/strlcpy.c | src/string/generic/strlcpy.c (copy, 76% alike) | cctools-port and PureDarwin | +7/-14 | notice |

## libpthread-454.80.2 (https://github.com/apple-oss-distributions/libpthread)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/Extensions/pthread/kern_init.c | kern/kern_init.c | PureDarwin (last 2023-10-02) | +1/-1 | notice |
| darwin/src/Kernel/Extensions/pthread/kern_internal.h | kern/kern_internal.h | PureDarwin (last 2026-08-30) | +34/-7 | notice |
| darwin/src/Kernel/Extensions/pthread/kern_support.c | kern/kern_support.c | PureDarwin (last 2026-09-28) | +94/-11 | notice |
| darwin/src/Kernel/Extensions/pthread/kern_synch.c | kern/kern_synch.c | PureDarwin (last 2026-08-30) | +143/-66 | notice |
| darwin/src/Kernel/Extensions/pthread/pthread-Info.plist | kern/pthread-Info.plist | PureDarwin (last 2026-08-30) | +5/-5 | no comment syntax for .plist |
| darwin/src/Libraries/libSystem/pthread/private/pthread/private.h | private/pthread/private.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/pthread/src/kern/kern_internal.h | kern/kern_internal.h | PureDarwin (last 2026-07-08) | +2/-2 | notice |
| darwin/src/Libraries/libSystem/pthread/src/pthread.c | src/pthread.c | PureDarwin (last 2026-09-28) | +74/-18 | notice |
| darwin/src/Libraries/libSystem/pthread/src/pthread_asm.s | src/pthread_asm.s | PureDarwin (last 2026-09-28) | +32/-0 | notice |
| darwin/src/Libraries/libSystem/pthread/src/pthread_cancelable.c | src/pthread_cancelable.c | PureDarwin (last 2026-08-30) | +5/-0 | notice |
| darwin/src/Libraries/libSystem/pthread/src/pthread_mutex.c | src/pthread_mutex.c | PureDarwin (last 2026-07-22) | +3/-0 | notice |
| darwin/src/Libraries/libSystem/pthread/src/pthread_rwlock.c | src/pthread_rwlock.c | PureDarwin (last 2026-09-28) | +2/-1 | notice |
| darwin/src/Libraries/libSystem/pthread/src/pthread_tsd.c | src/pthread_tsd.c | PureDarwin (last 2026-07-14) | +15/-1 | notice |
| darwin/src/Libraries/libSystem/pthread/src/qos.c | src/qos.c | PureDarwin (last 2026-08-22) | +12/-2 | notice |
| darwin/src/Libraries/objc4/macosx.internal/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/pthread/tsd_private.h | private/pthread/tsd_private.h (copy, 90% alike) | PureDarwin (last 2026-07-24) | +29/-33 | notice |
| darwin/src/Libraries/objc4/macosx.internal/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/pthread/workqueue_private.h | private/pthread/workqueue_private.h (copy, 99% alike) | PureDarwin (last 2026-07-24) | +3/-0 | notice |

## libresolv-96 (https://github.com/apple-oss-distributions/libresolv)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/libresolv/dst_api.c | dst_api.c | PureDarwin (last 2026-11-28) | +29/-5 | permissive needs no change notice |
| darwin/src/Libraries/libresolv/mtctxres.c | mtctxres.c | PureDarwin (last 2026-11-28) | +3/-6 | permissive needs no change notice |
| darwin/src/Libraries/libresolv/res_init.c | res_init.c | PureDarwin (last 2026-11-28) | +1/-0 | permissive needs no change notice |
| darwin/src/Libraries/libresolv/res_state.c | res_state.c | PureDarwin (last 2026-11-28) | +11/-8 | permissive needs no change notice |
| darwin/src/Userspace/shell_cmds/uudecode/base64.c | base64.c (copy, 97% alike) | PureDarwin (last 2026-07-18) | +6/-15 | permissive needs no change notice |
| darwin/src/Userspace/shell_cmds/uuencode/base64.c | base64.c (copy, 97% alike) | PureDarwin (last 2026-07-18) | +6/-15 | permissive needs no change notice |

## llvmorg-21.1.8 (https://github.com/llvm/llvm-project)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/clang_bootstrap/CMakeLists.txt | clang/lib/Headers/CMakeLists.txt | replaced by PureDarwin's own file | +2/-886 | none needed |
| darwin/src/Libraries/clang_bootstrap/__stddef_max_align_t.h | clang/lib/Headers/__stddef_max_align_t.h | PureDarwin (last 2023-10-03) | +1/-1 | notice |
| darwin/src/Libraries/clang_bootstrap/stdalign.h | clang/lib/Headers/stdalign.h | PureDarwin (last 2026-07-10) | +0/-3 | notice |
| darwin/src/Libraries/clang_bootstrap/stdarg.h | clang/lib/Headers/stdarg.h | PureDarwin (last 2023-10-03) | +19/-59 | notice |
| darwin/src/Libraries/clang_bootstrap/stdatomic.h | clang/lib/Headers/stdatomic.h | PureDarwin (last 2026-07-08) | +8/-30 | notice |
| darwin/src/Libraries/clang_bootstrap/stdbool.h | clang/lib/Headers/stdbool.h | PureDarwin (last 2023-10-03) | +7/-15 | notice |
| darwin/src/Libraries/clang_bootstrap/stddef.h | clang/lib/Headers/stddef.h | PureDarwin (last 2023-10-03) | +80/-98 | notice |
| darwin/src/Libraries/libcxxabi/CMakeLists.txt | libcxxabi/CMakeLists.txt | replaced by PureDarwin's own file | +321/-503 | none needed |
| darwin/src/Libraries/libunwind/src/AddressSpace.hpp | libunwind/src/AddressSpace.hpp | PureDarwin (last 2026-09-27) | +4/-0 | notice |
| darwin/src/Libraries/libunwind/src/Unwind-sjlj.c | libunwind/src/Unwind-sjlj.c | PureDarwin (last 2026-08-17) | +3/-3 | notice |
| darwin/src/Libraries/libunwind/src/config.h | libunwind/src/config.h | PureDarwin (last 2026-09-28) | +4/-1 | notice |
| darwin/src/Libraries/libunwind/src/libunwind.cpp | libunwind/src/libunwind.cpp | PureDarwin (last 2026-08-17) | +3/-1 | notice |
| darwin/tools/cctools/include/foreign/mach-o/compact_unwind_encoding.h | libunwind/include/mach-o/compact_unwind_encoding.h (copy, 70% alike) | cctools-port and PureDarwin | +114/-163 | notice |
| darwin/tools/cctools/ld64/src/3rd/BlocksRuntime/Block.h | compiler-rt/lib/BlocksRuntime/Block.h (copy, 98% alike) | cctools-port and PureDarwin | +1/-1 | permissive needs no change notice |
| darwin/tools/cctools/ld64/src/3rd/BlocksRuntime/Block_private.h | compiler-rt/lib/BlocksRuntime/Block_private.h (copy, 99% alike) | cctools-port and PureDarwin | +1/-1 | permissive needs no change notice |

## mDNSResponder-1310.120.71 (https://github.com/apple-oss-distributions/mDNSResponder)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/libresolv/include/dns_sd.h | mDNSShared/dns_sd.h (copy, 99% alike) | PureDarwin (last 2026-07-16) | +53/-1 | permissive needs no change notice |
| darwin/src/Libraries/mDNSResponder/mDNSCore/mDNS.c | mDNSCore/mDNS.c | PureDarwin (last 2026-07-30) | +5/-0 | notice |
| darwin/src/Libraries/mDNSResponder/mDNSPosix/PosixDaemon.c | mDNSPosix/PosixDaemon.c | PureDarwin (last 2026-07-30); kido (2026-11-22) | +12/-3 | notice |
| darwin/src/Libraries/mDNSResponder/mDNSPosix/mDNSPosix.c | mDNSPosix/mDNSPosix.c | PureDarwin (last 2026-07-30); kido (2026-11-22) | +116/-7 | notice |
| darwin/src/Libraries/mDNSResponder/mDNSPosix/mDNSPosix.h | mDNSPosix/mDNSPosix.h | PureDarwin (last 2026-07-30) | +13/-0 | notice |
| darwin/src/Libraries/mDNSResponder/mDNSShared/uds_daemon.c | mDNSShared/uds_daemon.c | PureDarwin (last 2026-07-30) | +17/-0 | notice |

## objc4-818.2 (https://github.com/apple-oss-distributions/objc4)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/objc4/runtime/Messengers.subproj/objc-msg-arm.s | runtime/Messengers.subproj/objc-msg-arm.s | PureDarwin (last 2026-08-22) | +60/-8 | notice |
| darwin/src/Libraries/objc4/runtime/NSObject.mm | runtime/NSObject.mm | PureDarwin (last 2026-09-10) | +31/-1 | notice |
| darwin/src/Libraries/objc4/runtime/isa.h | runtime/isa.h | PureDarwin (last 2026-09-28) | +20/-0 | notice |
| darwin/src/Libraries/objc4/runtime/objc-cache.mm | runtime/objc-cache.mm | PureDarwin (last 2026-09-28) | +35/-13 | notice |
| darwin/src/Libraries/objc4/runtime/objc-config.h | runtime/objc-config.h | PureDarwin (last 2026-09-28) | +9/-1 | notice |
| darwin/src/Libraries/objc4/runtime/objc-internal.h | runtime/objc-internal.h | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Libraries/objc4/runtime/objc-object.h | runtime/objc-object.h | PureDarwin (last 2026-09-28) | +72/-0 | notice |
| darwin/src/Libraries/objc4/runtime/objc-os.h | runtime/objc-os.h | PureDarwin (last 2026-08-30) | +50/-7 | notice |
| darwin/src/Libraries/objc4/runtime/objc-os.mm | runtime/objc-os.mm | PureDarwin (last 2026-07-24) | +9/-9 | notice |
| darwin/src/Libraries/objc4/runtime/objc-ptrauth.h | runtime/objc-ptrauth.h | PureDarwin (last 2026-07-24) | +18/-0 | notice |
| darwin/src/Libraries/objc4/runtime/objc-runtime-new.mm | runtime/objc-runtime-new.mm | PureDarwin (last 2026-08-22) | +33/-15 | notice |
| darwin/src/Libraries/objc4/runtime/objc-runtime.mm | runtime/objc-runtime.mm | PureDarwin (last 2026-07-24) | +6/-6 | notice |
| darwin/src/Libraries/objc4/runtime/objc-zalloc.h | runtime/objc-zalloc.h | PureDarwin (last 2026-09-28) | +12/-1 | notice |
| darwin/src/Libraries/objc4/runtime/objc-zalloc.mm | runtime/objc-zalloc.mm | PureDarwin (last 2026-09-28) | +36/-0 | notice |

## Security-59754.120.12 (https://github.com/apple-oss-distributions/Security)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/Security/apple/OSX/utilities/iOSforOSX.c | OSX/utilities/iOSforOSX.c | PureDarwin (last 2026-08-13) | +3/-1 | notice |
| darwin/src/Libraries/Security/apple/trust/trustd/trustd.c | trust/trustd/trustd.c | PureDarwin (last 2026-11-10) | +3/-0 | notice |

## shell_cmds-216.60.1 (https://github.com/apple-oss-distributions/shell_cmds)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Userspace/shell_cmds/nohup.c | nohup/nohup.c (copy, 97% alike) | PureDarwin (last 2026-08-14) | +4/-5 | permissive needs no change notice |
| darwin/src/Userspace/shell_cmds/uname.c | uname/uname.c (copy, 100% alike) | PureDarwin (last 2026-09-28) | +2/-0 | permissive needs no change notice |

## swift-5.0.3-RELEASE (https://github.com/apple/swift-corelibs-foundation)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Frameworks/CoreFoundation/AppServices.subproj/CFUserNotification.c | CoreFoundation/AppServices.subproj/CFUserNotification.c | PureDarwin (last 2026-11-10) | +7/-4 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFBase.c | CoreFoundation/Base.subproj/CFBase.c | PureDarwin (last 2026-10-28) | +36/-17 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFBase.h | CoreFoundation/Base.subproj/CFBase.h | PureDarwin (last 2026-09-07) | +2/-2 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFFileUtilities.c | CoreFoundation/Base.subproj/CFFileUtilities.c | PureDarwin (last 2026-11-10) | +63/-59 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFInternal.h | CoreFoundation/Base.subproj/CFInternal.h | PureDarwin (last 2026-11-10) | +131/-100 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFKnownLocations.c | CoreFoundation/Base.subproj/CFKnownLocations.c | PureDarwin (last 2026-11-10) | +30/-4 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFPlatform.c | CoreFoundation/Base.subproj/CFPlatform.c | PureDarwin (last 2026-09-28) | +525/-57 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFPriv.h | CoreFoundation/Base.subproj/CFPriv.h | PureDarwin (last 2026-11-10) | +8/-5 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFRuntime.c | CoreFoundation/Base.subproj/CFRuntime.c | PureDarwin (last 2026-11-10) | +412/-83 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFRuntime.h | CoreFoundation/Base.subproj/CFRuntime.h | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFRuntime_Internal.h | CoreFoundation/Base.subproj/CFRuntime_Internal.h | PureDarwin (last 2026-09-07) | +9/-5 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFSortFunctions.c | CoreFoundation/Base.subproj/CFSortFunctions.c | PureDarwin (last 2026-09-07) | +7/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFSystemDirectories.c | CoreFoundation/Base.subproj/CFSystemDirectories.c | PureDarwin (last 2026-09-07) | +2/-2 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFUUID.c | CoreFoundation/Base.subproj/CFUUID.c | PureDarwin (last 2026-09-07) | +3/-3 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFUtilities.c | CoreFoundation/Base.subproj/CFUtilities.c | PureDarwin (last 2026-09-07) | +61/-51 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFWindowsUtilities.c | CoreFoundation/Base.subproj/CFWindowsUtilities.c | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CoreFoundation.h | CoreFoundation/Base.subproj/CoreFoundation.h | PureDarwin (last 2026-10-19) | +2/-0 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CoreFoundation_Prefix.h | CoreFoundation/Base.subproj/CoreFoundation_Prefix.h | PureDarwin (last 2026-09-07) | +80/-53 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/ForFoundationOnly.h | CoreFoundation/Base.subproj/ForFoundationOnly.h | PureDarwin (last 2026-11-10) | +88/-21 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/ForSwiftFoundationOnly.h | CoreFoundation/Base.subproj/ForSwiftFoundationOnly.h | PureDarwin (last 2026-09-07) | +231/-80 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/SwiftRuntime/TargetConditionals.h | CoreFoundation/Base.subproj/SwiftRuntime/TargetConditionals.h | PureDarwin (last 2026-09-28) | +15/-0 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/module.map | CoreFoundation/Base.subproj/module.map | PureDarwin (last 2026-09-07) | +0/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/Base.subproj/module.modulemap | CoreFoundation/Base.subproj/module.modulemap | PureDarwin (last 2026-09-07) | +0/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/CMakeLists.txt | CoreFoundation/CMakeLists.txt | PureDarwin (last 2026-11-10) | +188/-178 | notice |
| darwin/src/Frameworks/CoreFoundation/Collections.subproj/CFArray.c | CoreFoundation/Collections.subproj/CFArray.c | PureDarwin (last 2026-10-09) | +23/-13 | notice |
| darwin/src/Frameworks/CoreFoundation/Collections.subproj/CFBag.c | CoreFoundation/Collections.subproj/CFBag.c | PureDarwin (last 2026-10-09) | +2/-0 | notice |
| darwin/src/Frameworks/CoreFoundation/Collections.subproj/CFBasicHash.c | CoreFoundation/Collections.subproj/CFBasicHash.c | PureDarwin (last 2026-09-07) | +5/-5 | notice |
| darwin/src/Frameworks/CoreFoundation/Collections.subproj/CFBitVector.c | CoreFoundation/Collections.subproj/CFBitVector.c | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/Collections.subproj/CFData.c | CoreFoundation/Collections.subproj/CFData.c | PureDarwin (last 2026-10-19) | +81/-5 | notice |
| darwin/src/Frameworks/CoreFoundation/Collections.subproj/CFDictionary.c | CoreFoundation/Collections.subproj/CFDictionary.c | PureDarwin (last 2026-10-09) | +2/-0 | notice |
| darwin/src/Frameworks/CoreFoundation/Collections.subproj/CFSet.c | CoreFoundation/Collections.subproj/CFSet.c | PureDarwin (last 2026-10-09) | +2/-0 | notice |
| darwin/src/Frameworks/CoreFoundation/Collections.subproj/CFStorage.c | CoreFoundation/Collections.subproj/CFStorage.c | PureDarwin (last 2026-09-07) | +3/-3 | notice |
| darwin/src/Frameworks/CoreFoundation/Collections.subproj/CFTree.c | CoreFoundation/Collections.subproj/CFTree.c | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/Error.subproj/CFError.c | CoreFoundation/Error.subproj/CFError.c | PureDarwin (last 2026-10-09) | +9/-7 | notice |
| darwin/src/Frameworks/CoreFoundation/LICENSE | LICENSE (copy, 100% alike) | PureDarwin (last 2026-09-07) | +0/-0 | no comment syntax for LICENSE |
| darwin/src/Frameworks/CoreFoundation/Locale.subproj/CFCalendar.c | CoreFoundation/Locale.subproj/CFCalendar.c | PureDarwin (last 2026-09-07) | +43/-40 | notice |
| darwin/src/Frameworks/CoreFoundation/Locale.subproj/CFCalendar_Enumerate.c | CoreFoundation/Locale.subproj/CFCalendar_Enumerate.c | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/Locale.subproj/CFDateFormatter.c | CoreFoundation/Locale.subproj/CFDateFormatter.c | PureDarwin (last 2026-09-07) | +4/-4 | notice |
| darwin/src/Frameworks/CoreFoundation/Locale.subproj/CFDateIntervalFormatter.c | CoreFoundation/Locale.subproj/CFDateIntervalFormatter.c | PureDarwin (last 2026-09-07) | +124/-56 | notice |
| darwin/src/Frameworks/CoreFoundation/Locale.subproj/CFLocale.c | CoreFoundation/Locale.subproj/CFLocale.c | PureDarwin (last 2026-11-10) | +105/-74 | notice |
| darwin/src/Frameworks/CoreFoundation/Locale.subproj/CFLocaleIdentifier.c | CoreFoundation/Locale.subproj/CFLocaleIdentifier.c | PureDarwin (last 2026-09-07) | +9/-9 | notice |
| darwin/src/Frameworks/CoreFoundation/Locale.subproj/CFLocaleKeys.c | CoreFoundation/Locale.subproj/CFLocaleKeys.c | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/NumberDate.subproj/CFDate.c | CoreFoundation/NumberDate.subproj/CFDate.c | PureDarwin (last 2026-09-07) | +18/-14 | notice |
| darwin/src/Frameworks/CoreFoundation/NumberDate.subproj/CFNumber.c | CoreFoundation/NumberDate.subproj/CFNumber.c | PureDarwin (last 2026-10-19) | +12/-9 | notice |
| darwin/src/Frameworks/CoreFoundation/NumberDate.subproj/CFTimeZone.c | CoreFoundation/NumberDate.subproj/CFTimeZone.c | PureDarwin (last 2026-09-07) | +490/-341 | notice |
| darwin/src/Frameworks/CoreFoundation/Parsing.subproj/CFBinaryPList.c | CoreFoundation/Parsing.subproj/CFBinaryPList.c | PureDarwin (last 2026-09-07) | +2/-2 | notice |
| darwin/src/Frameworks/CoreFoundation/Parsing.subproj/CFPropertyList.c | CoreFoundation/Parsing.subproj/CFPropertyList.c | PureDarwin (last 2026-09-07) | +180/-176 | notice |
| darwin/src/Frameworks/CoreFoundation/Parsing.subproj/CFXMLParser.h | CoreFoundation/Parsing.subproj/CFXMLParser.h | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle.c | CoreFoundation/PlugIn.subproj/CFBundle.c | PureDarwin (last 2026-09-07) | +27/-14 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle_Binary.c | CoreFoundation/PlugIn.subproj/CFBundle_Binary.c | PureDarwin (last 2026-09-28) | +14/-6 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle_BinaryTypes.h | CoreFoundation/PlugIn.subproj/CFBundle_BinaryTypes.h | PureDarwin (last 2026-09-07) | +4/-4 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle_Executable.c | CoreFoundation/PlugIn.subproj/CFBundle_Executable.c | PureDarwin (last 2026-11-10) | +38/-23 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle_Grok.c | CoreFoundation/PlugIn.subproj/CFBundle_Grok.c | PureDarwin (last 2026-09-07) | +21/-21 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle_InfoPlist.c | CoreFoundation/PlugIn.subproj/CFBundle_InfoPlist.c | PureDarwin (last 2026-09-07) | +95/-90 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle_Internal.h | CoreFoundation/PlugIn.subproj/CFBundle_Internal.h | PureDarwin (last 2026-09-07) | +4/-4 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle_Locale.c | CoreFoundation/PlugIn.subproj/CFBundle_Locale.c | PureDarwin (last 2026-10-13) | +47/-2 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle_Resources.c | CoreFoundation/PlugIn.subproj/CFBundle_Resources.c | PureDarwin (last 2026-11-10) | +135/-127 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFBundle_Strings.c | CoreFoundation/PlugIn.subproj/CFBundle_Strings.c | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/PlugIn.subproj/CFPlugInCOM.h | CoreFoundation/PlugIn.subproj/CFPlugInCOM.h | PureDarwin (last 2026-09-07) | +6/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/Preferences.subproj/CFApplicationPreferences.c | CoreFoundation/Preferences.subproj/CFApplicationPreferences.c | PureDarwin (last 2026-10-19) | +6/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/Preferences.subproj/CFPreferences.c | CoreFoundation/Preferences.subproj/CFPreferences.c | PureDarwin (last 2026-09-07) | +0/-26 | notice |
| darwin/src/Frameworks/CoreFoundation/Preferences.subproj/CFPreferences.h | CoreFoundation/Preferences.subproj/CFPreferences.h | PureDarwin (last 2026-10-19) | +3/-0 | notice |
| darwin/src/Frameworks/CoreFoundation/Preferences.subproj/CFXMLPreferencesDomain.c | CoreFoundation/Preferences.subproj/CFXMLPreferencesDomain.c | PureDarwin (last 2026-09-07) | +2/-2 | notice |
| darwin/src/Frameworks/CoreFoundation/RunLoop.subproj/CFMachPort.c | CoreFoundation/RunLoop.subproj/CFMachPort.c | PureDarwin (last 2026-11-10) | +3/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/RunLoop.subproj/CFMachPort_Lifetime.c | CoreFoundation/RunLoop.subproj/CFMachPort_Lifetime.c | PureDarwin (last 2026-11-10) | +5/-3 | notice |
| darwin/src/Frameworks/CoreFoundation/RunLoop.subproj/CFMessagePort.c | CoreFoundation/RunLoop.subproj/CFMessagePort.c | PureDarwin (last 2026-11-10) | +30/-3 | notice |
| darwin/src/Frameworks/CoreFoundation/RunLoop.subproj/CFMessagePort.h | CoreFoundation/RunLoop.subproj/CFMessagePort.h | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/RunLoop.subproj/CFRunLoop.c | CoreFoundation/RunLoop.subproj/CFRunLoop.c | PureDarwin (last 2026-11-10) | +77/-72 | notice |
| darwin/src/Frameworks/CoreFoundation/RunLoop.subproj/CFSocket.c | CoreFoundation/RunLoop.subproj/CFSocket.c | PureDarwin (last 2026-09-07) | +27/-27 | notice |
| darwin/src/Frameworks/CoreFoundation/Stream.subproj/CFConcreteStreams.c | CoreFoundation/Stream.subproj/CFConcreteStreams.c | PureDarwin (last 2026-09-07) | +4/-4 | notice |
| darwin/src/Frameworks/CoreFoundation/Stream.subproj/CFSocketStream.c | CoreFoundation/Stream.subproj/CFSocketStream.c | PureDarwin (last 2026-09-07) | +8/-8 | notice |
| darwin/src/Frameworks/CoreFoundation/Stream.subproj/CFStream.c | CoreFoundation/Stream.subproj/CFStream.c | PureDarwin (last 2026-09-07) | +6/-6 | notice |
| darwin/src/Frameworks/CoreFoundation/String.subproj/CFBurstTrie.c | CoreFoundation/String.subproj/CFBurstTrie.c | PureDarwin (last 2026-09-07) | +8/-8 | notice |
| darwin/src/Frameworks/CoreFoundation/String.subproj/CFBurstTrie.h | CoreFoundation/String.subproj/CFBurstTrie.h | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/String.subproj/CFCharacterSet.c | CoreFoundation/String.subproj/CFCharacterSet.c | PureDarwin (last 2026-09-07) | +12/-6 | notice |
| darwin/src/Frameworks/CoreFoundation/String.subproj/CFCharacterSetData.S | CoreFoundation/String.subproj/CFCharacterSetData.S | PureDarwin (last 2026-09-07) | +5/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/String.subproj/CFString.c | CoreFoundation/String.subproj/CFString.c | PureDarwin (last 2026-11-10) | +107/-37 | notice |
| darwin/src/Frameworks/CoreFoundation/String.subproj/CFStringEncodings.c | CoreFoundation/String.subproj/CFStringEncodings.c | PureDarwin (last 2026-09-07) | +3/-3 | notice |
| darwin/src/Frameworks/CoreFoundation/String.subproj/CFStringUtilities.c | CoreFoundation/String.subproj/CFStringUtilities.c | PureDarwin (last 2026-09-07) | +15/-19 | notice |
| darwin/src/Frameworks/CoreFoundation/String.subproj/CFUniCharPropertyDatabase.S | CoreFoundation/String.subproj/CFUniCharPropertyDatabase.S | PureDarwin (last 2026-09-07) | +5/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/String.subproj/CFUnicodeData.S | CoreFoundation/String.subproj/CFUnicodeData.S | PureDarwin (last 2026-09-07) | +5/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/StringEncodings.subproj/CFICUConverters.c | CoreFoundation/StringEncodings.subproj/CFICUConverters.c | replaced by PureDarwin's own file | +27/-408 | none needed |
| darwin/src/Frameworks/CoreFoundation/StringEncodings.subproj/CFPlatformConverters.c | CoreFoundation/StringEncodings.subproj/CFPlatformConverters.c | PureDarwin (last 2026-09-07) | +9/-9 | notice |
| darwin/src/Frameworks/CoreFoundation/StringEncodings.subproj/CFStringEncodingConverter.c | CoreFoundation/StringEncodings.subproj/CFStringEncodingConverter.c | PureDarwin (last 2026-09-07) | +9/-9 | notice |
| darwin/src/Frameworks/CoreFoundation/StringEncodings.subproj/CFStringEncodingDatabase.c | CoreFoundation/StringEncodings.subproj/CFStringEncodingDatabase.c | PureDarwin (last 2026-09-07) | +6/-6 | notice |
| darwin/src/Frameworks/CoreFoundation/StringEncodings.subproj/CFUniChar.c | CoreFoundation/StringEncodings.subproj/CFUniChar.c | PureDarwin (last 2026-09-07) | +74/-50 | notice |
| darwin/src/Frameworks/CoreFoundation/StringEncodings.subproj/CFUnicodeDecomposition.c | CoreFoundation/StringEncodings.subproj/CFUnicodeDecomposition.c | PureDarwin (last 2026-09-07) | +1/-1 | notice |
| darwin/src/Frameworks/CoreFoundation/URL.subproj/CFURL.c | CoreFoundation/URL.subproj/CFURL.c | PureDarwin (last 2026-11-10) | +410/-40 | notice |
| darwin/src/Frameworks/CoreFoundation/URL.subproj/CFURL.h | CoreFoundation/URL.subproj/CFURL.h | PureDarwin (last 2026-10-19) | +14/-2 | notice |
| darwin/src/Frameworks/CoreFoundation/URL.subproj/CFURL.inc.h | CoreFoundation/URL.subproj/CFURL.inc.h | PureDarwin (last 2026-09-07) | +2/-2 | notice |
| darwin/src/Frameworks/CoreFoundation/URL.subproj/CFURLAccess.c | CoreFoundation/URL.subproj/CFURLAccess.c | PureDarwin (last 2026-09-07) | +54/-9 | notice |
| darwin/src/Frameworks/CoreFoundation/URL.subproj/CFURLComponents.h | CoreFoundation/URL.subproj/CFURLComponents.h | PureDarwin (last 2026-09-07) | +6/-6 | notice |
| darwin/src/Frameworks/CoreFoundation/URL.subproj/CFURLComponents_URIParser.c | CoreFoundation/URL.subproj/CFURLComponents_URIParser.c | PureDarwin (last 2026-09-07) | +1/-1 | notice |

## syslog-385 (https://github.com/apple-oss-distributions/syslog)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Libraries/syslog/libsystem_asl.tproj/src/asl.c | libsystem_asl.tproj/src/asl.c | PureDarwin (last 2026-07-30) | +17/-0 | notice |
| darwin/src/Libraries/syslog/libsystem_asl.tproj/src/asl_core.c | libsystem_asl.tproj/src/asl_core.c | PureDarwin (last 2026-08-30) | +0/-1 | notice |
| darwin/src/Libraries/syslog/libsystem_asl.tproj/src/asl_file.c | libsystem_asl.tproj/src/asl_file.c | PureDarwin (last 2026-07-30) | +5/-1 | notice |
| darwin/src/Libraries/syslog/libsystem_asl.tproj/src/asl_legacy1.c | libsystem_asl.tproj/src/asl_legacy1.c | PureDarwin (last 2026-07-30) | +2/-0 | notice |
| darwin/src/Libraries/syslog/libsystem_asl.tproj/src/asl_msg_list.c | libsystem_asl.tproj/src/asl_msg_list.c | PureDarwin (last 2026-07-30) | +2/-2 | notice |
| darwin/src/Libraries/syslog/libsystem_asl.tproj/src/asl_string.c | libsystem_asl.tproj/src/asl_string.c | PureDarwin (last 2026-08-30) | +0/-2 | notice |
| darwin/src/Libraries/syslog/libsystem_asl.tproj/src/syslog.c | libsystem_asl.tproj/src/syslog.c | PureDarwin (last 2026-07-30) | +19/-1 | notice |

## system_cmds-880.120.1 (https://github.com/apple-oss-distributions/system_cmds)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Userspace/system_cmds/passwd.tproj/passwd.c | passwd.tproj/passwd.c | PureDarwin (last 2026-11-10) | +15/-0 | notice |
| darwin/src/Userspace/system_cmds/passwd.tproj/passwd.h | passwd.tproj/passwd.h | PureDarwin (last 2026-08-04) | +5/-1 | notice |
| darwin/src/Userspace/system_cmds/sync.c | sync.tproj/sync.c (copy, 78% alike) | PureDarwin (last 2026-07-14) | +3/-17 | permissive needs no change notice |
| darwin/src/Userspace/system_cmds/sysctl.c | sysctl.tproj/sysctl.c (copy, 100% alike) | PureDarwin (last 2026-07-17) | +1/-1 | permissive needs no change notice |
| darwin/src/Userspace/system_cmds/vm_stat.c | vm_stat.tproj/vm_stat.c (copy, 100% alike) | PureDarwin (last 2026-07-17) | +1/-0 | notice |

## xar-404 (https://github.com/apple-oss-distributions/xar)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/tools/xar/lib/appledouble.h | xar/lib/appledouble.h | PureDarwin (last 2021-03-29) | +4/-0 | release is BSD |
| darwin/tools/xar/lib/archive.c | xar/lib/archive.c | PureDarwin (last 2021-03-29) | +497/-353 | permissive needs no change notice |
| darwin/tools/xar/lib/archive.h | xar/lib/archive.h | PureDarwin (last 2021-03-29) | +8/-16 | permissive needs no change notice |
| darwin/tools/xar/lib/arcmod.c | xar/lib/arcmod.c | PureDarwin (last 2021-03-29) | +4/-4 | permissive needs no change notice |
| darwin/tools/xar/lib/arcmod.h | xar/lib/arcmod.h | PureDarwin (last 2021-03-29) | +1/-1 | permissive needs no change notice |
| darwin/tools/xar/lib/b64.c | xar/lib/b64.c | PureDarwin (last 2021-03-29) | +54/-100 | permissive needs no change notice |
| darwin/tools/xar/lib/b64.h | xar/lib/b64.h | PureDarwin (last 2021-03-29) | +1/-1 | release is BSD |
| darwin/tools/xar/lib/bzxar.c | xar/lib/bzxar.c | PureDarwin (last 2021-03-29) | +27/-9 | permissive needs no change notice |
| darwin/tools/xar/lib/bzxar.h | xar/lib/bzxar.h | PureDarwin (last 2021-03-29) | +2/-0 | permissive needs no change notice |
| darwin/tools/xar/lib/darwinattr.c | xar/lib/darwinattr.c | PureDarwin (last 2021-03-29) | +88/-63 | permissive needs no change notice |
| darwin/tools/xar/lib/data.c | xar/lib/data.c | PureDarwin (last 2021-03-29) | +24/-21 | permissive needs no change notice |
| darwin/tools/xar/lib/data.h | xar/lib/data.h | PureDarwin (last 2021-03-29) | +1/-14 | permissive needs no change notice |
| darwin/tools/xar/lib/ea.c | xar/lib/ea.c | PureDarwin (last 2021-03-29) | +13/-2 | permissive needs no change notice |
| darwin/tools/xar/lib/err.c | xar/lib/err.c | PureDarwin (last 2021-03-29) | +4/-13 | permissive needs no change notice |
| darwin/tools/xar/lib/ext2.c | xar/lib/ext2.c | PureDarwin (last 2021-03-29) | +24/-14 | permissive needs no change notice |
| darwin/tools/xar/lib/fbsdattr.c | xar/lib/fbsdattr.c | PureDarwin (last 2021-03-29) | +10/-6 | permissive needs no change notice |
| darwin/tools/xar/lib/filetree.c | xar/lib/filetree.c | PureDarwin (last 2021-03-29) | +36/-30 | permissive needs no change notice |
| darwin/tools/xar/lib/hash.c | xar/lib/hash.c | PureDarwin (last 2021-03-29) | +176/-285 | permissive needs no change notice |
| darwin/tools/xar/lib/hash.h | xar/lib/hash.h | PureDarwin (last 2021-03-29) | +8/-16 | permissive needs no change notice |
| darwin/tools/xar/lib/io.c | xar/lib/io.c | PureDarwin (last 2021-03-29) | +194/-202 | permissive needs no change notice |
| darwin/tools/xar/lib/io.h | xar/lib/io.h | PureDarwin (last 2021-03-29) | +5/-14 | permissive needs no change notice |
| darwin/tools/xar/lib/linuxattr.c | xar/lib/linuxattr.c | PureDarwin (last 2021-03-29) | +16/-10 | permissive needs no change notice |
| darwin/tools/xar/lib/lzmaxar.c | xar/lib/lzmaxar.c | PureDarwin (last 2021-03-29) | +141/-27 | permissive needs no change notice |
| darwin/tools/xar/lib/lzmaxar.h | xar/lib/lzmaxar.h | PureDarwin (last 2021-03-29) | +2/-0 | permissive needs no change notice |
| darwin/tools/xar/lib/macho.c | xar/lib/macho.c | PureDarwin (last 2021-03-29) | +29/-24 | permissive needs no change notice |
| darwin/tools/xar/lib/script.c | xar/lib/script.c | PureDarwin (last 2021-03-29) | +2/-1 | permissive needs no change notice |
| darwin/tools/xar/lib/signature.c | xar/lib/signature.c | PureDarwin (last 2021-03-29) | +67/-90 | permissive needs no change notice |
| darwin/tools/xar/lib/signature.h | xar/lib/signature.h | PureDarwin (last 2021-03-29) | +4/-7 | permissive needs no change notice |
| darwin/tools/xar/lib/stat.c | xar/lib/stat.c | PureDarwin (last 2026-07-12) | +119/-22 | permissive needs no change notice |
| darwin/tools/xar/lib/util.c | xar/lib/util.c | PureDarwin (last 2026-07-12) | +86/-234 | permissive needs no change notice |
| darwin/tools/xar/lib/util.h | xar/lib/util.h | PureDarwin (last 2021-03-29) | +2/-18 | permissive needs no change notice |
| darwin/tools/xar/lib/zxar.c | xar/lib/zxar.c | PureDarwin (last 2021-03-29) | +25/-8 | permissive needs no change notice |
| darwin/tools/xar/lib/zxar.h | xar/lib/zxar.h | PureDarwin (last 2021-03-29) | +4/-1 | permissive needs no change notice |
| darwin/tools/xar/src/xar.1 | xar/src/xar.1 | PureDarwin (last 2021-03-29) | +274/-45 | release is BSD |
| darwin/tools/xar/src/xar.c | xar/src/xar.c | PureDarwin (last 2021-03-29) | +1917/-338 | permissive needs no change notice |

## xnu-12377.121.6 (https://github.com/apple-oss-distributions/xnu)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/xnu/EXTERNAL_HEADERS/CoreEntitlements/der_vm.h | EXTERNAL_HEADERS/CoreEntitlements/der_vm.h | kido (2026-11-23) | +4/-0 | notice |
| darwin/src/Kernel/xnu/EXTERNAL_HEADERS/Makefile | EXTERNAL_HEADERS/Makefile | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Kernel/xnu/EXTERNAL_HEADERS/architecture/Makefile | EXTERNAL_HEADERS/architecture/Makefile | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Kernel/xnu/EXTERNAL_HEADERS/architecture/i386/asm_help.h | EXTERNAL_HEADERS/architecture/i386/asm_help.h | PureDarwin (last 2026-08-30) | +7/-7 | notice |
| darwin/src/Kernel/xnu/EXTERNAL_HEADERS/stdatomic.h | EXTERNAL_HEADERS/stdatomic.h | PureDarwin (last 2026-08-30) | +3/-3 | permissive needs no change notice |
| darwin/src/Kernel/xnu/Makefile | Makefile | PureDarwin (last 2026-09-28); kido (2026-11-18) | +15/-6 | notice |
| darwin/src/Kernel/xnu/SETUP/config/Makefile | SETUP/config/Makefile | PureDarwin (last 2021-11-03) | +0/-2 | notice |
| darwin/src/Kernel/xnu/SETUP/decomment/Makefile | SETUP/decomment/Makefile | PureDarwin (last 2021-11-03) | +0/-2 | notice |
| darwin/src/Kernel/xnu/SETUP/decomment/decomment.c | SETUP/decomment/decomment.c | PureDarwin (last 2025-06-07); kido (2026-11-18) | +8/-1 | notice |
| darwin/src/Kernel/xnu/SETUP/installfile/Makefile | SETUP/installfile/Makefile | PureDarwin (last 2021-11-03) | +0/-2 | notice |
| darwin/src/Kernel/xnu/SETUP/installfile/installfile.c | SETUP/installfile/installfile.c | PureDarwin (last 2026-07-12) | +65/-56 | notice |
| darwin/src/Kernel/xnu/SETUP/kextsymboltool/Makefile | SETUP/kextsymboltool/Makefile | PureDarwin (last 2026-09-07) | +15/-3 | notice |
| darwin/src/Kernel/xnu/SETUP/kextsymboltool/kextsymboltool.c | SETUP/kextsymboltool/kextsymboltool.c | PureDarwin (last 2026-09-07) | +15/-3 | notice |
| darwin/src/Kernel/xnu/SETUP/replacecontents/Makefile | SETUP/replacecontents/Makefile | PureDarwin (last 2021-11-03) | +0/-2 | notice |
| darwin/src/Kernel/xnu/SETUP/replacecontents/replacecontents.c | SETUP/replacecontents/replacecontents.c | PureDarwin (last 2025-06-07) | +1/-1 | notice |
| darwin/src/Kernel/xnu/SETUP/setsegname/Makefile | SETUP/setsegname/Makefile | PureDarwin (last 2021-11-03) | +0/-2 | notice |
| darwin/src/Kernel/xnu/SETUP/setsegname/setsegname.c | SETUP/setsegname/setsegname.c | PureDarwin (last 2025-06-13) | +25/-14 | notice |
| darwin/src/Kernel/xnu/bsd/Makefile | bsd/Makefile | PureDarwin (last 2026-09-28) | +6/-0 | notice |
| darwin/src/Kernel/xnu/bsd/conf/files | bsd/conf/files | PureDarwin (last 2026-10-19) | +1/-0 | notice |
| darwin/src/Kernel/xnu/bsd/dev/arm/km.c | bsd/dev/arm/km.c | PureDarwin (last 2026-09-06) | +5/-0 | notice |
| darwin/src/Kernel/xnu/bsd/dev/arm/systemcalls.c | bsd/dev/arm/systemcalls.c | PureDarwin (last 2026-10-11) | +176/-0 | notice |
| darwin/src/Kernel/xnu/bsd/dev/arm64/sysctl.c | bsd/dev/arm64/sysctl.c | PureDarwin (last 2026-11-10) | +26/-1 | notice |
| darwin/src/Kernel/xnu/bsd/dev/dtrace/profile_prvd.c | bsd/dev/dtrace/profile_prvd.c | PureDarwin (last 2026-08-30) | +2/-2 | notice |
| darwin/src/Kernel/xnu/bsd/dev/i386/km.c | bsd/dev/i386/km.c | PureDarwin (last 2026-08-30) | +13/-0 | notice |
| darwin/src/Kernel/xnu/bsd/dev/riscv/conf.c | bsd/dev/arm64/conf.c (copy, 96% alike) | PureDarwin (last 2026-09-28) | +5/-16 | notice |
| darwin/src/Kernel/xnu/bsd/dev/riscv/cons.c | bsd/dev/arm/cons.c (copy, 94% alike) | PureDarwin (last 2026-09-28) | +6/-18 | notice |
| darwin/src/Kernel/xnu/bsd/dev/riscv/km.c | bsd/dev/arm/km.c (copy, 85% alike) | PureDarwin (last 2026-09-28) | +24/-85 | notice |
| darwin/src/Kernel/xnu/bsd/dev/riscv/stubs.c | bsd/dev/arm/stubs.c (copy, 89% alike) | PureDarwin (last 2026-09-28) | +5/-17 | notice |
| darwin/src/Kernel/xnu/bsd/dev/riscv/sysctl.c | bsd/dev/arm64/sysctl.c (copy, 67% alike) | PureDarwin (last 2026-09-28) | +72/-221 | notice |
| darwin/src/Kernel/xnu/bsd/kern/bsd_init.c | bsd/kern/bsd_init.c | PureDarwin (last 2026-10-11) | +11/-0 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_codesigning.c | bsd/kern/kern_codesigning.c | PureDarwin (last 2026-09-28) | +4/-1 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_core.c | bsd/kern/kern_core.c | PureDarwin (last 2026-11-10) | +11/-5 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_descrip.c | bsd/kern/kern_descrip.c | PureDarwin (last 2026-10-19) | +15/-0 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_exec.c | bsd/kern/kern_exec.c | PureDarwin (last 2026-09-14) | +18/-12 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_exit.c | bsd/kern/kern_exit.c | PureDarwin (last 2026-09-28) | +17/-3 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_fork.c | bsd/kern/kern_fork.c | PureDarwin (last 2026-08-30) | +15/-0 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_malloc.c | bsd/kern/kern_malloc.c | PureDarwin (last 2026-09-05) | +9/-3 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_mib.c | bsd/kern/kern_mib.c | PureDarwin (last 2026-09-28) | +20/-1 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_mman.c | bsd/kern/kern_mman.c | PureDarwin (last 2026-10-19) | +131/-2 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_proc.c | bsd/kern/kern_proc.c | PureDarwin (last 2026-10-11) | +55/-0 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_shutdown.c | bsd/kern/kern_shutdown.c | PureDarwin (last 2026-10-09) | +26/-0 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_trustcache.c | bsd/kern/kern_trustcache.c | PureDarwin (last 2026-09-05) | +28/-11 | notice |
| darwin/src/Kernel/xnu/bsd/kern/kern_xxx.c | bsd/kern/kern_xxx.c | PureDarwin (last 2026-10-09) | +7/-0 | notice |
| darwin/src/Kernel/xnu/bsd/kern/mach_loader.c | bsd/kern/mach_loader.c | PureDarwin (last 2026-09-28) | +5/-0 | notice |
| darwin/src/Kernel/xnu/bsd/kern/subr_log.c | bsd/kern/subr_log.c | PureDarwin (last 2026-08-30) | +1/-0 | notice |
| darwin/src/Kernel/xnu/bsd/kern/syscalls.master | bsd/kern/syscalls.master | PureDarwin (last 2026-08-30) | +2/-2 | notice |
| darwin/src/Kernel/xnu/bsd/kern/ubc_subr.c | bsd/kern/ubc_subr.c | PureDarwin (last 2026-09-28) | +11/-3 | notice |
| darwin/src/Kernel/xnu/bsd/machine/_endian.h | bsd/machine/_endian.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/_limits.h | bsd/machine/_limits.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/_mcontext.h | bsd/machine/_mcontext.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/_param.h | bsd/machine/_param.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/_types.h | bsd/machine/_types.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/disklabel.h | bsd/machine/disklabel.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/endian.h | bsd/machine/endian.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/fasttrap_isa.h | bsd/machine/fasttrap_isa.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/limits.h | bsd/machine/limits.h | PureDarwin (last 2026-09-28) | +2/-0 | permissive needs no change notice |
| darwin/src/Kernel/xnu/bsd/machine/param.h | bsd/machine/param.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/profile.h | bsd/machine/profile.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/psl.h | bsd/machine/psl.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/ptrace.h | bsd/machine/ptrace.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/reg.h | bsd/machine/reg.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/signal.h | bsd/machine/signal.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/smp.h | bsd/machine/smp.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/types.h | bsd/machine/types.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/machine/vmparam.h | bsd/machine/vmparam.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/miscfs/devfs/devfs.h | bsd/miscfs/devfs/devfs.h | PureDarwin (last 2026-08-30) | +10/-0 | notice |
| darwin/src/Kernel/xnu/bsd/miscfs/devfs/devfs_tree.c | bsd/miscfs/devfs/devfs_tree.c | PureDarwin (last 2026-08-30) | +6/-0 | notice |
| darwin/src/Kernel/xnu/bsd/net/bpf.c | bsd/net/bpf.c | PureDarwin (last 2026-08-30) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/net/flowhash.c | bsd/net/flowhash.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/bsd/net/if.c | bsd/net/if.c | PureDarwin (last 2026-08-30) | +1/-0 | notice |
| darwin/src/Kernel/xnu/bsd/net/if_var.h | bsd/net/if_var.h | PureDarwin (last 2026-08-30) | +2/-2 | notice |
| darwin/src/Kernel/xnu/bsd/netinet/cpu_in_cksum_gen.c | bsd/netinet/cpu_in_cksum_gen.c | PureDarwin (last 2026-09-28) | +2/-2 | notice |
| darwin/src/Kernel/xnu/bsd/netinet/ip_input.c | bsd/netinet/ip_input.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/bsd/netinet/ip_var.h | bsd/netinet/ip_var.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/bsd/netinet/tcp_subr.c | bsd/netinet/tcp_subr.c | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/bsd/netinet6/esp_core.c | bsd/netinet6/esp_core.c | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/bsd/netinet6/ip6_input.c | bsd/netinet6/ip6_input.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/bsd/netinet6/ip6_var.h | bsd/netinet6/ip6_var.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/bsd/netinet6/ipsec.h | bsd/netinet6/ipsec.h | PureDarwin (last 2026-08-30) | +2/-0 | permissive needs no change notice |
| darwin/src/Kernel/xnu/bsd/pthread/pthread_shims.c | bsd/pthread/pthread_shims.c | PureDarwin (last 2026-08-30) | +11/-0 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/Makefile | bsd/arm/Makefile | PureDarwin (last 2026-09-28) | +5/-17 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/_endian.h | bsd/arm/_endian.h | PureDarwin (last 2026-09-28) | +5/-5 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/_limits.h | bsd/arm/_limits.h | PureDarwin (last 2026-09-28) | +5/-5 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/_mcontext.h | bsd/arm/_mcontext.h | PureDarwin (last 2026-09-28) | +13/-42 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/_param.h | bsd/arm/_param.h | PureDarwin (last 2026-09-28) | +8/-11 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/_types.h | bsd/arm/_types.h | PureDarwin (last 2026-09-28) | +13/-34 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/endian.h | bsd/arm/endian.h | PureDarwin (last 2026-09-28) | +7/-11 | permissive needs no change notice |
| darwin/src/Kernel/xnu/bsd/riscv/limits.h | bsd/arm/limits.h | PureDarwin (last 2026-09-28) | +10/-20 | permissive needs no change notice |
| darwin/src/Kernel/xnu/bsd/riscv/param.h | bsd/arm/param.h | PureDarwin (last 2026-09-28) | +15/-33 | permissive needs no change notice |
| darwin/src/Kernel/xnu/bsd/riscv/profile.h | bsd/arm/profile.h | PureDarwin (last 2026-09-28) | +7/-11 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/psl.h | bsd/arm/psl.h | PureDarwin (last 2026-09-28) | +3/-3 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/ptrace.h | bsd/arm/ptrace.h | PureDarwin (last 2026-09-28) | +1/-5 | permissive needs no change notice |
| darwin/src/Kernel/xnu/bsd/riscv/reg.h | bsd/arm/reg.h | PureDarwin (last 2026-09-28) | +3/-3 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/signal.h | bsd/arm/signal.h | PureDarwin (last 2026-09-28) | +5/-5 | notice |
| darwin/src/Kernel/xnu/bsd/riscv/types.h | bsd/arm/types.h | PureDarwin (last 2026-09-28) | +11/-52 | permissive needs no change notice |
| darwin/src/Kernel/xnu/bsd/riscv/vmparam.h | bsd/arm/vmparam.h | PureDarwin (last 2026-09-28) | +6/-8 | notice |
| darwin/src/Kernel/xnu/bsd/sys/Makefile | bsd/sys/Makefile | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/bsd/sys/_endian.h | bsd/sys/_endian.h | PureDarwin (last 2026-08-30) | +16/-0 | notice |
| darwin/src/Kernel/xnu/bsd/sys/cdefs.h | bsd/sys/cdefs.h | PureDarwin (last 2026-09-28) | +1/-0 | notice |
| darwin/src/Kernel/xnu/bsd/sys/dtrace.h | bsd/sys/dtrace.h | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Kernel/xnu/bsd/sys/fcntl.h | bsd/sys/fcntl.h | PureDarwin (last 2026-10-19) | +1/-0 | notice |
| darwin/src/Kernel/xnu/bsd/sys/file_internal.h | bsd/sys/file_internal.h | PureDarwin (last 2026-10-19) | +2/-1 | notice |
| darwin/src/Kernel/xnu/bsd/sys/mbuf.h | bsd/sys/mbuf.h | PureDarwin (last 2026-09-28) | +6/-6 | notice |
| darwin/src/Kernel/xnu/bsd/sys/mount_internal.h | bsd/sys/mount_internal.h | PureDarwin (last 2026-09-30) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/sys/select.h | bsd/sys/select.h | PureDarwin (last 2026-08-30) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/sys/spawn_internal.h | bsd/sys/spawn_internal.h | PureDarwin (last 2026-08-30) | +2/-0 | notice |
| darwin/src/Kernel/xnu/bsd/sys/wait.h | bsd/sys/wait.h | PureDarwin (last 2026-08-30) | +11/-0 | notice |
| darwin/src/Kernel/xnu/bsd/vfs/vfs_subr.c | bsd/vfs/vfs_subr.c | PureDarwin (last 2026-10-09) | +156/-1 | notice |
| darwin/src/Kernel/xnu/bsd/vfs/vfs_syscalls.c | bsd/vfs/vfs_syscalls.c | PureDarwin (last 2026-10-19) | +201/-4 | notice |
| darwin/src/Kernel/xnu/bsd/vfs/vfs_xattr.c | bsd/vfs/vfs_xattr.c | PureDarwin (last 2026-11-10) | +2/-1 | notice |
| darwin/src/Kernel/xnu/bsd/vm/vm_unix.c | bsd/vm/vm_unix.c | PureDarwin (last 2026-10-28) | +9/-6 | notice |
| darwin/src/Kernel/xnu/config/BSDKernel.exports | config/BSDKernel.exports | PureDarwin (last 2026-10-19) | +9/-0 | notice |
| darwin/src/Kernel/xnu/config/IOKit.exports | config/IOKit.exports | PureDarwin (last 2026-08-30) | +0/-4 | notice |
| darwin/src/Kernel/xnu/config/Libkern.exports | config/Libkern.exports | PureDarwin (last 2026-09-05) | +1/-0 | notice |
| darwin/src/Kernel/xnu/config/Libkern.x86_64.exports | config/Libkern.x86_64.exports | PureDarwin (last 2026-11-10) | +2/-0 | notice |
| darwin/src/Kernel/xnu/config/MASTER | config/MASTER | PureDarwin (last 2026-08-30) | +12/-0 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm | config/MASTER.arm | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64 | config/MASTER.arm64 | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.BridgeOS | config/MASTER.arm64.BridgeOS | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.MacOSX | config/MASTER.arm64.MacOSX | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.WatchOS | config/MASTER.arm64.WatchOS | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.iPhoneOS | config/MASTER.arm64.iPhoneOS | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/config/MASTER.x86_64 | config/MASTER.x86_64 | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/config/Makefile | config/Makefile | PureDarwin (last 2026-08-30) | +4/-0 | notice |
| darwin/src/Kernel/xnu/config/Private.arm64.MacOSX.exports | config/Private.arm64.MacOSX.exports | PureDarwin (last 2026-09-05) | +0/-4 | notice |
| darwin/src/Kernel/xnu/config/Private.arm64.exports | config/Private.arm64.exports | PureDarwin (last 2026-10-07) | +1/-65 | notice |
| darwin/src/Kernel/xnu/config/Private.exports | config/Private.exports | PureDarwin (last 2026-09-28) | +3/-8 | notice |
| darwin/src/Kernel/xnu/config/Private.x86_64.exports | config/Private.x86_64.exports | PureDarwin (last 2026-10-06) | +2/-0 | notice |
| darwin/src/Kernel/xnu/config/Unsupported.arm64.exports | config/Unsupported.arm64.exports | PureDarwin (last 2026-11-10) | +11/-0 | notice |
| darwin/src/Kernel/xnu/config/Unsupported.x86_64.exports | config/Unsupported.x86_64.exports | PureDarwin (last 2026-09-19) | +1/-0 | notice |
| darwin/src/Kernel/xnu/config/newvers.pl | config/newvers.pl | PureDarwin (last 2026-10-11) | +2/-0 | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOUserServer.iig | iokit/DriverKit/IOUserServer.iig | PureDarwin (last 2026-08-30) | +2/-8 | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/Makefile | iokit/DriverKit/Makefile | PureDarwin (last 2026-07-30) | +11/-42 | notice |
| darwin/src/Kernel/xnu/iokit/IOKit/IODeviceTreeSupport.h | iokit/IOKit/IODeviceTreeSupport.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/iokit/IOKit/IOHibernatePrivate.h | iokit/IOKit/IOHibernatePrivate.h | kido (2026-11-23) | +8/-0 | notice |
| darwin/src/Kernel/xnu/iokit/IOKit/IORPC.h | iokit/IOKit/IORPC.h | PureDarwin (last 2026-08-30) | +0/-4 | notice |
| darwin/src/Kernel/xnu/iokit/Kernel/IODeviceTreeSupport.cpp | iokit/Kernel/IODeviceTreeSupport.cpp | PureDarwin (last 2026-11-10) | +7/-5 | notice |
| darwin/src/Kernel/xnu/iokit/Kernel/IOMemoryDescriptor.cpp | iokit/Kernel/IOMemoryDescriptor.cpp | PureDarwin (last 2026-09-28) | +13/-4 | notice |
| darwin/src/Kernel/xnu/iokit/Kernel/IOPMrootDomain.cpp | iokit/Kernel/IOPMrootDomain.cpp | PureDarwin (last 2026-10-09) | +15/-0 | notice |
| darwin/src/Kernel/xnu/iokit/Kernel/IOPlatformExpert.cpp | iokit/Kernel/IOPlatformExpert.cpp | PureDarwin (last 2026-10-09) | +8/-0 | notice |
| darwin/src/Kernel/xnu/iokit/Kernel/IOReporter.cpp | iokit/Kernel/IOReporter.cpp | PureDarwin (last 2026-09-28) | +10/-0 | notice |
| darwin/src/Kernel/xnu/iokit/Kernel/IOService.cpp | iokit/Kernel/IOService.cpp | PureDarwin (last 2026-10-28) | +5/-3 | notice |
| darwin/src/Kernel/xnu/iokit/Kernel/IOStartIOKit.cpp | iokit/Kernel/IOStartIOKit.cpp | PureDarwin (last 2026-11-10); kido (2026-11-18) | +39/-2 | notice |
| darwin/src/Kernel/xnu/iokit/Kernel/IOUserClient.cpp | iokit/Kernel/IOUserClient.cpp | PureDarwin (last 2026-10-05) | +27/-6 | notice |
| darwin/src/Kernel/xnu/iokit/Kernel/IOUserServer.cpp | iokit/Kernel/IOUserServer.cpp | PureDarwin (last 2026-09-05) | +17/-18 | notice |
| darwin/src/Kernel/xnu/iokit/bsddev/IOKitBSDInit.cpp | iokit/bsddev/IOKitBSDInit.cpp | PureDarwin (last 2026-09-27) | +29/-1 | notice |
| darwin/src/Kernel/xnu/iokit/conf/files | iokit/conf/files | PureDarwin (last 2026-08-30) | +18/-18 | notice |
| darwin/src/Kernel/xnu/iokit/conf/files.x86_64 | iokit/conf/files.x86_64 | PureDarwin (last 2026-10-06) | +3/-0 | notice |
| darwin/src/Kernel/xnu/libkern/Makefile | libkern/Makefile | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/libkern/c++/OSKext.cpp | libkern/c++/OSKext.cpp | PureDarwin (last 2026-10-28) | +59/-10 | notice |
| darwin/src/Kernel/xnu/libkern/gen/OSDebug.cpp | libkern/gen/OSDebug.cpp | PureDarwin (last 2026-09-28) | +8/-2 | notice |
| darwin/src/Kernel/xnu/libkern/libkern/Makefile | libkern/libkern/Makefile | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Kernel/xnu/libkern/libkern/OSAtomic.h | libkern/libkern/OSAtomic.h | PureDarwin (last 2026-09-28) | +6/-0 | notice |
| darwin/src/Kernel/xnu/libkern/libkern/OSByteOrder.h | libkern/libkern/OSByteOrder.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/libkern/libkern/OSMalloc.h | libkern/libkern/OSMalloc.h | PureDarwin (last 2026-08-30) | +2/-2 | notice |
| darwin/src/Kernel/xnu/libkern/libkern/_OSByteOrder.h | libkern/libkern/_OSByteOrder.h | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Kernel/xnu/libkern/libkern/c++/OSMetaClass.h | libkern/libkern/c++/OSMetaClass.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/libkern/libkern/riscv/OSByteOrder.h | libkern/libkern/arm/OSByteOrder.h (copy, 97% alike) | PureDarwin (last 2026-09-28) | +6/-7 | notice |
| darwin/src/Kernel/xnu/libkern/libkern/riscv/_OSByteOrder.h | libkern/libkern/arm/_OSByteOrder.h (copy, 95% alike) | PureDarwin (last 2026-09-28) | +5/-5 | notice |
| darwin/src/Kernel/xnu/libkern/libkern/section_keywords.h | libkern/libkern/section_keywords.h | PureDarwin (last 2026-09-28) | +2/-2 | notice |
| darwin/src/Kernel/xnu/libkern/os/atomic_private.h | libkern/os/atomic_private.h | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Kernel/xnu/libkern/os/atomic_private_arch.h | libkern/os/atomic_private_arch.h | PureDarwin (last 2026-09-28) | +20/-0 | notice |
| darwin/src/Kernel/xnu/libsa/bootstrap.cpp | libsa/bootstrap.cpp | PureDarwin (last 2026-08-30) | +3/-2 | notice |
| darwin/src/Kernel/xnu/libsyscall/mach/mach/mach_interface.h | libsyscall/mach/mach/mach_interface.h | PureDarwin (last 2026-08-30) | +8/-0 | notice |
| darwin/src/Kernel/xnu/makedefs/MakeInc.cmd | makedefs/MakeInc.cmd | replaced by PureDarwin's own file | +1/-579 | none needed |
| darwin/src/Kernel/xnu/makedefs/MakeInc.def | makedefs/MakeInc.def | replaced by PureDarwin's own file | +1/-1605 | none needed |
| darwin/src/Kernel/xnu/makedefs/MakeInc.top | makedefs/MakeInc.top | PureDarwin (last 2026-08-30) | +9/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/Makefile | osfmk/Makefile | PureDarwin (last 2026-09-28) | +7/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/arm/arm_init.c | osfmk/arm/arm_init.c | PureDarwin (last 2026-09-28) | +29/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/arm/commpage/commpage.c | osfmk/arm/commpage/commpage.c | PureDarwin (last 2026-09-28) | +6/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/arm/commpage/commpage_asm.s | osfmk/arm/commpage/commpage_asm.s | PureDarwin (last 2026-09-28) | +19/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/arm/cpuid.c | osfmk/arm/cpuid.c | PureDarwin (last 2026-09-28) | +17/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/arm/cpuid.h | osfmk/arm/cpuid.h | PureDarwin (last 2026-09-13) | +1/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/arm/model_dep.c | osfmk/arm/model_dep.c | PureDarwin (last 2026-11-10) | +6/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/arm/pmap/pmap.c | osfmk/arm/pmap/pmap.c | PureDarwin (last 2026-10-05) | +23/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/arm/pmap/pmap_data.h | osfmk/arm/pmap/pmap_data.h | PureDarwin (last 2026-09-05) | +14/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/arm_vm_init.c | osfmk/arm64/arm_vm_init.c | PureDarwin (last 2026-09-05) | +22/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/caches_asm.s | osfmk/arm64/caches_asm.s | PureDarwin (last 2026-11-10) | +63/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/cpu.c | osfmk/arm64/cpu.c | PureDarwin (last 2026-11-10) | +45/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/machine_routines.c | osfmk/arm64/machine_routines.c | PureDarwin (last 2026-11-10) | +64/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/pcb.c | osfmk/arm64/pcb.c | PureDarwin (last 2026-09-14) | +2/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/proc_reg.h | osfmk/arm64/proc_reg.h | PureDarwin (last 2026-10-03) | +18/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/sleh.c | osfmk/arm64/sleh.c | PureDarwin (last 2026-11-10); kido (2026-11-18) | +1193/-9 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/sptm/pmap/pmap.c | osfmk/arm64/sptm/pmap/pmap.c | PureDarwin (last 2026-10-05) | +1/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/start.s | osfmk/arm64/start.s | PureDarwin (last 2026-11-10) | +61/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/arm64/status.c | osfmk/arm64/status.c | PureDarwin (last 2026-09-14) | +24/-5 | notice |
| darwin/src/Kernel/xnu/osfmk/atm/atm.c | osfmk/atm/atm.c | PureDarwin (last 2026-08-30) | +14/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/conf/Makefile.arm64 | osfmk/conf/Makefile.arm64 | PureDarwin (last 2026-09-13) | +4/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/conf/Makefile.template | osfmk/conf/Makefile.template | PureDarwin (last 2026-09-28); kido (2026-11-23) | +2/-6 | notice |
| darwin/src/Kernel/xnu/osfmk/conf/files | osfmk/conf/files | PureDarwin (last 2026-08-30); kido (2026-11-23) | +43/-33 | notice |
| darwin/src/Kernel/xnu/osfmk/conf/files.arm64 | osfmk/conf/files.arm64 | PureDarwin (last 2026-08-30); kido (2026-11-23) | +1/-5 | notice |
| darwin/src/Kernel/xnu/osfmk/conf/files.x86_64 | osfmk/conf/files.x86_64 | PureDarwin (last 2026-09-11) | +1/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/console/serial_console.c | osfmk/console/serial_console.c | PureDarwin (last 2026-09-28) | +6/-4 | notice |
| darwin/src/Kernel/xnu/osfmk/console/serial_general.c | osfmk/console/serial_general.c | PureDarwin (last 2026-11-10) | +46/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/console/video_console.c | osfmk/console/video_console.c | PureDarwin (last 2026-09-27) | +50/-5 | notice |
| darwin/src/Kernel/xnu/osfmk/device/iokit_rpc.c | osfmk/device/iokit_rpc.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/device/subrs.c | osfmk/device/subrs.c | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/AT386/model_dep.c | osfmk/i386/AT386/model_dep.c | PureDarwin (last 2026-10-11) | +72/-4 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/acpi.c | osfmk/i386/acpi.c | PureDarwin (last 2026-08-30) | +32/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/bsd_i386_native.c | osfmk/i386/bsd_i386_native.c | PureDarwin (last 2026-11-10) | +16/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/commpage/commpage.c | osfmk/i386/commpage/commpage.c | PureDarwin (last 2026-08-30) | +26/-6 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/cpu.c | osfmk/i386/cpu.c | PureDarwin (last 2026-11-10) | +2/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/cpu_threads.c | osfmk/i386/cpu_threads.c | PureDarwin (last 2026-11-10) | +139/-17 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/cpu_topology.c | osfmk/i386/cpu_topology.c | PureDarwin (last 2026-10-06) | +4/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/cpuid.c | osfmk/i386/cpuid.c | PureDarwin (last 2026-11-10) | +698/-397 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/cpuid.h | osfmk/i386/cpuid.h | PureDarwin (last 2026-10-19) | +117/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/fpu.c | osfmk/i386/fpu.c | PureDarwin (last 2026-08-30) | +5/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/genassym.c | osfmk/i386/genassym.c | PureDarwin (last 2026-10-11) | +8/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/i386_init.c | osfmk/i386/i386_init.c | PureDarwin (last 2026-10-13) | +60/-13 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/i386_vm_init.c | osfmk/i386/i386_vm_init.c | PureDarwin (last 2026-10-19) | +59/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/lapic.h | osfmk/i386/lapic.h | PureDarwin (last 2026-09-19) | +3/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/lapic_native.c | osfmk/i386/lapic_native.c | PureDarwin (last 2026-09-19) | +11/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/machdep_call.c | osfmk/i386/machdep_call.c | PureDarwin (last 2026-11-10) | +5/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/machdep_call.h | osfmk/i386/machdep_call.h | PureDarwin (last 2026-11-10) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/machine_check.c | osfmk/i386/machine_check.c | PureDarwin (last 2026-11-10) | +2/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/mp.c | osfmk/i386/mp.c | PureDarwin (last 2026-11-10) | +134/-15 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/mp.h | osfmk/i386/mp.h | PureDarwin (last 2026-08-30) | +6/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/mp_desc.c | osfmk/i386/mp_desc.c | PureDarwin (last 2026-09-19) | +11/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/mp_native.c | osfmk/i386/mp_native.c | PureDarwin (last 2026-11-10) | +6/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/mtrr.c | osfmk/i386/mtrr.c | PureDarwin (last 2026-08-30) | +5/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/pcb_native.c | osfmk/i386/pcb_native.c | PureDarwin (last 2026-11-10) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/pmap_internal.h | osfmk/i386/pmap_internal.h | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/pmap_x86_common.c | osfmk/i386/pmap_x86_common.c | PureDarwin (last 2026-11-10) | +72/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/postcode.h | osfmk/i386/postcode.h | PureDarwin (last 2026-10-13) | +54/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/proc_reg.h | osfmk/i386/proc_reg.h | PureDarwin (last 2026-10-19) | +6/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/rtclock.c | osfmk/i386/rtclock.c | PureDarwin (last 2026-09-19) | +19/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/thread.h | osfmk/i386/thread.h | PureDarwin (last 2026-11-10) | +1/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/trap.c | osfmk/i386/trap.c | PureDarwin (last 2026-09-14) | +143/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/tsc.c | osfmk/i386/tsc.c | PureDarwin (last 2026-11-10) | +408/-92 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/tsc.h | osfmk/i386/tsc.h | PureDarwin (last 2026-10-13) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/i386/x86_hypercall.h | osfmk/i386/x86_hypercall.h | PureDarwin (last 2026-08-30) | +26/-6 | notice |
| darwin/src/Kernel/xnu/osfmk/ipc/ipc_kmsg.c | osfmk/ipc/ipc_kmsg.c | PureDarwin (last 2026-09-29) | +3/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/ipc/ipc_object.c | osfmk/ipc/ipc_object.c | PureDarwin (last 2026-09-29) | +3/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/ipc/ipc_policy.c | osfmk/ipc/ipc_policy.c | PureDarwin (last 2026-08-30) | +9/-5 | notice |
| darwin/src/Kernel/xnu/osfmk/ipc/ipc_space.h | osfmk/ipc/ipc_space.h | kido (2026-11-23) | +1/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/ipc/mach_debug.c | osfmk/ipc/mach_debug.c | PureDarwin (last 2026-08-30) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/ipc/mach_port.c | osfmk/ipc/mach_port.c | PureDarwin (last 2026-09-27) | +6/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/kdp/kdp_core.c | osfmk/kdp/kdp_core.c | PureDarwin (last 2026-09-28) | +17/-17 | notice |
| darwin/src/Kernel/xnu/osfmk/kdp/kdp_udp.c | osfmk/kdp/kdp_udp.c | PureDarwin (last 2026-09-28) | +5/-5 | notice |
| darwin/src/Kernel/xnu/osfmk/kdp/ml/riscv/kdp_machdep.c | osfmk/kdp/ml/arm/kdp_machdep.c (copy, 75% alike) | PureDarwin (last 2026-09-28) | +98/-188 | notice |
| darwin/src/Kernel/xnu/osfmk/kdp/ml/riscv/kdp_vm.c | osfmk/kdp/ml/arm/kdp_vm.c (copy, 75% alike) | PureDarwin (last 2026-09-28) | +85/-166 | notice |
| darwin/src/Kernel/xnu/osfmk/kdp/ml/x86_64/kdp_machdep.c | osfmk/kdp/ml/x86_64/kdp_machdep.c | PureDarwin (last 2026-08-30) | +38/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/kdp/processor_core.c | osfmk/kdp/processor_core.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/Makefile | osfmk/kern/Makefile | PureDarwin (last 2026-08-30) | +9/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/arcade.c | osfmk/kern/arcade.c | PureDarwin (last 2026-08-30) | +4/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/backtrace.c | osfmk/kern/backtrace.c | PureDarwin (last 2026-09-28) | +48/-5 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/debug.c | osfmk/kern/debug.c | PureDarwin (last 2026-09-28) | +32/-22 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/ecc_init.h | osfmk/kern/ecc_init.h | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/ecc_logging.c | osfmk/kern/ecc_logging.c | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/ipc_host.c | osfmk/kern/ipc_host.c | PureDarwin (last 2026-08-30) | +1/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/ipc_kobject.c | osfmk/kern/ipc_kobject.c | PureDarwin (last 2026-08-30) | +7/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/ipc_tt.c | osfmk/kern/ipc_tt.c | PureDarwin (last 2026-08-30) | +1/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/kalloc.c | osfmk/kern/kalloc.c | PureDarwin (last 2026-10-28) | +19/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/kern_stackshot.c | osfmk/kern/kern_stackshot.c | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/lock_rw_old.c | osfmk/kern/lock_rw_old.c | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/lock_ticket.c | osfmk/kern/lock_ticket.c | PureDarwin (last 2026-11-10) | +16/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/locks.c | osfmk/kern/locks.c | PureDarwin (last 2026-09-27) | +7/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/machine.c | osfmk/kern/machine.c | PureDarwin (last 2026-08-30) | +1/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/mk_timer.c | osfmk/kern/mk_timer.c | PureDarwin (last 2026-08-30) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/printf.c | osfmk/kern/printf.c | PureDarwin (last 2026-11-10) | +13/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/processor.c | osfmk/kern/processor.c | PureDarwin (last 2026-11-10) | +104/-7 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/processor.h | osfmk/kern/processor.h | PureDarwin (last 2026-11-10) | +3/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/sched.h | osfmk/kern/sched.h | PureDarwin (last 2026-08-30) | +3/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/sched_common.c | osfmk/kern/sched_common.c | PureDarwin (last 2026-10-06) | +6/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/startup.c | osfmk/kern/startup.c | PureDarwin (last 2026-11-10) | +64/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/syscall_subr.c | osfmk/kern/syscall_subr.c | PureDarwin (last 2026-08-30) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/task.c | osfmk/kern/task.c | PureDarwin (last 2026-09-14) | +8/-4 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/task.h | osfmk/kern/task.h | PureDarwin (last 2026-09-14) | +6/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/task_ident.h | osfmk/kern/task_ident.h | PureDarwin (last 2026-08-30) | +4/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/thread.c | osfmk/kern/thread.c | PureDarwin (last 2026-09-27) | +5/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/thread_kernel_state.h | osfmk/kern/thread_kernel_state.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/trap_telemetry.c | osfmk/kern/trap_telemetry.c | PureDarwin (last 2026-09-28) | +6/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/waitq.h | osfmk/kern/waitq.h | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/kern/zalloc.c | osfmk/kern/zalloc.c | PureDarwin (last 2026-09-28) | +3/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/kperf/callstack.c | osfmk/kperf/callstack.c | PureDarwin (last 2026-09-28) | +165/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/kperf/kptimer.c | osfmk/kperf/kptimer.c | PureDarwin (last 2026-09-28) | +5/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/libsa/machine/types.h | osfmk/libsa/machine/types.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/Makefile | osfmk/mach/Makefile | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/arm/vm_param.h | osfmk/mach/arm/vm_param.h | PureDarwin (last 2026-09-28) | +3/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/host_special_ports.h | osfmk/mach/host_special_ports.h | PureDarwin (last 2026-08-30) | +3/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/mach_eventlink.defs | osfmk/mach/mach_eventlink.defs | PureDarwin (last 2026-08-30) | +3/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/mach_types.defs | osfmk/mach/mach_types.defs | PureDarwin (last 2026-08-30) | +0/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/mach_types.h | osfmk/mach/mach_types.h | PureDarwin (last 2026-08-30) | +37/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine.h | osfmk/mach/machine.h | PureDarwin (last 2026-11-10) | +40/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/_structs.h | osfmk/mach/machine/_structs.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/asm.h | osfmk/mach/machine/asm.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/boolean.h | osfmk/mach/machine/boolean.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/exception.h | osfmk/mach/machine/exception.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/kern_return.h | osfmk/mach/machine/kern_return.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/ndr_def.h | osfmk/mach/machine/ndr_def.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/processor_info.h | osfmk/mach/machine/processor_info.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/rpc.h | osfmk/mach/machine/rpc.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/sdt_isa.h | osfmk/mach/machine/sdt_isa.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/syscall_sw.h | osfmk/mach/machine/syscall_sw.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/thread_state.h | osfmk/mach/machine/thread_state.h | PureDarwin (last 2026-09-28) | +6/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/thread_status.h | osfmk/mach/machine/thread_status.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/vm_param.h | osfmk/mach/machine/vm_param.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/machine/vm_types.h | osfmk/mach/machine/vm_types.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/port.h | osfmk/mach/port.h | PureDarwin (last 2026-08-30) | +3/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/boolean.h | osfmk/mach/machine/boolean.h (copy, 78% alike) | PureDarwin (last 2026-09-28) | +7/-10 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/kern_return.h | osfmk/mach/machine/kern_return.h (copy, 76% alike) | PureDarwin (last 2026-09-28) | +9/-10 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/ndr_def.h | osfmk/mach/arm/ndr_def.h (copy, 93% alike) | PureDarwin (last 2026-09-28) | +0/-6 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/sdt_isa.h | osfmk/mach/arm/sdt_isa.h (copy, 69% alike) | PureDarwin (last 2026-09-28) | +71/-78 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/syscall_sw.h | osfmk/mach/machine/syscall_sw.h (copy, 60% alike) | PureDarwin (last 2026-09-28) | +21/-16 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/thread_state.h | osfmk/mach/arm/thread_state.h (copy, 78% alike) | PureDarwin (last 2026-09-28) | +7/-11 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/shared_region.h | osfmk/mach/shared_region.h | PureDarwin (last 2026-09-28) | +15/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/task_info.h | osfmk/mach/task_info.h | PureDarwin (last 2026-08-30) | +3/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/task_special_ports.h | osfmk/mach/task_special_ports.h | PureDarwin (last 2026-08-30) | +3/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/vm_statistics.h | osfmk/mach/vm_statistics.h | PureDarwin (last 2026-08-30) | +3/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/asm.h | osfmk/machine/asm.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/atomic.h | osfmk/machine/atomic.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/commpage.h | osfmk/machine/commpage.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/config.h | osfmk/machine/config.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/cpu_affinity.h | osfmk/machine/cpu_affinity.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/cpu_capabilities.h | osfmk/machine/cpu_capabilities.h | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/cpu_data.h | osfmk/machine/cpu_data.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/cpu_number.h | osfmk/machine/cpu_number.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/endian.h | osfmk/machine/endian.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/lock.h | osfmk/machine/lock.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/locks.h | osfmk/machine/locks.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/lowglobals.h | osfmk/machine/lowglobals.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/machine_cpu.h | osfmk/machine/machine_cpu.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/machine_cpuid.h | osfmk/machine/machine_cpuid.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/machine_routines.h | osfmk/machine/machine_routines.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/machine_rpc.h | osfmk/machine/machine_rpc.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/machlimits.h | osfmk/machine/machlimits.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/machparam.h | osfmk/machine/machparam.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/memory_types.h | osfmk/machine/memory_types.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/pal_hibernate.h | osfmk/machine/pal_hibernate.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/pal_routines.h | osfmk/machine/pal_routines.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/pmap.h | osfmk/machine/pmap.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/sched_param.h | osfmk/machine/sched_param.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/setjmp.h | osfmk/machine/setjmp.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/simple_lock.h | osfmk/machine/simple_lock.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/smp.h | osfmk/machine/smp.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/static_if.h | osfmk/machine/static_if.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/task.h | osfmk/machine/task.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/thread.h | osfmk/machine/thread.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/trap.h | osfmk/machine/trap.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/trap_internal.h | osfmk/machine/trap_internal.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/machine/vm_tuning.h | osfmk/machine/vm_tuning.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/prng/entropy.c | osfmk/prng/entropy.c | PureDarwin (last 2026-09-05) | +2/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/prng/prng_random.c | osfmk/prng/prng_random.c | kido (2026-11-23) | +1/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/atomic.h | osfmk/i386/atomic.h (copy, 76% alike) | PureDarwin (last 2026-09-28) | +14/-6 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/caches_internal.h | osfmk/arm/caches_internal.h (copy, 68% alike) | PureDarwin (last 2026-09-28) | +5/-45 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/commpage/commpage.h | osfmk/arm/commpage/commpage.h (copy, 94% alike) | PureDarwin (last 2026-09-28) | +3/-4 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/counter.c | osfmk/arm/counter.c (copy, 96% alike) | PureDarwin (last 2026-09-28) | +2/-5 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/cpu_affinity.h | osfmk/arm/cpu_affinity.h (copy, 93% alike) | PureDarwin (last 2026-09-28) | +3/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/cpu_capabilities.h | osfmk/arm/cpu_capabilities.h (copy, 74% alike) | PureDarwin (last 2026-09-28) | +40/-149 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/cpu_common.c | osfmk/arm/cpu_common.c (copy, 87% alike) | PureDarwin (last 2026-09-28) | +45/-221 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/cpu_data.h | osfmk/arm/cpu_data.h (copy, 62% alike) | PureDarwin (last 2026-09-28) | +8/-74 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/cpu_data_internal.h | osfmk/arm/cpu_data_internal.h (copy, 73% alike) | PureDarwin (last 2026-09-28) | +23/-128 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/cpu_internal.h | osfmk/arm/cpu_internal.h (copy, 76% alike) | PureDarwin (last 2026-09-28) | +10/-25 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/cpu_number.h | osfmk/machine/cpu_number.h (copy, 74% alike) | PureDarwin (last 2026-09-28) | +12/-11 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/cpu_topology.h | osfmk/arm/cpu_topology.h (copy, 88% alike) | PureDarwin (last 2026-09-28) | +9/-18 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/io_map.c | osfmk/arm/io_map.c (copy, 92% alike) | PureDarwin (last 2026-09-28) | +6/-13 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/lock.h | osfmk/machine/lock.h (copy, 78% alike) | PureDarwin (last 2026-09-28) | +6/-12 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/locks.h | osfmk/arm/locks.h (copy, 90% alike) | PureDarwin (last 2026-09-28) | +12/-19 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/loose_ends.c | osfmk/arm64/loose_ends.c (copy, 82% alike) | PureDarwin (last 2026-09-28) | +53/-307 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/lowmem_vectors.c | osfmk/arm64/lowmem_vectors.c (copy, 84% alike) | PureDarwin (last 2026-09-28) | +11/-33 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/machine_cpuid.h | osfmk/machine/machine_cpuid.h (copy, 69% alike) | PureDarwin (last 2026-09-28) | +10/-15 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/machine_remote_time.c | osfmk/arm64/machine_remote_time.c (copy, 91% alike) | PureDarwin (last 2026-09-28) | +9/-9 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/machine_routines.h | osfmk/arm/machine_routines.h (copy, 76% alike) | PureDarwin (last 2026-09-28) | +92/-553 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/machine_routines_common.c | osfmk/arm/machine_routines_common.c (copy, 85% alike) | PureDarwin (last 2026-09-28) | +251/-221 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/machparam.h | osfmk/machine/machparam.h (copy, 79% alike) | PureDarwin (last 2026-09-28) | +1/-13 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/memory_types.h | osfmk/machine/memory_types.h (copy, 69% alike) | PureDarwin (last 2026-09-28) | +16/-10 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/model_dep.c | osfmk/arm/model_dep.c (copy, 77% alike) | PureDarwin (last 2026-09-28) | +104/-509 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/pal_hibernate.h | osfmk/arm64/pal_hibernate.h (copy, 64% alike) | PureDarwin (last 2026-09-28) | +4/-65 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/pal_routines.c | osfmk/arm/pal_routines.c (copy, 94% alike) | PureDarwin (last 2026-09-28) | +2/-5 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/pal_routines.h | osfmk/arm/pal_routines.h (copy, 95% alike) | PureDarwin (last 2026-09-28) | +3/-4 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/preemption_disable.c | osfmk/arm/preemption_disable.c (copy, 77% alike) | PureDarwin (last 2026-09-28) | +44/-157 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/sched_param.h | osfmk/machine/sched_param.h (copy, 77% alike) | PureDarwin (last 2026-09-28) | +4/-12 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/setjmp.h | osfmk/machine/setjmp.h (copy, 75% alike) | PureDarwin (last 2026-09-28) | +8/-11 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/simple_lock.h | osfmk/arm/simple_lock.h (copy, 78% alike) | PureDarwin (last 2026-09-28) | +6/-49 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/smp.h | osfmk/i386/smp.h (copy, 87% alike) | PureDarwin (last 2026-09-28) | +6/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/static_if.c | osfmk/arm64/static_if.c (copy, 72% alike) | PureDarwin (last 2026-09-28) | +28/-22 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/static_if.h | osfmk/arm64/static_if.h (copy, 95% alike) | PureDarwin (last 2026-09-28) | +3/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/string.h | osfmk/i386/string.h (copy, 82% alike) | PureDarwin (last 2026-09-28) | +4/-11 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/task.h | osfmk/machine/task.h (copy, 74% alike) | PureDarwin (last 2026-09-28) | +8/-12 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/trap.h | osfmk/arm/trap.h (copy, 60% alike) | PureDarwin (last 2026-09-28) | +26/-83 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/trap_internal.h | osfmk/machine/trap_internal.h (copy, 62% alike) | PureDarwin (last 2026-09-28) | +23/-11 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/vm_tuning.h | osfmk/machine/vm_tuning.h (copy, 77% alike) | PureDarwin (last 2026-09-28) | +4/-12 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/device_vm.c | osfmk/vm/device_vm.c | PureDarwin (last 2026-10-19) | +19/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/pmap.h | osfmk/vm/pmap.h | PureDarwin (last 2026-09-28) | +2/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_compressor.c | osfmk/vm/vm_compressor.c | PureDarwin (last 2026-09-28) | +3/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_compressor_xnu.h | osfmk/vm/vm_compressor_xnu.h | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_dyld_pager.c | osfmk/vm/vm_dyld_pager.c | PureDarwin (last 2026-09-14) | +47/-23 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_fault.c | osfmk/vm/vm_fault.c | PureDarwin (last 2026-10-28) | +217/-17 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_iokit.h | osfmk/vm/vm_iokit.h | PureDarwin (last 2026-10-19) | +2/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_map.c | osfmk/vm/vm_map.c | PureDarwin (last 2026-09-28) | +23/-17 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_map_xnu.h | osfmk/vm/vm_map_xnu.h | PureDarwin (last 2026-09-27) | +3/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_object.c | osfmk/vm/vm_object.c | PureDarwin (last 2026-09-29) | +3/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_page.h | osfmk/vm/vm_page.h | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_protos.h | osfmk/vm/vm_protos.h | PureDarwin (last 2026-09-14) | +4/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_protos_internal.h | osfmk/vm/vm_protos_internal.h | PureDarwin (last 2026-09-14) | +12/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_resident.c | osfmk/vm/vm_resident.c | PureDarwin (last 2026-09-28) | +3/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_shared_region.c | osfmk/vm/vm_shared_region.c | PureDarwin (last 2026-09-28) | +122/-41 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_shared_region_internal.h | osfmk/vm/vm_shared_region_internal.h | PureDarwin (last 2026-09-14) | +2/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_shared_region_pager.c | osfmk/vm/vm_shared_region_pager.c | PureDarwin (last 2026-09-28) | +145/-21 | notice |
| darwin/src/Kernel/xnu/osfmk/vm/vm_shared_region_xnu.h | osfmk/vm/vm_shared_region_xnu.h | PureDarwin (last 2026-09-14) | +19/-4 | notice |
| darwin/src/Kernel/xnu/osfmk/x86_64/boot_pt.c | osfmk/x86_64/boot_pt.c | PureDarwin (last 2026-10-13) | +8/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/x86_64/cpc_x86_64.c | osfmk/x86_64/cpc_x86_64.c | PureDarwin (last 2026-11-10) | +15/-1 | notice |
| darwin/src/Kernel/xnu/osfmk/x86_64/pmap_pcid.c | osfmk/x86_64/pmap_pcid.c | PureDarwin (last 2026-08-30) | +1/-0 | notice |
| darwin/src/Kernel/xnu/osfmk/x86_64/start.s | osfmk/x86_64/start.s | PureDarwin (last 2026-10-13) | +157/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/Makefile | pexpert/Makefile | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/arm/pe_init.c | pexpert/arm/pe_init.c | PureDarwin (last 2026-11-10) | +33/-2 | notice |
| darwin/src/Kernel/xnu/pexpert/arm/pe_serial.c | pexpert/arm/pe_serial.c | PureDarwin (last 2026-11-10) | +343/-1 | notice |
| darwin/src/Kernel/xnu/pexpert/i386/pe_init.c | pexpert/i386/pe_init.c | PureDarwin (last 2026-09-27) | +11/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/i386/pe_interrupt.c | pexpert/i386/pe_interrupt.c | PureDarwin (last 2026-08-30) | +6/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/i386/pe_kprintf.c | pexpert/i386/pe_kprintf.c | PureDarwin (last 2026-11-10) | +30/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/i386/pe_serial.c | pexpert/i386/pe_serial.c | PureDarwin (last 2026-08-30) | +25/-6 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/Makefile | pexpert/pexpert/Makefile | PureDarwin (last 2026-09-28) | +3/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/arm/protos.h | pexpert/pexpert/arm/protos.h | PureDarwin (last 2026-11-10) | +3/-1 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/arm64/Makefile | pexpert/pexpert/arm64/Makefile | PureDarwin (last 2026-11-10) | +5/-1 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/arm64/apple_arm64_common.h | pexpert/pexpert/arm64/apple_arm64_common.h | PureDarwin (last 2026-11-10) | +8/-1 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/arm64/apple_arm64_cpu.h | pexpert/pexpert/arm64/apple_arm64_cpu.h | PureDarwin (last 2026-09-05) | +3/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/arm64/apple_arm64_regs.h | pexpert/pexpert/arm64/apple_arm64_regs.h | PureDarwin (last 2026-11-10) | +45/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/arm64/board_config.h | pexpert/pexpert/arm64/board_config.h | PureDarwin (last 2026-11-10) | +52/-2 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/machine/boot.h | pexpert/pexpert/machine/boot.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/machine/protos.h | pexpert/pexpert/machine/protos.h | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/pexpert.h | pexpert/pexpert/pexpert.h | PureDarwin (last 2026-09-28) | +3/-3 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/riscv/boot.h | pexpert/pexpert/arm64/boot.h (copy, 81% alike) | PureDarwin (last 2026-09-28) | +6/-20 | notice |
| darwin/src/Kernel/xnu/pexpert/riscv/pe_identify_machine.c | pexpert/arm/pe_identify_machine.c | PureDarwin (last 2026-09-28) | +335/-205 | notice |
| darwin/src/Kernel/xnu/pexpert/riscv/pe_init.c | pexpert/arm/pe_init.c | PureDarwin (last 2026-09-28) | +75/-260 | notice |
| darwin/src/Kernel/xnu/pexpert/riscv/pe_kprintf.c | pexpert/arm/pe_kprintf.c | PureDarwin (last 2026-09-28) | +9/-26 | notice |
| darwin/src/Kernel/xnu/pexpert/riscv/pe_serial.c | pexpert/arm/pe_serial.c | replaced by PureDarwin's own file | +434/-1136 | none needed |
| darwin/src/Kernel/xnu/san/coverage/Makefile | san/coverage/Makefile | PureDarwin (last 2026-08-30) | +4/-0 | notice |
| darwin/src/Kernel/xnu/san/coverage/kcov-denylist-x86_64 | san/coverage/kcov-denylist-x86_64 | kido (2026-11-23) | +4/-5 | notice |
| darwin/src/Kernel/xnu/san/memory/Makefile | san/memory/Makefile | PureDarwin (last 2026-08-30) | +4/-0 | notice |
| darwin/src/Kernel/xnu/san/memory/kasan-denylist-x86_64 | san/memory/kasan-denylist-x86_64 | kido (2026-11-23) | +4/-5 | notice |
| darwin/src/Kernel/xnu/san/memory/ubsan-denylist | san/memory/ubsan-denylist | kido (2026-11-23) | +1/-1 | notice |
| darwin/src/Kernel/xnu/security/mac_base.c | security/mac_base.c | PureDarwin (last 2026-09-27) | +1/-0 | notice |
| darwin/src/Kernel/xnu/security/mac_vfs.c | security/mac_vfs.c | PureDarwin (last 2026-08-30) | +1/-1 | notice |
| darwin/src/Kernel/xnu/tools/vm_sanitize_enforcement.py | tools/vm_sanitize_enforcement.py | PureDarwin (last 2026-08-30) | +5/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/kmod.h | osfmk/mach/kmod.h (copy, 95% alike) | PureDarwin (last 2026-07-16) | +0/-17 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/_structs.h | osfmk/mach/machine/_structs.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/asm.h | osfmk/mach/machine/asm.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/boolean.h | osfmk/mach/machine/boolean.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/exception.h | osfmk/mach/machine/exception.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/kern_return.h | osfmk/mach/machine/kern_return.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/ndr_def.h | osfmk/mach/machine/ndr_def.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/processor_info.h | osfmk/mach/machine/processor_info.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/rpc.h | osfmk/mach/machine/rpc.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/sdt.h | osfmk/mach/sdt.h (copy, 82% alike) | PureDarwin (last 2026-07-16) | +8/-4 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/sdt_isa.h | osfmk/mach/machine/sdt_isa.h (copy, 97% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/thread_status.h | osfmk/mach/machine/thread_status.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/vm_param.h | osfmk/mach/machine/vm_param.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/vm_types.h | osfmk/mach/machine/vm_types.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/mig.h | osfmk/mach/mig.h (copy, 89% alike) | PureDarwin (last 2026-07-16) | +1/-42 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/boolean.h | osfmk/mach/machine/boolean.h (copy, 78% alike) | PureDarwin (last 2026-09-28) | +7/-10 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/kern_return.h | osfmk/mach/machine/kern_return.h (copy, 76% alike) | PureDarwin (last 2026-09-28) | +9/-10 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/ndr_def.h | osfmk/mach/arm/ndr_def.h (copy, 93% alike) | PureDarwin (last 2026-09-28) | +0/-6 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/sdt_isa.h | osfmk/mach/arm/sdt_isa.h (copy, 62% alike) | PureDarwin (last 2026-09-28) | +72/-106 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/syscall_sw.h | osfmk/mach/machine/syscall_sw.h (copy, 60% alike) | PureDarwin (last 2026-09-28) | +21/-16 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/thread_state.h | osfmk/mach/arm/thread_state.h (copy, 75% alike) | PureDarwin (last 2026-09-28) | +11/-10 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/semaphore.h | osfmk/mach/semaphore.h (copy, 72% alike) | PureDarwin (last 2026-07-16) | +0/-62 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/shared_memory_server.h | osfmk/mach/shared_memory_server.h (copy, 99% alike) | PureDarwin (last 2026-07-16) | +0/-2 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/task_policy.h | osfmk/mach/task_policy.h (copy, 97% alike) | PureDarwin (last 2026-07-16) | +3/-7 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/thread_policy.h | osfmk/mach/thread_policy.h (copy, 97% alike) | PureDarwin (last 2026-07-16) | +8/-13 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/thread_switch.h | osfmk/mach/thread_switch.h (copy, 96% alike) | PureDarwin (last 2026-07-16) | +0/-7 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/Platforms/MacOSX/riscv64/syscall.map | libsyscall/Platforms/DriverKit/arm64/syscall.map (copy, 95% alike) | PureDarwin (last 2026-09-28) | +6/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/SYS.h | libsyscall/custom/SYS.h | PureDarwin (last 2026-09-28) | +51/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__fork.s | libsyscall/custom/__fork.s | PureDarwin (last 2026-09-28) | +28/-2 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__getpid.s | libsyscall/custom/__getpid.s | PureDarwin (last 2026-09-28) | +31/-11 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__gettimeofday.s | libsyscall/custom/__gettimeofday.s | PureDarwin (last 2026-09-28) | +12/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__kdebug_trace_string.s | libsyscall/custom/__kdebug_trace_string.s | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__lseek.s | libsyscall/custom/__lseek.s | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__pipe.s | libsyscall/custom/__pipe.s | PureDarwin (last 2026-09-28) | +10/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__ptrace.s | libsyscall/custom/__ptrace.s | PureDarwin (last 2026-09-28) | +9/-3 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__sigaltstack.s | libsyscall/custom/__sigaltstack.s | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__sigreturn.s | libsyscall/custom/__sigreturn.s | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__syscall.s | libsyscall/custom/__syscall.s | PureDarwin (last 2026-09-28) | +8/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__thread_selfid.s | libsyscall/custom/__thread_selfid.s | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__thread_selfusage.s | libsyscall/custom/__thread_selfusage.s | PureDarwin (last 2026-09-28) | +4/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/__vfork.s | libsyscall/custom/__vfork.s | PureDarwin (last 2026-09-28) | +37/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/custom.s | libsyscall/custom/custom.s | PureDarwin (last 2026-09-28) | +13/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/custom/errno.c | libsyscall/custom/errno.c | PureDarwin (last 2026-09-28) | +26/-7 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/err_libkern.sub | libsyscall/mach/err_libkern.sub | PureDarwin (last 2021-11-03) | +0/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/error_codes.c | libsyscall/mach/error_codes.c | PureDarwin (last 2021-11-03) | +40/-47 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/host.c | libsyscall/mach/host.c | PureDarwin (last 2026-08-30) | +2/-9 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/mach/mach_interface.h | libsyscall/mach/mach/mach_interface.h | PureDarwin (last 2026-08-30) | +12/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/mach_init.c | libsyscall/mach/mach_init.c | PureDarwin (last 2026-10-29) | +8/-2 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/mach_msg.c | libsyscall/mach/mach_msg.c | PureDarwin (last 2026-10-19) | +180/-167 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/mach_port.c | libsyscall/mach/mach_port.c | PureDarwin (last 2021-11-03) | +5/-53 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/mach_vm.c | libsyscall/mach/mach_vm.c | PureDarwin (last 2021-11-03) | +2/-363 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/mig_reply_port.c | libsyscall/mach/mig_reply_port.c | PureDarwin (last 2021-11-03) | +3/-18 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/port_descriptions.c | libsyscall/mach/port_descriptions.c | PureDarwin (last 2026-08-30) | +12/-5 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/string.h | libsyscall/mach/string.h | PureDarwin (last 2026-09-14) | +8/-6 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/task.c | libsyscall/mach/task.c | PureDarwin (last 2026-10-19) | +0/-8 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/mach/thread_act.c | libsyscall/mach/thread_act.c | PureDarwin (last 2021-11-03) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/os/proc.h | libsyscall/os/proc.h | PureDarwin (last 2021-11-23) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/os/tsd.h | libsyscall/os/tsd.h | PureDarwin (last 2026-09-28) | +41/-45 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/scripts/compile-syscalls.pl | libsyscall/xcodescripts/compile-syscalls.pl (copy, 81% alike) | PureDarwin (last 2026-08-13) | +46/-11 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/scripts/create-syscalls.pl | libsyscall/xcodescripts/create-syscalls.pl (copy, 99% alike) | PureDarwin (last 2026-09-28) | +10/-3 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/__get_cpu_capabilities.s | libsyscall/wrappers/__get_cpu_capabilities.s | PureDarwin (last 2026-09-28) | +10/-0 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/_libc_funcptr.c | libsyscall/wrappers/_libc_funcptr.c | PureDarwin (last 2026-08-30) | +302/-159 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/_libkernel_init.c | libsyscall/wrappers/_libkernel_init.c | PureDarwin (last 2026-08-30) | +20/-114 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/gethostuuid.h | libsyscall/wrappers/gethostuuid.h | PureDarwin (last 2026-09-14) | +2/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/init_cpu_capabilities.c | libsyscall/wrappers/init_cpu_capabilities.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/kdebug_trace.c | libsyscall/wrappers/kdebug_trace.c | PureDarwin (last 2026-07-14) | +18/-33 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/libproc/libproc.c | libsyscall/wrappers/libproc/libproc.c | PureDarwin (last 2024-03-09) | +2/-121 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/mach_absolute_time.s | libsyscall/wrappers/mach_absolute_time.s | PureDarwin (last 2026-09-28) | +72/-29 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/mach_approximate_time.c | libsyscall/wrappers/mach_approximate_time.c | PureDarwin (last 2026-09-28) | +1/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/mach_continuous_time.c | libsyscall/wrappers/mach_continuous_time.c | PureDarwin (last 2026-09-28) | +15/-17 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/proc.c | libsyscall/wrappers/proc.c | PureDarwin (last 2021-11-03) | +0/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/renamex.c | libsyscall/wrappers/renamex.c | PureDarwin (last 2021-11-03) | +0/-1 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/spawn/posix_spawn.c | libsyscall/wrappers/spawn/posix_spawn.c | PureDarwin (last 2026-10-19) | +159/-558 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/system-version-compat.c | libsyscall/wrappers/system-version-compat.c | PureDarwin (last 2024-03-09) | +20/-36 | notice |
| darwin/src/Libraries/libSystem/libsystem_kernel/wrappers/thread_register_state.c | libsyscall/wrappers/thread_register_state.c | PureDarwin (last 2026-09-28) | +18/-0 | notice |
| darwin/src/Libraries/libSystem/pthread/compat-include/os/base_private.h | libkern/os/base_private.h (copy, 95% alike) | PureDarwin (last 2026-07-09) | +0/-4 | notice |
| darwin/src/Libraries/objc4/macosx.internal/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/os/base_private.h | libkern/os/base_private.h (copy, 95% alike) | PureDarwin (last 2026-07-24) | +0/-4 | notice |
| darwin/tools/cctools/include/foreign/libkern/OSByteOrder.h | libkern/libkern/OSByteOrder.h (copy, 76% alike) | cctools-port and PureDarwin | +88/-95 | notice |
| darwin/tools/cctools/include/foreign/libkern/_OSByteOrder.h | libkern/libkern/_OSByteOrder.h (copy, 71% alike) | cctools-port and PureDarwin | +41/-33 | notice |
| darwin/tools/cctools/include/foreign/mach/boolean.h | osfmk/mach/boolean.h (copy, 76% alike) | cctools-port and PureDarwin | +21/-21 | notice |
| darwin/tools/cctools/include/foreign/mach/bootstrap.h | osfmk/mach/bootstrap.h (copy, 85% alike) | cctools-port and PureDarwin | +5/-5 | notice |
| darwin/tools/cctools/include/foreign/mach/clock_types.h | osfmk/mach/clock_types.h (copy, 61% alike) | cctools-port and PureDarwin | +49/-50 | notice |
| darwin/tools/cctools/include/foreign/mach/exception.h | osfmk/mach/exception.h (copy, 79% alike) | cctools-port and PureDarwin | +13/-13 | notice |
| darwin/tools/cctools/include/foreign/mach/host_notify.h | osfmk/mach/host_notify.h (copy, 74% alike) | cctools-port and PureDarwin | +9/-11 | notice |
| darwin/tools/cctools/include/foreign/mach/host_notify_reply.defs | osfmk/mach/host_notify_reply.defs (copy, 96% alike) | cctools-port and PureDarwin | +0/-3 | notice |
| darwin/tools/cctools/include/foreign/mach/host_reboot.h | osfmk/mach/host_reboot.h (copy, 85% alike) | cctools-port and PureDarwin | +6/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/ndr_def.h | osfmk/mach/i386/ndr_def.h (copy, 82% alike) | cctools-port and PureDarwin | +8/-8 | notice |
| darwin/tools/cctools/include/foreign/mach/kmod.h | osfmk/mach/kmod.h (copy, 69% alike) | cctools-port and PureDarwin | +49/-66 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_error.h | libsyscall/mach/mach/mach_error.h (copy, 75% alike) | cctools-port and PureDarwin | +23/-23 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_syscalls.h | osfmk/mach/mach_syscalls.h (copy, 84% alike) | cctools-port and PureDarwin | +6/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/asm.h | osfmk/mach/machine/asm.h (copy, 95% alike) | cctools-port and PureDarwin | +0/-4 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/boolean.h | osfmk/mach/machine/boolean.h (copy, 81% alike) | cctools-port and PureDarwin | +10/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/exception.h | osfmk/mach/machine/exception.h (copy, 81% alike) | cctools-port and PureDarwin | +10/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/kern_return.h | osfmk/mach/machine/kern_return.h (copy, 81% alike) | cctools-port and PureDarwin | +10/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/ndr_def.h | osfmk/mach/machine/ndr_def.h (copy, 87% alike) | cctools-port and PureDarwin | +4/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/processor_info.h | osfmk/mach/machine/processor_info.h (copy, 81% alike) | cctools-port and PureDarwin | +10/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/rpc.h | osfmk/mach/machine/rpc.h (copy, 79% alike) | cctools-port and PureDarwin | +11/-7 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/sdt.h | osfmk/mach/sdt.h (copy, 82% alike) | cctools-port and PureDarwin | +8/-4 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/sdt_isa.h | osfmk/mach/machine/sdt_isa.h (copy, 87% alike) | cctools-port and PureDarwin | +4/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/thread_status.h | osfmk/mach/machine/thread_status.h (copy, 81% alike) | cctools-port and PureDarwin | +10/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/vm_param.h | osfmk/mach/machine/vm_param.h (copy, 81% alike) | cctools-port and PureDarwin | +10/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/vm_types.h | osfmk/mach/machine/vm_types.h (copy, 81% alike) | cctools-port and PureDarwin | +10/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/mig.h | osfmk/mach/mig.h (copy, 67% alike) | cctools-port and PureDarwin | +42/-89 | notice |
| darwin/tools/cctools/include/foreign/mach/mig_errors.h | osfmk/mach/mig_errors.h (copy, 79% alike) | cctools-port and PureDarwin | +26/-27 | notice |
| darwin/tools/cctools/include/foreign/mach/ndr.h | osfmk/mach/ndr.h (copy, 75% alike) | cctools-port and PureDarwin | +52/-52 | notice |
| darwin/tools/cctools/include/foreign/mach/notify.h | osfmk/mach/notify.h (copy, 65% alike) | cctools-port and PureDarwin | +43/-52 | notice |
| darwin/tools/cctools/include/foreign/mach/policy.h | osfmk/mach/policy.h (copy, 62% alike) | cctools-port and PureDarwin | +108/-108 | notice |
| darwin/tools/cctools/include/foreign/mach/port_obj.h | libsyscall/mach/mach/port_obj.h (copy, 66% alike) | cctools-port and PureDarwin | +35/-35 | notice |
| darwin/tools/cctools/include/foreign/mach/ppc/ndr_def.h | osfmk/mach/i386/ndr_def.h (copy, 77% alike) | cctools-port and PureDarwin | +9/-11 | notice |
| darwin/tools/cctools/include/foreign/mach/ppc/processor_info.h | osfmk/mach/i386/processor_info.h (copy, 70% alike) | cctools-port and PureDarwin | +13/-12 | notice |
| darwin/tools/cctools/include/foreign/mach/riscv/boolean.h | osfmk/mach/machine/boolean.h (copy, 78% alike) | cctools-port and PureDarwin | +7/-10 | notice |
| darwin/tools/cctools/include/foreign/mach/riscv/kern_return.h | osfmk/mach/machine/kern_return.h (copy, 76% alike) | cctools-port and PureDarwin | +9/-10 | notice |
| darwin/tools/cctools/include/foreign/mach/riscv/thread_state.h | osfmk/mach/arm/thread_state.h (copy, 77% alike) | cctools-port and PureDarwin | +8/-11 | notice |
| darwin/tools/cctools/include/foreign/mach/rpc.h | osfmk/mach/rpc.h (copy, 75% alike) | cctools-port and PureDarwin | +34/-34 | notice |
| darwin/tools/cctools/include/foreign/mach/sdt.h | osfmk/mach/sdt.h (copy, 94% alike) | cctools-port and PureDarwin | +2/-2 | notice |
| darwin/tools/cctools/include/foreign/mach/shared_memory_server.h | osfmk/mach/shared_memory_server.h (copy, 67% alike) | cctools-port and PureDarwin | +42/-48 | notice |
| darwin/tools/cctools/include/foreign/mach/std_types.h | osfmk/mach/std_types.h (copy, 80% alike) | cctools-port and PureDarwin | +13/-16 | notice |
| darwin/tools/cctools/include/foreign/mach/sync_policy.h | osfmk/mach/sync_policy.h (copy, 67% alike) | cctools-port and PureDarwin | +12/-24 | notice |
| darwin/tools/cctools/include/foreign/mach/task_ledger.h | osfmk/mach/task_ledger.h (copy, 82% alike) | cctools-port and PureDarwin | +9/-9 | notice |
| darwin/tools/cctools/include/foreign/mach/task_special_ports.h | osfmk/mach/task_special_ports.h (copy, 61% alike) | cctools-port and PureDarwin | +56/-53 | notice |
| darwin/tools/cctools/include/foreign/mach/thread_policy.h | osfmk/mach/thread_policy.h (copy, 66% alike) | cctools-port and PureDarwin | +76/-129 | notice |
| darwin/tools/cctools/include/foreign/mach/thread_special_ports.h | osfmk/mach/thread_special_ports.h (copy, 75% alike) | cctools-port and PureDarwin | +19/-23 | notice |
| darwin/tools/cctools/include/foreign/mach/thread_status.h | osfmk/mach/thread_status.h (copy, 78% alike) | cctools-port and PureDarwin | +18/-24 | notice |
| darwin/tools/cctools/include/foreign/mach/thread_switch.h | osfmk/mach/thread_switch.h (copy, 75% alike) | cctools-port and PureDarwin | +17/-24 | notice |
| darwin/tools/cctools/include/foreign/mach/time_value.h | osfmk/mach/time_value.h (copy, 71% alike) | cctools-port and PureDarwin | +28/-28 | notice |
| darwin/tools/cctools/include/foreign/mach/vm_attributes.h | osfmk/mach/vm_attributes.h (copy, 73% alike) | cctools-port and PureDarwin | +27/-27 | notice |
| darwin/tools/cctools/include/foreign/mach/vm_task.h | libsyscall/mach/mach/vm_task.h (copy, 82% alike) | cctools-port and PureDarwin | +6/-6 | notice |
| darwin/tools/cctools/include/foreign/mach_debug/hash_info.h | osfmk/mach_debug/hash_info.h (copy, 81% alike) | cctools-port and PureDarwin | +13/-15 | notice |
| darwin/tools/cctools/include/foreign/mach_debug/mach_debug.h | osfmk/mach_debug/mach_debug.h (copy, 88% alike) | cctools-port and PureDarwin | +7/-5 | notice |
| darwin/tools/cctools/include/foreign/mach_debug/page_info.h | osfmk/mach_debug/page_info.h (copy, 78% alike) | cctools-port and PureDarwin | +14/-14 | notice |
| darwin/tools/cctools/include/foreign/mach_debug/vm_info.h | osfmk/mach_debug/vm_info.h (copy, 67% alike) | cctools-port and PureDarwin | +49/-49 | notice |
| darwin/tools/cctools/include/foreign/machine/_limits.h | bsd/machine/_limits.h (copy, 82% alike) | cctools-port and PureDarwin | +8/-6 | notice |
| darwin/tools/cctools/include/foreign/machine/_structs.h | bsd/sys/_structs.h (copy, 71% alike) | cctools-port and PureDarwin | +10/-8 | notice |
| darwin/tools/cctools/include/foreign/machine/_types.h | bsd/machine/_types.h (copy, 75% alike) | cctools-port and PureDarwin | +14/-7 | notice |
| darwin/tools/cctools/include/foreign/machine/byte_order.h | bsd/machine/byte_order.h (copy, 81% alike) | cctools-port and PureDarwin | +7/-7 | notice |
| darwin/tools/cctools/include/foreign/machine/endian.h | bsd/machine/endian.h (copy, 77% alike) | cctools-port and PureDarwin | +16/-6 | notice |
| darwin/tools/cctools/include/foreign/machine/fasttrap_isa.h | bsd/machine/fasttrap_isa.h (copy, 87% alike) | cctools-port and PureDarwin | +4/-6 | notice |
| darwin/tools/cctools/include/foreign/machine/param.h | bsd/machine/param.h (copy, 77% alike) | cctools-port and PureDarwin | +12/-8 | notice |
| darwin/tools/cctools/include/foreign/machine/profile.h | bsd/machine/profile.h (copy, 88% alike) | cctools-port and PureDarwin | +4/-6 | notice |
| darwin/tools/cctools/include/foreign/machine/setjmp.h | osfmk/machine/setjmp.h (copy, 64% alike) | cctools-port and PureDarwin | +12/-15 | notice |
| darwin/tools/cctools/include/foreign/machine/signal.h | bsd/machine/signal.h (copy, 80% alike) | cctools-port and PureDarwin | +10/-6 | notice |
| darwin/tools/cctools/include/foreign/machine/vmparam.h | bsd/machine/vmparam.h (copy, 87% alike) | cctools-port and PureDarwin | +4/-6 | notice |
| darwin/tools/cctools/include/foreign/riscv/_endian.h | bsd/arm/_endian.h (copy, 95% alike) | cctools-port and PureDarwin | +5/-5 | notice |
| darwin/tools/cctools/include/foreign/riscv/_limits.h | bsd/arm/_limits.h (copy, 62% alike) | cctools-port and PureDarwin | +5/-5 | notice |
| darwin/tools/cctools/include/foreign/riscv/_types.h | bsd/arm/_types.h (copy, 81% alike) | cctools-port and PureDarwin | +13/-34 | notice |
| darwin/tools/cctools/include/foreign/riscv/endian.h | bsd/arm/endian.h (copy, 86% alike) | cctools-port and PureDarwin | +7/-11 | permissive needs no change notice |
| darwin/tools/cctools/include/foreign/riscv/limits.h | bsd/arm/limits.h (copy, 89% alike) | cctools-port and PureDarwin | +10/-20 | permissive needs no change notice |
| darwin/tools/cctools/include/foreign/riscv/param.h | bsd/arm/param.h (copy, 83% alike) | cctools-port and PureDarwin | +15/-33 | permissive needs no change notice |
| darwin/tools/cctools/include/foreign/riscv/signal.h | bsd/arm/signal.h (copy, 77% alike) | cctools-port and PureDarwin | +5/-5 | notice |

## xnu-7195.121.3 (https://github.com/apple-oss-distributions/xnu)

| File | Release file | Changed by | +/- | Notice |
|---|---|---|---|---|
| darwin/src/Kernel/xnu/osfmk/mach/riscv/processor_info.h | osfmk/mach/arm/processor_info.h (copy, 94% alike) | PureDarwin (last 2026-09-28) | +6/-3 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/rpc.h | osfmk/mach/arm/rpc.h (copy, 85% alike) | PureDarwin (last 2026-09-28) | +3/-7 | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/traps.h | osfmk/mach/arm/traps.h (copy, 82% alike) | PureDarwin (last 2026-09-28) | +12/-2 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/commpage/commpage.c | osfmk/arm/commpage/commpage.c (copy, 64% alike) | PureDarwin (last 2026-09-28) | +243/-280 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/machine_cpu.h | osfmk/arm/machine_cpu.h (copy, 87% alike) | PureDarwin (last 2026-09-28) | +8/-8 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/machine_machdep.h | osfmk/arm64/machine_machdep.h (copy, 72% alike) | PureDarwin (last 2026-09-28) | +9/-13 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/machine_task.c | osfmk/arm/machine_task.c (copy, 66% alike) | PureDarwin (last 2026-09-28) | +36/-74 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/misc_protos.h | osfmk/arm/misc_protos.h (copy, 63% alike) | PureDarwin (last 2026-09-28) | +11/-64 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/pmap_public.h | osfmk/arm/pmap_public.h (copy, 85% alike) | PureDarwin (last 2026-09-28) | +4/-9 | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/rtclock.c | osfmk/arm/rtclock.c (copy, 79% alike) | PureDarwin (last 2026-09-28) | +25/-163 | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/arm64/H8.h | pexpert/pexpert/arm64/H8.h (copy, 95% alike) | PureDarwin (last 2026-11-10) | +5/-0 | notice |
| darwin/src/Libraries/XPC/launchd/mach_exc.defs | osfmk/mach/mach_exc.defs (copy, 80% alike) | PureDarwin (last 2026-07-22) | +18/-32 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/arm/_structs.h | osfmk/mach/arm/_structs.h (copy, 97% alike) | PureDarwin (last 2026-07-16) | +19/-20 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/arm/exception.h | osfmk/mach/arm/exception.h (copy, 97% alike) | PureDarwin (last 2026-07-16) | +0/-5 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/arm/sdt_isa.h | osfmk/mach/arm/sdt_isa.h (copy, 96% alike) | PureDarwin (last 2026-07-16) | +0/-37 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/arm/vm_types.h | osfmk/mach/arm/vm_types.h (copy, 99% alike) | PureDarwin (last 2026-07-16) | +0/-4 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/arm64/asm.h | osfmk/arm64/asm.h (copy, 99% alike) | PureDarwin (last 2026-07-16) | +0/-5 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/exception_types.h | osfmk/mach/exception_types.h (copy, 99% alike) | PureDarwin (last 2026-07-16) | +0/-3 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/host_info.h | osfmk/mach/host_info.h (copy, 89% alike) | PureDarwin (last 2026-07-16) | +1/-65 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/host_priv.defs | osfmk/mach/host_priv.defs (copy, 99% alike) | PureDarwin (last 2026-07-16) | +0/-8 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/i386/fp_reg.h | osfmk/mach/i386/fp_reg.h (copy, 75% alike) | PureDarwin (last 2026-07-16) | +0/-79 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/i386/sdt_isa.h | osfmk/mach/i386/sdt_isa.h (copy, 96% alike) | PureDarwin (last 2026-07-16) | +0/-37 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/i386/thread_status.h | osfmk/mach/i386/thread_status.h (copy, 84% alike) | PureDarwin (last 2026-07-16) | +0/-141 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/i386/vm_param.h | osfmk/mach/i386/vm_param.h (copy, 64% alike) | PureDarwin (last 2026-07-16) | +0/-180 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/i386/vm_types.h | osfmk/mach/i386/vm_types.h (copy, 95% alike) | PureDarwin (last 2026-08-30) | +4/-10 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/mach.h | libsyscall/mach/mach/mach.h (copy, 96% alike) | PureDarwin (last 2026-08-30) | +19/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/mach_host.defs | osfmk/mach/mach_host.defs (copy, 94% alike) | PureDarwin (last 2026-07-16) | +6/-54 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/mach_interface.h | libsyscall/mach/mach/mach_interface.h (copy, 87% alike) | PureDarwin (last 2026-08-30) | +16/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/mach_time.h | osfmk/mach/mach_time.h (copy, 91% alike) | PureDarwin (last 2026-07-16) | +0/-15 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/mach_types.defs | osfmk/mach/mach_types.defs (copy, 97% alike) | PureDarwin (last 2026-07-16) | +0/-45 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/mach_types.h | osfmk/mach/mach_types.h (copy, 86% alike) | PureDarwin (last 2026-08-30) | +4/-91 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/mach_voucher_types.h | osfmk/mach/mach_voucher_types.h (copy, 97% alike) | PureDarwin (last 2026-07-16) | +0/-16 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine.h | osfmk/mach/machine.h (copy, 93% alike) | PureDarwin (last 2026-09-28) | +6/-51 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/machine/thread_state.h | osfmk/mach/machine/thread_state.h (copy, 98% alike) | PureDarwin (last 2026-09-28) | +2/-0 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/message.h | osfmk/mach/message.h (copy, 86% alike) | PureDarwin (last 2026-07-16) | +0/-287 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/port.h | osfmk/mach/port.h (copy, 93% alike) | PureDarwin (last 2026-07-16) | +0/-68 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/processor_info.h | osfmk/mach/processor_info.h (copy, 95% alike) | PureDarwin (last 2026-07-16) | +0/-16 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/asm.h | osfmk/arm64/asm.h (copy, 62% alike) | PureDarwin (last 2026-09-28) | +26/-102 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/processor_info.h | osfmk/mach/arm/processor_info.h (copy, 94% alike) | PureDarwin (last 2026-09-28) | +6/-3 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/rpc.h | osfmk/mach/arm/rpc.h (copy, 85% alike) | PureDarwin (last 2026-09-28) | +3/-7 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/traps.h | osfmk/mach/arm/traps.h (copy, 82% alike) | PureDarwin (last 2026-09-28) | +12/-2 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/shared_region.h | osfmk/mach/shared_region.h (copy, 92% alike) | PureDarwin (last 2026-09-28) | +15/-19 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/sync_policy.h | osfmk/mach/sync_policy.h (copy, 89% alike) | PureDarwin (last 2026-07-16) | +0/-12 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/task_info.h | osfmk/mach/task_info.h (copy, 95% alike) | PureDarwin (last 2026-07-16) | +1/-52 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/task_special_ports.h | osfmk/mach/task_special_ports.h (copy, 99% alike) | PureDarwin (last 2026-07-16) | +0/-3 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/thread_act.defs | osfmk/mach/thread_act.defs (copy, 99% alike) | PureDarwin (last 2026-07-16) | +0/-4 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/thread_info.h | osfmk/mach/thread_info.h (copy, 96% alike) | PureDarwin (last 2026-07-16) | +0/-18 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/vm_prot.h | osfmk/mach/vm_prot.h (copy, 95% alike) | PureDarwin (last 2026-07-16) | +0/-15 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/vm_region.h | osfmk/mach/vm_region.h (copy, 97% alike) | PureDarwin (last 2026-07-16) | +0/-25 | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/vm_statistics.h | osfmk/mach/vm_statistics.h (copy, 86% alike) | PureDarwin (last 2026-07-16) | +2/-189 | notice |
| darwin/src/Libraries/objc4/macosx.internal/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/os/tsd.h | libsyscall/os/tsd.h (copy, 93% alike) | PureDarwin (last 2026-09-28) | +30/-0 | notice |
| darwin/tools/cctools/include/foreign/i386/_limits.h | bsd/i386/_limits.h (copy, 74% alike) | cctools-port and PureDarwin | +7/-7 | notice |
| darwin/tools/cctools/include/foreign/i386/_param.h | bsd/i386/_param.h (copy, 85% alike) | cctools-port and PureDarwin | +7/-7 | notice |
| darwin/tools/cctools/include/foreign/i386/_types.h | bsd/i386/_types.h (copy, 62% alike) | cctools-port and PureDarwin | +66/-37 | notice |
| darwin/tools/cctools/include/foreign/i386/endian.h | bsd/i386/endian.h (copy, 83% alike) | cctools-port and PureDarwin | +21/-14 | notice |
| darwin/tools/cctools/include/foreign/i386/limits.h | bsd/i386/limits.h (copy, 70% alike) | cctools-port and PureDarwin | +31/-35 | permissive needs no change notice |
| darwin/tools/cctools/include/foreign/i386/param.h | bsd/i386/param.h (copy, 73% alike) | cctools-port and PureDarwin | +45/-47 | notice |
| darwin/tools/cctools/include/foreign/i386/profile.h | bsd/i386/profile.h (copy, 74% alike) | cctools-port and PureDarwin | +4/-21 | notice |
| darwin/tools/cctools/include/foreign/i386/signal.h | bsd/i386/signal.h (copy, 65% alike) | cctools-port and PureDarwin | +29/-8 | notice |
| darwin/tools/cctools/include/foreign/i386/user_ldt.h | osfmk/i386/user_ldt.h (copy, 73% alike) | cctools-port and PureDarwin | +15/-33 | notice |
| darwin/tools/cctools/include/foreign/i386/vmparam.h | bsd/i386/vmparam.h (copy, 66% alike) | cctools-port and PureDarwin | +22/-20 | notice |
| darwin/tools/cctools/include/foreign/libkern/OSDebug.h | libkern/libkern/OSDebug.h (copy, 80% alike) | cctools-port and PureDarwin | +12/-15 | notice |
| darwin/tools/cctools/include/foreign/libkern/OSReturn.h | libkern/libkern/OSReturn.h (copy, 94% alike) | cctools-port and PureDarwin | +10/-16 | notice |
| darwin/tools/cctools/include/foreign/libkern/arm/_OSByteOrder.h | libkern/libkern/_OSByteOrder.h (copy, 65% alike) | cctools-port and PureDarwin | +18/-60 | notice |
| darwin/tools/cctools/include/foreign/libkern/i386/OSByteOrder.h | libkern/libkern/i386/OSByteOrder.h (copy, 65% alike) | cctools-port and PureDarwin | +41/-38 | notice |
| darwin/tools/cctools/include/foreign/libkern/i386/_OSByteOrder.h | libkern/libkern/i386/_OSByteOrder.h (copy, 74% alike) | cctools-port and PureDarwin | +27/-27 | notice |
| darwin/tools/cctools/include/foreign/libkern/machine/OSByteOrder.h | libkern/libkern/machine/OSByteOrder.h (copy, 70% alike) | cctools-port and PureDarwin | +43/-43 | notice |
| darwin/tools/cctools/include/foreign/mach/error.h | osfmk/mach/error.h (copy, 68% alike) | cctools-port and PureDarwin | +37/-37 | notice |
| darwin/tools/cctools/include/foreign/mach/exc.defs | osfmk/mach/exc.defs (copy, 80% alike) | cctools-port and PureDarwin | +18/-32 | notice |
| darwin/tools/cctools/include/foreign/mach/host_priv.defs | osfmk/mach/host_priv.defs (copy, 92% alike) | cctools-port and PureDarwin | +35/-24 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/asm.h | osfmk/i386/asm.h (copy, 76% alike) | cctools-port and PureDarwin | +46/-118 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/boolean.h | osfmk/mach/i386/boolean.h (copy, 81% alike) | cctools-port and PureDarwin | +14/-14 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/exception.h | osfmk/mach/i386/exception.h (copy, 67% alike) | cctools-port and PureDarwin | +44/-44 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/kern_return.h | osfmk/mach/i386/kern_return.h (copy, 80% alike) | cctools-port and PureDarwin | +15/-15 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/machine_types.defs | osfmk/mach/machine/machine_types.defs (copy, 94% alike) | cctools-port and PureDarwin | +6/-9 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/processor_info.h | osfmk/mach/i386/processor_info.h (copy, 84% alike) | cctools-port and PureDarwin | +6/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/rpc.h | osfmk/mach/i386/rpc.h (copy, 79% alike) | cctools-port and PureDarwin | +8/-7 | notice |
| darwin/tools/cctools/include/foreign/mach/i386/vm_types.h | osfmk/mach/i386/vm_types.h (copy, 73% alike) | cctools-port and PureDarwin | +33/-45 | notice |
| darwin/tools/cctools/include/foreign/mach/ledger.defs | osfmk/mach/ledger.defs (copy, 74% alike) | cctools-port and PureDarwin | +32/-1 | notice |
| darwin/tools/cctools/include/foreign/mach/lock_set.defs | osfmk/mach/lock_set.defs (copy, 98% alike) | cctools-port and PureDarwin | +0/-4 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_exc.defs | osfmk/mach/mach_exc.defs (copy, 80% alike) | cctools-port and PureDarwin | +18/-32 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_host.defs | osfmk/mach/mach_host.defs (copy, 70% alike) | cctools-port and PureDarwin | +26/-174 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_init.h | libsyscall/mach/mach/mach_init.h (copy, 69% alike) | cctools-port and PureDarwin | +50/-29 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_interface.h | libsyscall/mach/mach/mach_interface.h (copy, 90% alike) | cctools-port and PureDarwin | +6/-5 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_param.h | osfmk/mach/mach_param.h (copy, 77% alike) | cctools-port and PureDarwin | +13/-22 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_port.defs | osfmk/mach/mach_port.defs (copy, 78% alike) | cctools-port and PureDarwin | +20/-243 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_types.defs | osfmk/mach/mach_types.defs (copy, 73% alike) | cctools-port and PureDarwin | +26/-310 | notice |
| darwin/tools/cctools/include/foreign/mach/mach_vm.defs | osfmk/mach/mach_vm.defs (copy, 85% alike) | cctools-port and PureDarwin | +40/-129 | notice |
| darwin/tools/cctools/include/foreign/mach/machine/thread_state.h | osfmk/mach/machine/thread_state.h (copy, 81% alike) | cctools-port and PureDarwin | +10/-6 | notice |
| darwin/tools/cctools/include/foreign/mach/notify.defs | osfmk/mach/notify.defs (copy, 95% alike) | cctools-port and PureDarwin | +1/-11 | notice |
| darwin/tools/cctools/include/foreign/mach/ppc/boolean.h | osfmk/mach/i386/boolean.h (copy, 76% alike) | cctools-port and PureDarwin | +18/-18 | notice |
| darwin/tools/cctools/include/foreign/mach/ppc/kern_return.h | osfmk/mach/i386/kern_return.h (copy, 78% alike) | cctools-port and PureDarwin | +16/-16 | notice |
| darwin/tools/cctools/include/foreign/mach/ppc/machine_types.defs | osfmk/mach/machine/machine_types.defs (copy, 90% alike) | cctools-port and PureDarwin | +11/-15 | notice |
| darwin/tools/cctools/include/foreign/mach/ppc/rpc.h | osfmk/mach/i386/rpc.h (copy, 80% alike) | cctools-port and PureDarwin | +7/-7 | notice |
| darwin/tools/cctools/include/foreign/mach/ppc/vm_types.h | osfmk/mach/i386/vm_types.h (copy, 64% alike) | cctools-port and PureDarwin | +53/-56 | notice |
| darwin/tools/cctools/include/foreign/mach/processor.defs | osfmk/mach/processor.defs (copy, 95% alike) | cctools-port and PureDarwin | +1/-12 | notice |
| darwin/tools/cctools/include/foreign/mach/processor_info.h | osfmk/mach/processor_info.h (copy, 61% alike) | cctools-port and PureDarwin | +55/-71 | notice |
| darwin/tools/cctools/include/foreign/mach/riscv/processor_info.h | osfmk/mach/arm/processor_info.h (copy, 94% alike) | cctools-port and PureDarwin | +6/-3 | notice |
| darwin/tools/cctools/include/foreign/mach/riscv/rpc.h | osfmk/mach/arm/rpc.h (copy, 85% alike) | cctools-port and PureDarwin | +3/-7 | notice |
| darwin/tools/cctools/include/foreign/mach/sync.h | libsyscall/mach/mach/sync.h (copy, 89% alike) | cctools-port and PureDarwin | +4/-4 | notice |
| darwin/tools/cctools/include/foreign/mach/task.defs | osfmk/mach/task.defs (copy, 77% alike) | cctools-port and PureDarwin | +20/-211 | notice |
| darwin/tools/cctools/include/foreign/mach/thread_act.defs | osfmk/mach/thread_act.defs (copy, 83% alike) | cctools-port and PureDarwin | +24/-102 | notice |
| darwin/tools/cctools/include/foreign/mach/vm_behavior.h | osfmk/mach/vm_behavior.h (copy, 74% alike) | cctools-port and PureDarwin | +20/-21 | notice |
| darwin/tools/cctools/include/foreign/mach/vm_inherit.h | osfmk/mach/vm_inherit.h (copy, 77% alike) | cctools-port and PureDarwin | +23/-19 | notice |
| darwin/tools/cctools/include/foreign/mach/vm_map.defs | osfmk/mach/vm_map.defs (copy, 93% alike) | cctools-port and PureDarwin | +8/-59 | notice |
| darwin/tools/cctools/include/foreign/mach/vm_prot.h | osfmk/mach/vm_prot.h (copy, 74% alike) | cctools-port and PureDarwin | +28/-54 | notice |
| darwin/tools/cctools/include/foreign/mach/vm_sync.h | osfmk/mach/vm_sync.h (copy, 80% alike) | cctools-port and PureDarwin | +16/-16 | notice |
| darwin/tools/cctools/include/foreign/mach_debug/ipc_info.h | osfmk/mach_debug/ipc_info.h (copy, 70% alike) | cctools-port and PureDarwin | +29/-42 | notice |
| darwin/tools/cctools/include/foreign/mach_debug/mach_debug_types.defs | osfmk/mach_debug/mach_debug_types.defs (copy, 88% alike) | cctools-port and PureDarwin | +5/-21 | notice |
| darwin/tools/cctools/include/foreign/mach_debug/mach_debug_types.h | osfmk/mach_debug/mach_debug_types.h (copy, 72% alike) | cctools-port and PureDarwin | +13/-34 | notice |
| darwin/tools/cctools/include/foreign/machine/_param.h | bsd/machine/_param.h (copy, 72% alike) | cctools-port and PureDarwin | +12/-8 | notice |
| darwin/tools/cctools/include/foreign/ppc/_limits.h | bsd/i386/_limits.h (copy, 74% alike) | cctools-port and PureDarwin | +7/-7 | notice |
| darwin/tools/cctools/include/foreign/ppc/_param.h | bsd/i386/_param.h (copy, 76% alike) | cctools-port and PureDarwin | +11/-11 | notice |
| darwin/tools/cctools/include/foreign/ppc/_types.h | bsd/i386/_types.h (copy, 60% alike) | cctools-port and PureDarwin | +56/-45 | notice |
| darwin/tools/cctools/include/foreign/ppc/endian.h | bsd/i386/endian.h (copy, 66% alike) | cctools-port and PureDarwin | +55/-25 | notice |
| darwin/tools/cctools/include/foreign/ppc/limits.h | bsd/arm/limits.h (copy, 66% alike) | cctools-port and PureDarwin | +35/-38 | permissive needs no change notice |
| darwin/tools/cctools/include/foreign/ppc/profile.h | bsd/i386/profile.h (copy, 67% alike) | cctools-port and PureDarwin | +7/-24 | notice |
| darwin/tools/cctools/include/foreign/ppc/vmparam.h | bsd/i386/vmparam.h (copy, 62% alike) | cctools-port and PureDarwin | +27/-21 | notice |

## Files with an Apple notice that no compared release has

PureDarwin's ports (riscv64, the DriverKit headers its iig-lite generated, board configs, its CMake makedefs) carry a notice, as do two near-copies of compared releases (libc's include/_stdlib.h, 99% like Libc-1752.120.2's; cctools-port's machine/types.h, 78% like xnu's). The rest are copies from Apple releases this list did not compare (CarbonHeaders, Libm, hfs, libclosure, and older xnu and cctools releases for cctools-port's foreign headers); they say "May have been modified".

| File | Notice |
|---|---|
| darwin/src/Frameworks/CoreFoundation/Base.subproj/CFLocking.h | may have been modified |
| darwin/src/Kernel/Extensions/AppleFileSystemDriver/shim/hfs/hfs_format.h | may have been modified |
| darwin/src/Kernel/Extensions/AppleFileSystemDriver/shim/hfs/hfs_unistr.h | may have been modified |
| darwin/src/Kernel/Extensions/Ext4FileSystemDriver/Ext4FileSystemDriver.cpp | notice |
| darwin/src/Kernel/Extensions/IOPCIFamily/Info.plist | no comment syntax for .plist |
| darwin/src/Kernel/xnu/bsd/dev/riscv/kern_machdep.c | notice |
| darwin/src/Kernel/xnu/bsd/dev/riscv/systemcalls.c | notice |
| darwin/src/Kernel/xnu/bsd/dev/riscv/unix_signal.c | notice |
| darwin/src/Kernel/xnu/cmake/MakeInc.cmd.in | notice |
| darwin/src/Kernel/xnu/cmake/MakeInc.def.in | notice |
| darwin/src/Kernel/xnu/cmake/make_symbol_aliasing.sh.in | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm.bcm2835 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.s8000 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.sc8280xp | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.sun50i | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.t8010 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.t8103 | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.virt | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.virt4k | notice |
| darwin/src/Kernel/xnu/config/MASTER.arm64.virtamp | notice |
| darwin/src/Kernel/xnu/config/MASTER.riscv64 | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOBufferMemoryDescriptor.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IODMACommand.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IODataQueueDispatchSource.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IODispatchQueue.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IODispatchSource.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOEventLink.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOExtensiblePaniclog.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOInterruptDispatchSource.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOMemoryDescriptor.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOMemoryMap.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOService.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOServiceNotificationDispatchSource.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOServiceStateNotificationDispatchSource.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOUserClient.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOUserServer.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/IOWorkGroup.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/OSAction.h | notice |
| darwin/src/Kernel/xnu/iokit/DriverKit/OSObject.h | notice |
| darwin/src/Kernel/xnu/osfmk/arm/caches_macros.s | notice |
| darwin/src/Kernel/xnu/osfmk/libsa/riscv/types.h | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/_structs.h | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/exception.h | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/thread_status.h | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/vm_param.h | notice |
| darwin/src/Kernel/xnu/osfmk/mach/riscv/vm_types.h | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/asm.h | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/bsd_riscv.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/caches.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/copyio.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/copyio_asm.s | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/cpu.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/data.s | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/genassym.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/locks_riscv.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/lowglobals.h | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/machine_routines.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/pcb.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/pmap.h | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/riscv_hypercall.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/riscv_init.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/riscv_timer.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/rtclock.h | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/sleh.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/status.c | notice |
| darwin/src/Kernel/xnu/osfmk/riscv/thread.h | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/riscv/board_config.h | notice |
| darwin/src/Kernel/xnu/pexpert/pexpert/riscv/protos.h | notice |
| darwin/src/Libraries/AvailabilityVersions/include/ConditionalMacros.h | may have been modified |
| darwin/src/Libraries/AvailabilityVersions/include/TargetConditionals.h | may have been modified |
| darwin/src/Libraries/AvailabilityVersions/include/mach-o/dyld_version_defines.h | may have been modified |
| darwin/src/Libraries/AvailabilityVersions/include/os/availability.h | may have been modified |
| darwin/src/Libraries/CrashReporterClient/include/CrashReporterClient.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/ConditionalMacros.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/MacTypes.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/_stdlib.h | notice |
| darwin/src/Libraries/libSystem/libc/include/complex.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/mach/arm/thread_status.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/mach/arm/vm_param.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/mach/mach_traps.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/mach/memory_object_types.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/_structs.h | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/exception.h | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/thread_status.h | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/vm_param.h | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/riscv/vm_types.h | notice |
| darwin/src/Libraries/libSystem/libc/include/mach/vm_param.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/mach/vm_types.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/math.h | may have been modified |
| darwin/src/Libraries/libSystem/libc/include/sysdir.h | may have been modified |
| darwin/src/Libraries/libSystem/libplatform/src/setjmp/riscv/setjmp.s | notice |
| darwin/src/Libraries/libSystem/libplatform/src/ucontext/riscv/_ctx_start.s | notice |
| darwin/src/Libraries/libSystem/libplatform/src/ucontext/riscv/_setcontext.s | notice |
| darwin/src/Libraries/libSystem/libplatform/src/ucontext/riscv/asm_help.h | notice |
| darwin/src/Libraries/libSystem/libplatform/src/ucontext/riscv/getcontext.s | notice |
| darwin/src/Libraries/libSystem/libplatform/src/ucontext/riscv/mcontext.c | notice |
| darwin/src/Libraries/objc4/macosx.internal/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/Block_private.h | may have been modified |
| darwin/src/Libraries/objc4/macosx.internal/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders/CrashReporterClient.h | may have been modified |
| darwin/src/Libraries/objc4/runtime/Messengers.subproj/objc-msg-riscv64.s | notice |
| darwin/src/Userspace/system_cmds/mkfile.c | may have been modified |
| darwin/tools/cctools/include/foreign/i386/_structs.h | may have been modified |
| darwin/tools/cctools/include/foreign/i386/types.h | may have been modified |
| darwin/tools/cctools/include/foreign/libkern/OSAtomic.h | may have been modified |
| darwin/tools/cctools/include/foreign/libkern/OSKextLib.h | may have been modified |
| darwin/tools/cctools/include/foreign/libkern/OSTypes.h | may have been modified |
| darwin/tools/cctools/include/foreign/libkern/arm/OSByteOrder.h | may have been modified |
| darwin/tools/cctools/include/foreign/libkern/ppc/OSByteOrder.h | may have been modified |
| darwin/tools/cctools/include/foreign/libunwind.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/exception_types.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/host_info.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/host_special_ports.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/i386/vm_param.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/kern_return.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/mach.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/mach_time.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/mach_traps.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/mach_types.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/machine/machine_types.defs | may have been modified |
| darwin/tools/cctools/include/foreign/mach/memory_object_types.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/message.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/port.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/ppc/asm.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/ppc/exception.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/ppc/vm_param.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/riscv/_structs.h | notice |
| darwin/tools/cctools/include/foreign/mach/riscv/exception.h | notice |
| darwin/tools/cctools/include/foreign/mach/riscv/thread_status.h | notice |
| darwin/tools/cctools/include/foreign/mach/riscv/vm_param.h | notice |
| darwin/tools/cctools/include/foreign/mach/riscv/vm_types.h | notice |
| darwin/tools/cctools/include/foreign/mach/semaphore.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/shared_region.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/task.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/task_info.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/task_policy.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/thread_act.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/thread_info.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/vm_param.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/vm_purgable.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/vm_region.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/vm_statistics.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach/vm_types.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach_debug/lockgroup_info.h | may have been modified |
| darwin/tools/cctools/include/foreign/mach_debug/zone_info.h | may have been modified |
| darwin/tools/cctools/include/foreign/machine/types.h | notice |
| darwin/tools/cctools/include/foreign/ppc/_structs.h | may have been modified |
| darwin/tools/cctools/include/foreign/ppc/fasttrap_isa.h | may have been modified |
| darwin/tools/cctools/include/foreign/ppc/param.h | may have been modified |
| darwin/tools/cctools/include/foreign/ppc/setjmp.h | may have been modified |
| darwin/tools/cctools/include/foreign/ppc/signal.h | may have been modified |
| darwin/tools/cctools/include/foreign/ppc/types.h | may have been modified |
| darwin/tools/cctools/include/foreign/riscv/_param.h | notice |
| darwin/tools/cctools/include/foreign/riscv/types.h | may have been modified |
| darwin/tools/cctools/include/foreign/sys/_types.h | may have been modified |

## xnu-loader

| File | Release file | Changed by | Notice |
|---|---|---|---|
| loader/include/boot.h | xnu pexpert/i386/boot.h | xnu-loader (2026-03-30 to 2026-10-11); kido (2026-11-23, the APSL header) | notice |
