/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * EVSE Sensor Interface for J1772 Pilot and Current Clamp
 */

#ifndef EVSE_SENSORS_H
#define EVSE_SENSORS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief J1772 charging state enumeration
 *
 * Based on SAE J1772 pilot signal voltages:
 *   State A: +12V (Not connected)
 *   State B: +9V  (Connected, not ready to charge)
 *   State C: +6V  (Charging)
 *   State D: +3V  (Charging with ventilation required)
 *   State E: 0V   (Error - no power)
 *   State F: -12V (Error - EVSE not available)
 */
typedef enum {
    J1772_STATE_A = 0,      /* Not connected (+12V) */
    J1772_STATE_B,          /* Connected, not ready (+9V) */
    J1772_STATE_C,          /* Charging (+6V) */
    J1772_STATE_D,          /* Charging w/ ventilation (+3V) */
    J1772_STATE_E,          /* Error (0V) */
    J1772_STATE_F,          /* Error (-12V) */
    J1772_STATE_UNKNOWN
} j1772_state_t;

/**
 * @brief Initialize the EVSE ADC sensors
 *
 * @return 0 on success, negative errno on failure
 */
int evse_sensors_init(void);

/**
 * @brief Read the J1772 pilot voltage
 *
 * @param[out] voltage_mv Pilot voltage in millivolts (after divider)
 * @return 0 on success, negative errno on failure
 */
int evse_pilot_voltage_read(uint16_t *voltage_mv);

/**
 * @brief Get the J1772 charging state from pilot voltage
 *
 * @param[out] state Detected J1772 state
 * @param[out] voltage_mv Optional: raw voltage reading in mV (can be NULL)
 * @return 0 on success, negative errno on failure
 */
int evse_j1772_state_get(j1772_state_t *state, uint16_t *voltage_mv);

/**
 * @brief Read the current clamp value
 *
 * @param[out] current_ma Current reading in milliamps
 * @return 0 on success, negative errno on failure
 */
int evse_current_read(uint16_t *current_ma);

/**
 * @brief Get string representation of J1772 state
 *
 * @param state J1772 state enum value
 * @return Constant string describing the state
 */
const char *j1772_state_to_string(j1772_state_t state);

/**
 * @brief Simulate a J1772 state for testing
 *
 * Overrides sensor readings with simulated state for specified duration.
 * When duration expires, returns to real sensor readings.
 *
 * @param j1772_state State to simulate (J1772_STATE_A, B, C, etc.)
 * @param duration_ms Duration in milliseconds (0 to cancel simulation)
 */
void evse_sensors_simulate_state(uint8_t j1772_state, uint32_t duration_ms);

/**
 * @brief Check if simulation mode is active
 *
 * @return true if simulation is active, false for real sensor readings
 */
bool evse_sensors_is_simulating(void);

#endif /* EVSE_SENSORS_H */
