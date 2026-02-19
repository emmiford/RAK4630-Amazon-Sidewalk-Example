/*
 * Charge Control Implementation
 *
 * App-side: GPIO access goes through platform API.
 */

#include <charge_control.h>
#include <delay_window.h>
#include <time_sync.h>
#include <platform_api.h>
#include <string.h>

/* GPIO pin index — must match platform board-level mapping */
#define EVSE_PIN_CHARGE_EN  0

static const struct platform_api *api;

/* Current state */
static charge_control_state_t current_state = {
	.charging_allowed = true,
	.auto_resume_min = 0,
	.pause_timestamp_ms = 0
};

/* Last transition reason (cleared after read by app_entry snapshot) */
static uint8_t last_transition_reason = TRANSITION_REASON_NONE;

void charge_control_set_api(const struct platform_api *platform)
{
	api = platform;
}

int charge_control_init(void)
{
	/* Reset all state to defaults */
	current_state.charging_allowed = true;
	current_state.auto_resume_min = 0;
	current_state.pause_timestamp_ms = 0;
	last_transition_reason = TRANSITION_REASON_NONE;

	/* Platform owns GPIO init — just set default state */
	if (api) {
		api->gpio_set(EVSE_PIN_CHARGE_EN, 1);
		api->log_inf("Charge control initialized");
	}
	return 0;
}

int charge_control_process_cmd(const uint8_t *data, size_t len)
{
	if (data == NULL || len < sizeof(charge_control_cmd_t)) {
		if (api) api->log_wrn("charge_control: bad args data=%p len=%u", data, (unsigned)len);
		return -1;
	}

	const charge_control_cmd_t *cmd = (const charge_control_cmd_t *)data;

	if (cmd->cmd_type != CHARGE_CONTROL_CMD_TYPE) {
		if (api) api->log_wrn("charge_control: unexpected cmd_type 0x%02x", cmd->cmd_type);
		return -1;
	}

	/* Legacy command clears any active delay window */
	delay_window_clear();

	bool allowed = (cmd->charge_allowed != 0);
	uint16_t duration = cmd->duration_min;

	if (api) {
		api->log_inf("Charge control command: allowed=%d, duration=%d min",
			     allowed, duration);
	}

	charge_control_set_with_reason(allowed, duration, TRANSITION_REASON_CLOUD_CMD);
	return 0;
}

void charge_control_set_with_reason(bool allowed, uint16_t auto_resume_min,
				    uint8_t reason)
{
	/* Record transition reason only when state actually changes */
	if (allowed != current_state.charging_allowed) {
		last_transition_reason = reason;
	}

	current_state.charging_allowed = allowed;
	current_state.auto_resume_min = auto_resume_min;

	if (!allowed && auto_resume_min > 0 && api) {
		current_state.pause_timestamp_ms = (int64_t)api->uptime_ms();
	} else {
		current_state.pause_timestamp_ms = 0;
	}

	if (api) {
		api->gpio_set(EVSE_PIN_CHARGE_EN, allowed ? 1 : 0);
		api->log_inf("Charge control: %s%s",
			     allowed ? "ALLOW" : "PAUSE",
			     (!allowed && auto_resume_min > 0) ? " (with auto-resume)" : "");
	}
}

void charge_control_set(bool allowed, uint16_t auto_resume_min)
{
	charge_control_set_with_reason(allowed, auto_resume_min,
				       TRANSITION_REASON_NONE);
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

uint8_t charge_control_get_last_reason(void)
{
	return last_transition_reason;
}

void charge_control_clear_last_reason(void)
{
	last_transition_reason = TRANSITION_REASON_NONE;
}

void charge_control_tick(void)
{
	if (!api) {
		return;
	}

	/* --- Delay window management (requires time sync) --- */
	if (delay_window_has_window()) {
		uint32_t now = time_sync_get_epoch();
		if (now != 0) {
			uint32_t start, end;
			delay_window_get(&start, &end);

			if (now > end) {
				/* Window expired — resume and clear */
				if (!current_state.charging_allowed) {
					api->log_inf("Delay window expired, resuming");
					last_transition_reason = TRANSITION_REASON_DELAY_WINDOW;
					current_state.charging_allowed = true;
					current_state.auto_resume_min = 0;
					current_state.pause_timestamp_ms = 0;
					api->gpio_set(EVSE_PIN_CHARGE_EN, 1);
				}
				delay_window_clear();
			} else if (now >= start && current_state.charging_allowed) {
				/* Window active — pause charging */
				api->log_inf("Delay window active, pausing");
				last_transition_reason = TRANSITION_REASON_DELAY_WINDOW;
				current_state.charging_allowed = false;
				api->gpio_set(EVSE_PIN_CHARGE_EN, 0);
			}
			return;  /* Delay window controls state — skip auto-resume */
		}
		/* Time not synced — fall through to auto-resume */
	}

	/* --- Auto-resume timer (legacy, uses relative uptime) --- */
	if (!current_state.charging_allowed &&
	    current_state.auto_resume_min > 0 &&
	    current_state.pause_timestamp_ms > 0) {
		uint32_t now_ms = api->uptime_ms();
		int64_t elapsed_ms = (int64_t)now_ms - current_state.pause_timestamp_ms;
		int64_t resume_threshold_ms = (int64_t)current_state.auto_resume_min * 60 * 1000;

		if (elapsed_ms >= resume_threshold_ms) {
			api->log_inf("Auto-resume timer expired, allowing charging");
			last_transition_reason = TRANSITION_REASON_AUTO_RESUME;
			current_state.charging_allowed = true;
			current_state.auto_resume_min = 0;
			current_state.pause_timestamp_ms = 0;
			api->gpio_set(EVSE_PIN_CHARGE_EN, 1);
		}
	}
}
