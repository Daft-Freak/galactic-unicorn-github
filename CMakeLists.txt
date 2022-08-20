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

add_executable(picow-stuff picow-stuff.cpp )

pico_set_program_name(picow-stuff "picow-stuff")
pico_set_program_version(picow-stuff "0.1")

pico_enable_stdio_uart(picow-stuff 1)
pico_enable_stdio_usb(picow-stuff 0)

# Add the standard library to the build
target_link_libraries(picow-stuff pico_stdlib)

pico_add_extra_outputs(picow-stuff)

