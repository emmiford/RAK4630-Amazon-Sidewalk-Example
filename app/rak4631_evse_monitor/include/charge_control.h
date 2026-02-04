/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef CHARGE_CONTROL_H
#define CHARGE_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Charge control command type (matches Lambda CMD_TYPE_CHARGE_CONTROL)
 */
#define CHARGE_CONTROL_CMD_TYPE 0x10

/**
 * @brief Charge control command structure
 *
 * Binary format from cloud:
 *   Byte 0: cmd_type (0x10 = charge control)
 *   Byte 1: charge_allowed (0 = pause, 1 = allow)
 *   Bytes 2-3: duration_min (little-endian, 0 = no auto-resume)
 */
typedef struct __attribute__((packed)) {
	uint8_t cmd_type;
	uint8_t charge_allowed;
	uint16_t duration_min;
} charge_control_cmd_t;

/**
 * @brief Current charge control state
 */
typedef struct {
	bool charging_allowed;
	uint16_t auto_resume_min;
	int64_t pause_timestamp_ms;
} charge_control_state_t;

/**
 * @brief Initialize charge control subsystem
 *
 * Configures the GPIO output for EVSE control and sets default state.
 *
 * @return 0 on success, negative errno on failure
 */
int charge_control_init(void);

/**
 * @brief Process a charge control command from downlink
 *
 * @param data Raw command data bytes
 * @param len Length of command data
 * @return 0 on success, negative errno on failure
 */
int charge_control_process_cmd(const uint8_t *data, size_t len);

/**
 * @brief Set charging allowed state
 *
 * @param allowed true to allow charging, false to pause
 * @param auto_resume_min Minutes until auto-resume (0 = no auto-resume)
 */
void charge_control_set(bool allowed, uint16_t auto_resume_min);

/**
 * @brief Get current charge control state
 *
 * @param state Pointer to state structure to fill
 */
void charge_control_get_state(charge_control_state_t *state);

/**
 * @brief Check if charging is currently allowed
 *
 * @return true if charging allowed, false if paused
 */
bool charge_control_is_allowed(void);

/**
 * @brief Periodic task to check auto-resume timer
 *
 * Call this periodically (e.g., every second) to handle auto-resume.
 */
void charge_control_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* CHARGE_CONTROL_H */
