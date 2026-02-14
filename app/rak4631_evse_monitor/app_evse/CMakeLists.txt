#
# Standalone App Image Build
#
# Builds the EVSE app code as a standalone binary that runs on top of
# the platform image.  Uses arm-zephyr-eabi-gcc (no Zephyr build system).
#
# Usage (inside nrfutil toolchain-manager launch):
#   mkdir build_app && cd build_app
#   cmake ../rak-sid/app/rak4631_evse_monitor/app_evse
#   make
#
# Output: app.hex (flash at 0x90000)
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

project(evse_app C ASM)

# Paths
set(APP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(APP_SRC ${APP_ROOT}/src/app_evse)
set(APP_INC ${APP_ROOT}/include)
set(LINKER_SCRIPT ${APP_ROOT}/app.ld)

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

# Include paths
include_directories(
    ${APP_INC}
)

# App source files
set(APP_SOURCES
    ${APP_SRC}/app_entry.c
    ${APP_SRC}/evse_sensors.c
    ${APP_SRC}/charge_control.c
    ${APP_SRC}/thermostat_inputs.c
    ${APP_SRC}/rak_sidewalk.c
    ${APP_SRC}/app_tx.c
    ${APP_SRC}/app_rx.c
    ${APP_SRC}/selftest.c
    ${APP_SRC}/time_sync.c
    ${APP_SRC}/event_buffer.c
)

# Build the ELF
add_executable(app.elf ${APP_SOURCES})

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

# We need libc for string functions — use newlib-nano
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
