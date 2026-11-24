add_kext_bundle(VirtioFS.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER sh.kido.filesystems.virtiofs BUNDLE_VERSION 1.0.0)
set_property(TARGET VirtioFS.kext PROPERTY OUTPUT_NAME VirtioFS)

target_sources(VirtioFS.kext PRIVATE
    VirtioFSDevice.cpp
    VirtioFSFilesystem.cpp
    fuse_proto.c
    virtiofs_vfsops.c
    virtiofs_vnops.c
)

target_include_directories(VirtioFS.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOVirtIOFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(VirtioFS.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(VirtioFS.kext PRIVATE __PRIVATE_SPI__)
set_property(TARGET VirtioFS.kext PROPERTY CXX_STANDARD 14)
add_dependencies(VirtioFS.kext IOVirtIOFamily.kext)
install(TARGETS VirtioFS.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
