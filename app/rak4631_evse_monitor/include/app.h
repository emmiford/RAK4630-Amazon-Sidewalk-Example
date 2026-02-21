/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct app_callbacks;  /* forward declaration */

/**
 * @brief Start Sidewalk end device application.
 *
 * @note This function should never return.
 */
void app_start(void);

/**
 * @brief Check if a valid app image is loaded.
 */
bool app_image_valid(void);

/**
 * @brief Get the app callback table (NULL if no valid image).
 */
const struct app_callbacks *app_get_callbacks(void);

/**
 * @brief Get the reason the app image was rejected (NULL if loaded OK).
 */
const char *app_get_reject_reason(void);

/**
 * @brief Set the periodic timer interval.
 * @param interval_ms  Timer period in milliseconds (100-300000).
 * @return 0 on success, -1 if out of range.
 */
int app_set_timer_interval(uint32_t interval_ms);

/**
 * @brief Route an incoming message to OTA engine or app callback.
 *
 * Messages with first byte == OTA_CMD_TYPE (0x20) go to the OTA engine.
 * Other messages are forwarded to the app's on_msg_received callback
 * if a valid app image is loaded.
 *
 * @param data  Message payload
 * @param len   Length of payload
 */
void app_route_message(const uint8_t *data, size_t len);

#endif /* APP_H */
