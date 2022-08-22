# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picow-stuff C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(picow-stuff
    http_client.cpp
    picow-stuff.cpp
)

pico_set_program_name(picow-stuff "picow-stuff")
pico_set_program_version(picow-stuff "0.1")

pico_enable_stdio_uart(picow-stuff 1)
pico_enable_stdio_usb(picow-stuff 0)

target_include_directories(picow-stuff PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# mbedtls
set(ENABLE_TESTING OFF CACHE BOOL "")
set(ENABLE_PROGRAMS OFF CACHE BOOL "")
set(MBEDTLS_CONFIG_FILE ${CMAKE_CURRENT_LIST_DIR}/mbedtls_config.h)

add_subdirectory(mbedtls)

target_compile_definitions(mbedcrypto PUBLIC MBEDTLS_CONFIG_FILE="${MBEDTLS_CONFIG_FILE}")
target_compile_definitions(mbedx509 PUBLIC MBEDTLS_CONFIG_FILE="${MBEDTLS_CONFIG_FILE}")
target_compile_definitions(mbedtls PUBLIC MBEDTLS_CONFIG_FILE="${MBEDTLS_CONFIG_FILE}")

# lwip tls
add_library(lwip_tls_mbedtls INTERFACE)
target_sources(lwip_tls_mbedtls INTERFACE
    ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
    ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls_mem.c
)
target_link_libraries(lwip_tls_mbedtls INTERFACE mbedtls)

target_link_libraries(picow-stuff
    lwip_tls_mbedtls
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_http
    pico_stdlib
)

target_compile_definitions(picow-stuff PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASSWORD="${WIFI_PASSWORD}"
)

pico_add_extra_outputs(picow-stuff)

