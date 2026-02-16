/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SAMPLE_APP_H
#define SAMPLE_APP_H

#include <stdbool.h>
#include <stdint.h>

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

#endif /* SAMPLE_APP_H */
