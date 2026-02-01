/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <charge_control.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(charge_control, CONFIG_SIDEWALK_LOG_LEVEL);

/* GPIO specification for charge enable output */
static const struct gpio_dt_spec charge_enable_gpio =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(charge_enable), gpios, {0});

/* Current state */
static charge_control_state_t current_state = {
	.charging_allowed = true,
	.auto_resume_min = 0,
	.pause_timestamp_ms = 0
};

/* Mutex for state access */
static K_MUTEX_DEFINE(state_mutex);

int charge_control_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&charge_enable_gpio)) {
		LOG_WRN("Charge enable GPIO not configured in device tree");
		LOG_WRN("Add 'charge_enable' node to overlay for hardware control");
		return 0; /* Not fatal - can still process commands for testing */
	}

	ret = gpio_pin_configure_dt(&charge_enable_gpio, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure charge enable GPIO: %d", ret);
		return ret;
	}

	/* Start with charging allowed */
	gpio_pin_set_dt(&charge_enable_gpio, 1);

	LOG_INF("Charge control initialized, GPIO ready");
	return 0;
}

int charge_control_process_cmd(const uint8_t *data, size_t len)
{
	if (data == NULL || len < sizeof(charge_control_cmd_t)) {
		LOG_ERR("Invalid charge control command: len=%zu, expected=%zu",
			len, sizeof(charge_control_cmd_t));
		return -EINVAL;
	}

	const charge_control_cmd_t *cmd = (const charge_control_cmd_t *)data;

	if (cmd->cmd_type != CHARGE_CONTROL_CMD_TYPE) {
		LOG_ERR("Invalid command type: 0x%02x, expected 0x%02x",
			cmd->cmd_type, CHARGE_CONTROL_CMD_TYPE);
		return -EINVAL;
	}

	bool allowed = (cmd->charge_allowed != 0);
	uint16_t duration = cmd->duration_min; /* Already little-endian on ARM */

	LOG_INF("Charge control command: allowed=%d, duration=%d min",
		allowed, duration);

	charge_control_set(allowed, duration);

	return 0;
}

void charge_control_set(bool allowed, uint16_t auto_resume_min)
{
	k_mutex_lock(&state_mutex, K_FOREVER);

	current_state.charging_allowed = allowed;
	current_state.auto_resume_min = auto_resume_min;

	if (!allowed && auto_resume_min > 0) {
		current_state.pause_timestamp_ms = k_uptime_get();
	} else {
		current_state.pause_timestamp_ms = 0;
	}

	k_mutex_unlock(&state_mutex);

	/* Update GPIO output */
	if (gpio_is_ready_dt(&charge_enable_gpio)) {
		gpio_pin_set_dt(&charge_enable_gpio, allowed ? 1 : 0);
	}

	LOG_INF("Charge control: %s%s",
		allowed ? "ALLOW" : "PAUSE",
		(!allowed && auto_resume_min > 0) ? " (with auto-resume)" : "");
}

void charge_control_get_state(charge_control_state_t *state)
{
	if (state == NULL) {
		return;
	}

	k_mutex_lock(&state_mutex, K_FOREVER);
	*state = current_state;
	k_mutex_unlock(&state_mutex);
}

bool charge_control_is_allowed(void)
{
	bool allowed;

	k_mutex_lock(&state_mutex, K_FOREVER);
	allowed = current_state.charging_allowed;
	k_mutex_unlock(&state_mutex);

	return allowed;
}

void charge_control_tick(void)
{
	k_mutex_lock(&state_mutex, K_FOREVER);

	/* Check for auto-resume */
	if (!current_state.charging_allowed &&
	    current_state.auto_resume_min > 0 &&
	    current_state.pause_timestamp_ms > 0) {

		int64_t now = k_uptime_get();
		int64_t elapsed_ms = now - current_state.pause_timestamp_ms;
		int64_t resume_threshold_ms = (int64_t)current_state.auto_resume_min * 60 * 1000;

		if (elapsed_ms >= resume_threshold_ms) {
			LOG_INF("Auto-resume timer expired, allowing charging");
			current_state.charging_allowed = true;
			current_state.auto_resume_min = 0;
			current_state.pause_timestamp_ms = 0;

			/* Update GPIO output */
			if (gpio_is_ready_dt(&charge_enable_gpio)) {
				gpio_pin_set_dt(&charge_enable_gpio, 1);
			}
		}
	}

	k_mutex_unlock(&state_mutex);
}
