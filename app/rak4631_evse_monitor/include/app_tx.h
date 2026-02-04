/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_TX_H
#define APP_TX_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Set the Sidewalk ready state
 *
 * Called from status callback when Sidewalk link becomes ready/not ready.
 *
 * @param ready true when Sidewalk is ready to send messages
 */
void app_tx_set_ready(bool ready);

/**
 * @brief Send EVSE telemetry data over Sidewalk
 *
 * Reads current sensor values and sends an 8-byte raw payload:
 *   Byte 0: Magic (0xE5)
 *   Byte 1: Version (0x01)
 *   Byte 2: J1772 state (0-6)
 *   Byte 3-4: Pilot voltage mV (little-endian)
 *   Byte 5-6: Current mA (little-endian)
 *   Byte 7: Thermostat flags
 *
 * @return 0 on success, negative errno on failure
 */
int app_tx_send_evse_data(void);

/**
 * @brief Update the link mask for outgoing messages
 *
 * Called from status callback to track available links.
 *
 * @param link_mask Bitmask of available Sidewalk links
 */
void app_tx_set_link_mask(uint32_t link_mask);

#endif /* APP_TX_H */
