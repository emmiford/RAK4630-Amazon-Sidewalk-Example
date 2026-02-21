#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(evse_monitoring)

# Platform sources — Zephyr + Sidewalk + drivers + platform API
# Excludes the app_evse/ directory (built separately as standalone app)
target_sources(app PRIVATE
    src/main.c
    src/app.c
    src/sidewalk_dispatch.c
    src/platform_shell.c
    src/sidewalk.c
    src/sidewalk_events.c
    src/tx_state.c
    src/app_leds.c
    src/platform_api_impl.c
    src/ota_flash.c
    src/ota_update.c
    src/ota_signing.c
    src/mfg_health.c
)

zephyr_include_directories(
    include
)

# Place .platform_api section at fixed address 0x7FF00
zephyr_linker_sources(SECTIONS platform_api.ld)

# Read build versions from VERSION files
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/BUILD_VERSION" APP_BUILD_VER)
string(STRIP "${APP_BUILD_VER}" APP_BUILD_VER)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/PLATFORM_BUILD_VERSION" PLATFORM_BUILD_VER)
string(STRIP "${PLATFORM_BUILD_VER}" PLATFORM_BUILD_VER)
zephyr_compile_definitions(
    APP_BUILD_VERSION=${APP_BUILD_VER}
    PLATFORM_BUILD_VERSION=${PLATFORM_BUILD_VER}
)
