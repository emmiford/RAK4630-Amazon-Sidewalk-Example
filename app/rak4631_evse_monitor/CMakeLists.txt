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
    src/sidewalk.c
    src/sidewalk_events.c
    src/app_tx.c
    src/app_leds.c
    src/platform_api_impl.c
    src/ota_update.c
    src/sensor_monitor.c
    ext/sid_demo_parser.c
)

zephyr_include_directories(
    include
    ext
)

# Place .platform_api section at fixed address 0x7FF00
zephyr_linker_sources(SECTIONS platform_api.ld)
