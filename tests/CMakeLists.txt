cmake_minimum_required(VERSION 3.20.0)

project(rak_sid_tests C)

# Use host compiler (not ARM cross-compiler)
enable_testing()

# Paths
set(APP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../app/rak4631_evse_monitor)
set(APP_SRC ${APP_ROOT}/src/app_evse)
set(APP_INC ${APP_ROOT}/include)

# Unity test framework
set(UNITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/unity)

# Include paths
set(TEST_INCLUDES
    ${APP_INC}
    ${UNITY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)

# Common compiler flags
add_compile_options(-Wall -Wextra -Wno-unused-parameter -std=c11 -g)
add_compile_definitions(__ZEPHYR__=0)

# Unity library
add_library(unity STATIC ${UNITY_DIR}/unity.c)
target_include_directories(unity PUBLIC ${UNITY_DIR})

# Mock platform library
add_library(mock_platform STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_platform_api.c
)
target_include_directories(mock_platform PUBLIC ${TEST_INCLUDES})

# --- Helper function to add a test executable ---
function(add_unit_test TEST_NAME)
    # Remaining arguments are source files under test
    set(SOURCES_UNDER_TEST ${ARGN})

    add_executable(${TEST_NAME}
        ${CMAKE_CURRENT_SOURCE_DIR}/app/${TEST_NAME}.c
        ${SOURCES_UNDER_TEST}
    )
    target_include_directories(${TEST_NAME} PRIVATE ${TEST_INCLUDES})
    target_link_libraries(${TEST_NAME} unity mock_platform)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# --- Test executables ---

# evse_sensors tests
add_unit_test(test_evse_sensors
    ${APP_SRC}/evse_sensors.c
)

# charge_control tests
add_unit_test(test_charge_control
    ${APP_SRC}/charge_control.c
)

# thermostat_inputs tests
add_unit_test(test_thermostat_inputs
    ${APP_SRC}/thermostat_inputs.c
)

# app_tx tests (needs evse_sensors + thermostat_inputs + evse_payload + charge_control + time_sync)
add_unit_test(test_app_tx
    ${APP_SRC}/app_tx.c
    ${APP_SRC}/evse_payload.c
    ${APP_SRC}/evse_sensors.c
    ${APP_SRC}/thermostat_inputs.c
    ${APP_SRC}/selftest.c
    ${APP_SRC}/charge_control.c
    ${APP_SRC}/time_sync.c
)

# app_rx tests (needs charge_control + time_sync + event_buffer)
add_unit_test(test_app_rx
    ${APP_SRC}/app_rx.c
    ${APP_SRC}/charge_control.c
    ${APP_SRC}/time_sync.c
    ${APP_SRC}/event_buffer.c
)

# time_sync tests
add_unit_test(test_time_sync
    ${APP_SRC}/time_sync.c
)

# event_buffer tests
add_unit_test(test_event_buffer
    ${APP_SRC}/event_buffer.c
)

# selftest_trigger tests (needs selftest + evse_sensors + charge_control)
add_unit_test(test_selftest_trigger
    ${APP_SRC}/selftest_trigger.c
    ${APP_SRC}/selftest.c
    ${APP_SRC}/evse_sensors.c
    ${APP_SRC}/charge_control.c
)

# shell command dispatch tests (needs app_entry + all app modules)
add_executable(test_shell_commands
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_shell_commands.c
    ${APP_SRC}/app_entry.c
    ${APP_SRC}/evse_sensors.c
    ${APP_SRC}/charge_control.c
    ${APP_SRC}/thermostat_inputs.c
    ${APP_SRC}/evse_payload.c
    ${APP_SRC}/app_tx.c
    ${APP_SRC}/app_rx.c
    ${APP_SRC}/selftest.c
    ${APP_SRC}/selftest_trigger.c
    ${APP_SRC}/time_sync.c
    ${APP_SRC}/event_buffer.c
)
target_include_directories(test_shell_commands PRIVATE ${TEST_INCLUDES})
target_compile_definitions(test_shell_commands PRIVATE HOST_TEST)
target_link_libraries(test_shell_commands unity mock_platform)
add_test(NAME test_shell_commands COMMAND test_shell_commands)

# --- OTA recovery tests (platform module, needs mock Zephyr) ---

# Mock flash library (provides RAM-backed Zephyr flash stubs)
add_library(mock_flash STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_flash.c
)
target_include_directories(mock_flash PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${APP_INC}
)

# Mock OTA signing library (controllable ota_verify_signature)
add_library(mock_ota_signing STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_ota_signing.c
)
target_include_directories(mock_ota_signing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${APP_INC}
)

# OTA recovery test — compiles ota_update.c + ota_flash.c against mock Zephyr headers
add_executable(test_ota_recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_ota_recovery.c
    ${APP_ROOT}/src/ota_flash.c
    ${APP_ROOT}/src/ota_update.c
)
target_include_directories(test_ota_recovery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks   # mock Zephyr headers found here first
    ${APP_INC}
    ${UNITY_DIR}
)
target_link_libraries(test_ota_recovery unity mock_flash mock_ota_signing)
add_test(NAME test_ota_recovery COMMAND test_ota_recovery)

# --- MFG key health check tests (platform module, needs mock MFG store) ---

add_executable(test_mfg_health
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_mfg_health.c
    ${APP_ROOT}/src/mfg_health.c
)
target_include_directories(test_mfg_health PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks   # mock sid_pal_mfg_store_ifc.h + Zephyr headers
    ${APP_INC}
    ${UNITY_DIR}
)
target_link_libraries(test_mfg_health unity)
add_test(NAME test_mfg_health COMMAND test_mfg_health)

# OTA chunk receive + delta bitmap tests
add_executable(test_ota_chunks
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_ota_chunks.c
    ${APP_ROOT}/src/ota_flash.c
    ${APP_ROOT}/src/ota_update.c
)
target_include_directories(test_ota_chunks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks   # mock Zephyr headers found here first
    ${APP_INC}
    ${UNITY_DIR}
)
target_link_libraries(test_ota_chunks unity mock_flash mock_ota_signing)
add_test(NAME test_ota_chunks COMMAND test_ota_chunks)

# OTA ED25519 signing tests — uses mock_ota_signing instead of real ota_signing.c
add_executable(test_ota_signing
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_ota_signing.c
    ${APP_ROOT}/src/ota_flash.c
    ${APP_ROOT}/src/ota_update.c
)
target_include_directories(test_ota_signing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks   # mock Zephyr headers found here first
    ${APP_INC}
    ${UNITY_DIR}
)
target_link_libraries(test_ota_signing unity mock_flash mock_ota_signing)
add_test(NAME test_ota_signing COMMAND test_ota_signing)
