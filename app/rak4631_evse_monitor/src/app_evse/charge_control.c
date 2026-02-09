/*
 * Charge Control Implementation
 *
 * App-side: GPIO access goes through platform API.
 */

#include <charge_control.h>
#include <platform_api.h>
#include <string.h>

static const struct platform_api *api;

/* Current state */
static charge_control_state_t current_state = {
	.charging_allowed = true,
	.auto_resume_min = 0,
	.pause_timestamp_ms = 0
};

void charge_control_set_api(const struct platform_api *platform)
{
	api = platform;
}

int charge_control_init(void)
{
	/* Platform owns GPIO init — just set default state */
	if (api) {
		api->gpio_set(PIN_CHARGE_EN, 1);
		api->log_inf("Charge control initialized");
	}
	return 0;
}

int charge_control_process_cmd(const uint8_t *data, size_t len)
{
	if (data == NULL || len < sizeof(charge_control_cmd_t)) {
		return -1;
	}

	const charge_control_cmd_t *cmd = (const charge_control_cmd_t *)data;

	if (cmd->cmd_type != CHARGE_CONTROL_CMD_TYPE) {
		return -1;
	}

	bool allowed = (cmd->charge_allowed != 0);
	uint16_t duration = cmd->duration_min;

	if (api) {
		api->log_inf("Charge control command: allowed=%d, duration=%d min",
			     allowed, duration);
	}

	charge_control_set(allowed, duration);
	return 0;
}

void charge_control_set(bool allowed, uint16_t auto_resume_min)
{
	current_state.charging_allowed = allowed;
	current_state.auto_resume_min = auto_resume_min;

	if (!allowed && auto_resume_min > 0 && api) {
		current_state.pause_timestamp_ms = (int64_t)api->uptime_ms();
	} else {
		current_state.pause_timestamp_ms = 0;
	}

	if (api) {
		api->gpio_set(PIN_CHARGE_EN, allowed ? 1 : 0);
		api->log_inf("Charge control: %s%s",
			     allowed ? "ALLOW" : "PAUSE",
			     (!allowed && auto_resume_min > 0) ? " (with auto-resume)" : "");
	}
}

void charge_control_get_state(charge_control_state_t *state)
{
	if (state) {
		*state = current_state;
	}
}

bool charge_control_is_allowed(void)
{
	return current_state.charging_allowed;
}

void charge_control_tick(void)
{
	if (!current_state.charging_allowed &&
	    current_state.auto_resume_min > 0 &&
	    current_state.pause_timestamp_ms > 0 &&
	    api) {
		uint32_t now = api->uptime_ms();
		int64_t elapsed_ms = (int64_t)now - current_state.pause_timestamp_ms;
		int64_t resume_threshold_ms = (int64_t)current_state.auto_resume_min * 60 * 1000;

		if (elapsed_ms >= resume_threshold_ms) {
			api->log_inf("Auto-resume timer expired, allowing charging");
			current_state.charging_allowed = true;
			current_state.auto_resume_min = 0;
			current_state.pause_timestamp_ms = 0;
			api->gpio_set(PIN_CHARGE_EN, 1);
		}
	}
}
