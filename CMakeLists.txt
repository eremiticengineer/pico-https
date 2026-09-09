cmake_minimum_required(VERSION 3.13)

set(LIBRARY pico_https)

# ---------------------------------------------------------
# Are we being built directly or as a submodule?
# ---------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PICO_HTTPS_STANDALONE TRUE)
else()
    set(PICO_HTTPS_STANDALONE FALSE)
endif()

# ---------------------------------------------------------
# Standalone project setup
# ---------------------------------------------------------
if(PICO_HTTPS_STANDALONE)

    set(PROJECT pico_https_example)

    set(PICO_BOARD pico2_w CACHE STRING "Board type")

    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

    project(${LIBRARY} C CXX ASM)

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

target_include_directories(${LIBRARY} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# When building the standalone example, its configuration
# files are the application-wide FreeRTOS/lwIP configuration.
if(PICO_HTTPS_STANDALONE)
    target_include_directories(${LIBRARY} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/examples/include
    )
endif()

target_link_libraries(${LIBRARY} PUBLIC
    pico_stdlib
    pico_mbedtls
    pico_cyw43_arch_lwip_sys_freertos
    FreeRTOS-Kernel-Heap4
)

# ---------------------------------------------------------
# Standalone example
# ---------------------------------------------------------
if(PICO_HTTPS_STANDALONE)

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
        ${LIBRARY}
    )

    pico_enable_stdio_usb(${PROJECT} 1)
    pico_enable_stdio_uart(${PROJECT} 1)

    pico_add_extra_outputs(${PROJECT})

endif()