add_darwin_executable(kido-dmesg-save MACOSX_VERSION_MIN 26.5)

get_target_property(_kido_dmesg_save_opts kido-dmesg-save COMPILE_OPTIONS)
if(_kido_dmesg_save_opts)
    list(REMOVE_ITEM _kido_dmesg_save_opts -nostdinc)
    set_target_properties(kido-dmesg-save PROPERTIES COMPILE_OPTIONS "${_kido_dmesg_save_opts}")
endif()

target_sources(kido-dmesg-save PRIVATE kido-dmesg-save.c)
target_compile_options(kido-dmesg-save PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_link_options(kido-dmesg-save PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
add_dependencies(kido-dmesg-save libSystem_B_stub dyld)

install(TARGETS kido-dmesg-save DESTINATION usr/libexec COMPONENT BaseSystem)
