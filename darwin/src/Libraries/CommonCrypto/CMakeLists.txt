if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-DDEBUG)
endif()

include(host_commoncrypto_static.cmake)

add_darwin_static_library(commoncrypto_static)
target_sources(commoncrypto_static PRIVATE
    $<TARGET_OBJECTS:host_commoncrypto_static>
)

target_link_libraries(commoncrypto_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    pthread_common_headers
    xnu_headers
    corecrypto_static
)
