cmake_minimum_required(VERSION 3.20.0)

project(evse_tests C)

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

# App module sources (no app_entry — it requires HOST_TEST for the section attribute)
set(APP_MODULE_SRCS
    ${APP_SRC}/evse_sensors.c
    ${APP_SRC}/charge_control.c
    ${APP_SRC}/thermostat_inputs.c
    ${APP_SRC}/evse_payload.c
    ${APP_SRC}/app_tx.c
    ${APP_SRC}/app_rx.c
    ${APP_SRC}/selftest.c
    ${APP_SRC}/selftest_trigger.c
    ${APP_SRC}/charge_now.c
    ${APP_SRC}/time_sync.c
    ${APP_SRC}/event_buffer.c
    ${APP_SRC}/event_filter.c
    ${APP_SRC}/delay_window.c
    ${APP_SRC}/diag_request.c
    ${APP_SRC}/led_engine.c
    ${APP_SRC}/cmd_auth.c
)

# Full set including app_entry.c (needs HOST_TEST + HEARTBEAT_INTERVAL_MS)
set(ALL_APP_SRCS ${APP_SRC}/app_entry.c ${APP_MODULE_SRCS})

# Common compiler flags
add_compile_options(-Wall -Wextra -Wno-unused-parameter -std=c11 -g)
add_compile_definitions(__ZEPHYR__=0)

# Unity library
add_library(unity STATIC ${UNITY_DIR}/unity.c)
target_include_directories(unity PUBLIC ${UNITY_DIR})

# Mock platform library (includes app_platform.c for the shared `platform` global)
add_library(mock_platform STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_platform_api.c
    ${APP_SRC}/app_platform.c
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

# --- Focused unit tests (single module + minimal deps) ---

add_unit_test(test_evse_sensors
    ${APP_SRC}/evse_sensors.c
)

add_unit_test(test_charge_control
    ${APP_SRC}/charge_control.c
    ${APP_SRC}/delay_window.c
    ${APP_SRC}/time_sync.c
)

add_unit_test(test_thermostat_inputs
    ${APP_SRC}/thermostat_inputs.c
)

add_unit_test(test_time_sync
    ${APP_SRC}/time_sync.c
)

add_unit_test(test_event_buffer
    ${APP_SRC}/event_buffer.c
)

# --- Integration tests (need all app modules) ---

add_unit_test(test_app_tx ${APP_MODULE_SRCS})
add_unit_test(test_app_rx ${APP_MODULE_SRCS})
add_unit_test(test_selftest_trigger ${APP_MODULE_SRCS})

# shell command dispatch
add_executable(test_shell_commands
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_shell_commands.c
    ${ALL_APP_SRCS}
)
target_include_directories(test_shell_commands PRIVATE ${TEST_INCLUDES})
target_compile_definitions(test_shell_commands PRIVATE HOST_TEST)
target_link_libraries(test_shell_commands unity mock_platform)
add_test(NAME test_shell_commands COMMAND test_shell_commands)

# --- Monolithic app-layer integration test (assert-based, not Unity) ---
# Migrated from the Grenning Makefile — compiles all app_evse modules
# against the consolidated mock_platform_api.

add_executable(test_app
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_app.c
    ${ALL_APP_SRCS}
)
target_include_directories(test_app PRIVATE ${TEST_INCLUDES})
target_compile_definitions(test_app PRIVATE HOST_TEST HEARTBEAT_INTERVAL_MS=60000)
target_link_libraries(test_app mock_platform)
add_test(NAME test_app COMMAND test_app)

# --- Platform boot path test (assert-based, not Unity) ---
# Tests discover_app_image(), app_route_message(), timer bounds in app.c.

add_executable(test_boot_path
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_boot_path.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_boot.c
    ${APP_ROOT}/src/app.c
)
target_include_directories(test_boot_path PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks   # mock Zephyr/Sidewalk headers
    ${APP_INC}
)
target_compile_definitions(test_boot_path PRIVATE HOST_TEST)
target_compile_options(test_boot_path PRIVATE -Wno-unused-function)
add_test(NAME test_boot_path COMMAND test_boot_path)

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

add_executable(test_ota_recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_ota_recovery.c
    ${APP_ROOT}/src/ota_flash.c
    ${APP_ROOT}/src/ota_update.c
)
target_include_directories(test_ota_recovery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${APP_INC}
    ${UNITY_DIR}
)
target_link_libraries(test_ota_recovery unity mock_flash mock_ota_signing)
add_test(NAME test_ota_recovery COMMAND test_ota_recovery)

# --- MFG key health check tests ---

add_executable(test_mfg_health
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_mfg_health.c
    ${APP_ROOT}/src/mfg_health.c
)
target_include_directories(test_mfg_health PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
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
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${APP_INC}
    ${UNITY_DIR}
)
target_link_libraries(test_ota_chunks unity mock_flash mock_ota_signing)
add_test(NAME test_ota_chunks COMMAND test_ota_chunks)

# OTA ED25519 signing tests
add_executable(test_ota_signing
    ${CMAKE_CURRENT_SOURCE_DIR}/app/test_ota_signing.c
    ${APP_ROOT}/src/ota_flash.c
    ${APP_ROOT}/src/ota_update.c
)
target_include_directories(test_ota_signing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${APP_INC}
    ${UNITY_DIR}
)
target_link_libraries(test_ota_signing unity mock_flash mock_ota_signing)
add_test(NAME test_ota_signing COMMAND test_ota_signing)
