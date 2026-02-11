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

# app_tx tests (needs evse_sensors + thermostat_inputs + rak_sidewalk)
add_unit_test(test_app_tx
    ${APP_SRC}/app_tx.c
    ${APP_SRC}/rak_sidewalk.c
    ${APP_SRC}/evse_sensors.c
    ${APP_SRC}/thermostat_inputs.c
)

# app_rx tests (needs charge_control)
add_unit_test(test_app_rx
    ${APP_SRC}/app_rx.c
    ${APP_SRC}/charge_control.c
)
