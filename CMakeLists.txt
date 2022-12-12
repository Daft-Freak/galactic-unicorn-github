# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(pimoroni_pico_import.cmake)

project(galactic-unicorn-github C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# first three to avoid compile/link errors
include(libraries/hershey_fonts/hershey_fonts) # pico_graphics dependency
include(libraries/bitmap_fonts/bitmap_fonts) # pico_graphics dependency
include(libraries/pico_graphics/pico_graphics) # galactic_unicorn dependency
include(libraries/galactic_unicorn/galactic_unicorn)

# Add executable. Default name is the project name, version 0.1

add_executable(galactic-unicorn-github
    http_client.cpp
    galactic-unicorn-github.cpp
)

pico_set_program_name(galactic-unicorn-github "galactic-unicorn-github")
pico_set_program_version(galactic-unicorn-github "0.1")

pico_enable_stdio_uart(galactic-unicorn-github 0)
pico_enable_stdio_usb(galactic-unicorn-github 1)

target_include_directories(galactic-unicorn-github PRIVATE ${CMAKE_CURRENT_LIST_DIR})

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

# tinyjson
add_library(tinyjson INTERFACE)
target_sources(tinyjson INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/tiny-json/tiny-json.c
)
target_include_directories(tinyjson INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tiny-json)

target_link_libraries(galactic-unicorn-github
    lwip_tls_mbedtls
    pico_cyw43_arch_lwip_threadsafe_background
    pico_stdlib
    galactic_unicorn
    tinyjson
)

target_compile_definitions(galactic-unicorn-github PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASSWORD="${WIFI_PASSWORD}"
    GITHUB_TOKEN="${GITHUB_TOKEN}"
)

pico_add_extra_outputs(galactic-unicorn-github)

