cmake_minimum_required(VERSION 3.13)

set(PROJECT pico-post-https-test)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

set(PICO_MBEDTLS_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/include/mbedtls_config.h")

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(${PROJECT} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

pico_sdk_init()

message(STATUS "PICO_PLATFORM=${PICO_PLATFORM}")

set(FREERTOS_KERNEL_PATH
    ${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel
)

include(
    ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake
)

add_executable(${PROJECT}
  src/pico-post-https-test.cpp
  src/HttpsJsonClient/HttpsJsonClient.cpp
  src/WeatherPayload/WeatherPayload.cpp
)

target_include_directories(${PROJECT} PUBLIC
  include/
  src/
  src/HttpsJsonClient
  src/WeatherPayload
)

target_link_libraries(${PROJECT}
    pico_stdlib
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip
    pico_mbedtls
    FreeRTOS-Kernel-Heap4
)

# target_compile_definitions(${PROJECT} PRIVATE
#     MBEDTLS_NO_PLATFORM_ENTROPY
#     MBEDTLS_ENTROPY_HARDWARE_ALT
# )

pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 1)

pico_add_extra_outputs(${PROJECT})
