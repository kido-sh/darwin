if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../xnu/pd_apple_builtins.h")
    add_compile_options(-include "${CMAKE_CURRENT_SOURCE_DIR}/../xnu/pd_apple_builtins.h")
endif()

if(PD_CORECRYPTO_DIR)
    include(${PD_CORECRYPTO_DIR}/pd/corecrypto-pd.cmake)
endif()

# The legacy extension sources use the MacOSX branch of the XNU KPI headers.
add_compile_definitions(XNU_PLATFORM_MacOSX=1)

add_subdirectory(PDArmPlatformExpert)
add_subdirectory(PDArmPCI)
add_subdirectory(PDEcamPCI)
add_subdirectory(IOPCIFamily)
add_subdirectory(IOStorageFamily)
add_subdirectory(IOVirtIOFamily)
add_subdirectory(IOVirtIONet)
add_subdirectory(IOVirtIOBlock)
add_subdirectory(IONetworkingFamily)
add_subdirectory(corecrypto)
add_subdirectory(ext4)
add_subdirectory(AppleFileSystemDriver)
add_subdirectory(Ext4FileSystemDriver)

add_custom_target(kexts)
add_dependencies(kexts
    PDArmPlatformExpert PDArmPCI PDEcamPCI IOPCIFamily ApplePIODMA IOStorageFamily
    IOVirtIOFamily.kext IOVirtIONet.kext IOVirtIOBlock.kext IONetworkingFamily.kext
    corecrypto.kext ext4.kext AppleFileSystemDriver.kext Ext4FileSystemDriver.kext
)

add_subdirectory(pthread)
add_dependencies(kexts pthread.kext)

# Permissive AppleMobileFileIntegrity stand-in: PureDarwin does not enforce
# Apple code signing, so this registers the amfi_t/TrustCache vtables that
# xnu 12377 requires a real AMFI kext to provide, with allow-all semantics.
add_subdirectory(amfi)
add_dependencies(kexts amfi.kext)

# MACF policy behind sandbox_init(3); loaded at boot as a security extension.
add_subdirectory(Sandbox)
add_dependencies(kexts Sandbox.kext)

# IOWatchdog service that watchdogd opens at boot
add_subdirectory(PDWatchdog)
add_dependencies(kexts PDWatchdog.kext)
add_subdirectory(PDDiskImages2)
add_dependencies(kexts PDDiskImages2.kext)
# FSKit's lifs user client fskitd opens at start
add_subdirectory(PDLifs)
add_dependencies(kexts PDLifs.kext)
add_subdirectory(PDKeyStore)
add_dependencies(kexts PDKeyStore.kext)
