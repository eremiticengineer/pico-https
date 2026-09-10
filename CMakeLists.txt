# A dependency is PUBLIC if someone compiling the public header needs it.
# Otherwise it is PRIVATE.

cmake_minimum_required(VERSION 3.13)

set(LIBRARY pico_https)

# ---------------------------------------------------------
# Are we being built directly or as a submodule?
# ---------------------------------------------------------
# Building pico_https directly:
# pico_https
# └── CMakeLists.txt       ← top-level
#     ├── pico_https
#     └── example
#
# Building application that consumes pico_https:
# weather-station
# ├── CMakeLists.txt       ← top-level
# └── lib/
#     └── pico_https/
#         └── CMakeLists.txt
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(LIBRARY_STANDALONE TRUE)
else()
    # Do not make assumptions about the parent application's configuration
    set(LIBRARY_STANDALONE FALSE)
endif()

# ---------------------------------------------------------
# Standalone project setup
# ---------------------------------------------------------
if(LIBRARY_STANDALONE)

    set(PROJECT pico_https_example)

    if(DEVICE STREQUAL "pico_w")
        set(PICO_BOARD pico_w CACHE STRING "Board type" FORCE)
    elseif(DEVICE STREQUAL "pico2_w")
        set(PICO_BOARD pico2_w CACHE STRING "Board type" FORCE)
    else()
        message(FATAL_ERROR
            "Unknown DEVICE='${DEVICE}'. Use pico_w or pico2_w."
        )
    endif()

    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

    project(${PROJECT} C CXX ASM)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 23)

    pico_sdk_init()

    # -----------------------------------------------------
    # FreeRTOS
    # -----------------------------------------------------
    set(FREERTOS_KERNEL_PATH
        ${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel
    )

    include(
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake
    )

    message(STATUS "DEVICE=${DEVICE}")
    message(STATUS "PICO_BOARD=${PICO_BOARD}")
    message(STATUS "PICO_PLATFORM=${PICO_PLATFORM}")

endif()

# ---------------------------------------------------------
# Library
# ---------------------------------------------------------
add_library(${LIBRARY}
    src/HttpsClient.cpp
)

# PUBLIC because an application will write:
# #include "HttpsClient.hpp"
# Anything linking against pico_https needs src added to its include search path.
target_include_directories(${LIBRARY} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# If pico_https itself is the project being built,
# then its example application owns the FreeRTOS/lwIP configuration,
# so make that configuration visible while compiling the library.
# Otherwise this is a library inside another application.
# Do not assume where that application's FreeRTOS/lwIP configuration lives.
if(LIBRARY_STANDALONE)
        # These include paths are needed while compiling pico_https
        # in standalone mode, but they should not be propagated
        # to targets that consume pico_https.
        target_include_directories(${LIBRARY} PRIVATE
        # PRIVATE because only the compilation of this target needs these files.
        ${CMAKE_CURRENT_LIST_DIR}/examples/include
    )
endif()

target_link_libraries(${LIBRARY}
    PUBLIC
        # Because HttpsClient.hpp contains private members of type mbedtls_*
        # Dependencies when building in other application:
        # project that uses pico_https
        #       │
        #       └── pico_https
        #                │
        #                └── pico_mbedtls
        pico_mbedtls
    PRIVATE
        # Purely implementation in the library.
        # Consumers do not inherit these dependencies
        # as one of pico_https usage requirements.
        pico_stdlib
        pico_cyw43_arch_lwip_sys_freertos
        FreeRTOS-Kernel-Heap4
)

# ---------------------------------------------------------
# Standalone example
# ---------------------------------------------------------
if(LIBRARY_STANDALONE)

    add_executable(${PROJECT}
        examples/main.cpp
        examples/HttpsPayload/HttpsPayload.cpp
    )

    target_include_directories(${PROJECT} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/examples/include
        ${CMAKE_CURRENT_LIST_DIR}/examples/HttpsPayload
    )

    target_compile_definitions(${PROJECT} PRIVATE
        DEVICE_${DEVICE}
    )

    target_link_libraries(${PROJECT} PRIVATE
        pico_stdlib
        FreeRTOS-Kernel-Heap4
        ${LIBRARY}
    )

    pico_enable_stdio_usb(${PROJECT} 1)
    pico_enable_stdio_uart(${PROJECT} 1)

    pico_add_extra_outputs(${PROJECT})

endif()