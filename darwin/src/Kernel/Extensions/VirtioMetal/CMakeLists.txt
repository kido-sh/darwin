# Copyright (c) 2026 The kido authors
# SPDX-License-Identifier: Apache-2.0

add_kext_bundle(VirtioMetal.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER sh.kido.driver.virtiometal BUNDLE_VERSION 1.0.0)
set_property(TARGET VirtioMetal.kext PROPERTY OUTPUT_NAME VirtioMetal)

target_sources(VirtioMetal.kext PRIVATE
    VirtioMetalDevice.cpp
    VirtioMetalUserClient.cpp
)

target_include_directories(VirtioMetal.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOVirtIOFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(VirtioMetal.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(VirtioMetal.kext PRIVATE __PRIVATE_SPI__)
set_property(TARGET VirtioMetal.kext PROPERTY CXX_STANDARD 14)
add_dependencies(VirtioMetal.kext IOVirtIOFamily.kext)
install(TARGETS VirtioMetal.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
