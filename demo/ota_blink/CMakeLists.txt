#
# Demo: OTA Blink — Standalone App Build
#
# Builds the blink demo as a standalone binary for OTA deployment.
# Uses the same linker script and toolchain as the production app.
#
# Usage (inside nrfutil toolchain-manager launch):
#   rm -rf build_demo && mkdir build_demo && cd build_demo
#   cmake ../rak-sid/demo/ota_blink
#   make
#
# Output: app.hex / app.bin (flash at 0x90000 or deploy via OTA)
#

cmake_minimum_required(VERSION 3.20.0)

# Cross compiler — must be set BEFORE project()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Find ARM cross compiler (arm-none-eabi or arm-zephyr-eabi)
find_program(ARM_GCC arm-none-eabi-gcc)
if(NOT ARM_GCC)
    find_program(ARM_GCC arm-zephyr-eabi-gcc REQUIRED)
    string(REPLACE "gcc" "" TOOL_PREFIX_PATH "${ARM_GCC}")
    get_filename_component(TOOL_DIR "${ARM_GCC}" DIRECTORY)
    set(TOOL_PREFIX arm-zephyr-eabi)
else()
    get_filename_component(TOOL_DIR "${ARM_GCC}" DIRECTORY)
    set(TOOL_PREFIX arm-none-eabi)
endif()

set(CMAKE_C_COMPILER "${ARM_GCC}")
find_program(CMAKE_OBJCOPY ${TOOL_PREFIX}-objcopy HINTS "${TOOL_DIR}" REQUIRED)
find_program(CMAKE_OBJDUMP ${TOOL_PREFIX}-objdump HINTS "${TOOL_DIR}" REQUIRED)
find_program(CMAKE_SIZE ${TOOL_PREFIX}-size HINTS "${TOOL_DIR}" REQUIRED)

project(ota_blink_demo C ASM)

# Paths — reuse the production linker script and shared headers
set(EVSE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../app/rak4631_evse_monitor)
set(APP_INC ${EVSE_ROOT}/include)
set(LINKER_SCRIPT ${EVSE_ROOT}/app.ld)

# Compiler flags
set(COMMON_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    -ffunction-sections
    -fdata-sections
    -fno-common
    -fno-builtin
    -ffreestanding
    -Os
    -Wall
    -Wextra
    -Wno-unused-parameter
    -std=c11
)

add_compile_options(${COMMON_FLAGS})
add_compile_definitions(
    __ZEPHYR__=0
)

# Include paths (only need platform_api.h)
include_directories(
    ${APP_INC}
)

# Single source file — the whole demo is one file
add_executable(app.elf
    ${CMAKE_CURRENT_SOURCE_DIR}/app_entry.c
)

target_link_options(app.elf PRIVATE
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    -T${LINKER_SCRIPT}
    -nostartfiles
    -nostdlib
    -nodefaultlibs
    -Wl,--gc-sections
    -Wl,-Map=app.map
)

target_link_libraries(app.elf
    -lc_nano
    -lgcc
)

# Generate hex and binary
add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex --gap-fill 0x00 app.elf app.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary app.elf app.bin
    COMMAND ${CMAKE_SIZE} app.elf
    COMMAND ${CMAKE_OBJDUMP} -h app.elf
    COMMENT "Generating app.hex and app.bin"
)
