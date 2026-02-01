/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Thermostat Digital Input Interface
 */

#ifndef THERMOSTAT_INPUTS_H
#define THERMOSTAT_INPUTS_H

#include <stdint.h>
#include <stdbool.h>

/* Thermostat flag bit positions */
#define THERMOSTAT_FLAG_HEAT    (1 << 0)
#define THERMOSTAT_FLAG_COOL    (1 << 1)

/**
 * @brief Initialize the thermostat GPIO inputs
 *
 * @return 0 on success, negative errno on failure
 */
int thermostat_inputs_init(void);

/**
 * @brief Read the heat call input state
 *
 * @return true if heat call is active, false otherwise
 */
bool thermostat_heat_call_get(void);

/**
 * @brief Read the cool call input state
 *
 * @return true if cool call is active, false otherwise
 */
bool thermostat_cool_call_get(void);

/**
 * @brief Get all thermostat states as a flag byte
 *
 * @return Bit flags: bit 0 = heat call, bit 1 = cool call
 */
uint8_t thermostat_flags_get(void);

#endif /* THERMOSTAT_INPUTS_H */
