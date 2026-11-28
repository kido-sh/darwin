# Left 0 by xnu's cdefs.h (no PLATFORM_* defined), which asm-renames 64-bit calls
# to $UNIX2003 symbols that do not exist. Userland only: the kernel's own cdefs.h
# KERNEL branch defines it as 0 and would collide.
add_compile_definitions(__DARWIN_ONLY_UNIX_CONFORMANCE=1)

add_subdirectory(diskdev_cmds)
add_subdirectory(doubleagentd)
add_subdirectory(ext4tool)
add_subdirectory(ext4util)
add_subdirectory(gpt)
add_subdirectory(mdnsd)
add_subdirectory(pkill)
add_subdirectory(killall)
add_subdirectory(netsetup)
add_subdirectory(kido-dmesg-save)
add_subdirectory(ping)
add_subdirectory(ps)
add_subdirectory(sh)
add_subdirectory(shell_cmds)
add_subdirectory(system_cmds)
add_subdirectory(text_cmds)
