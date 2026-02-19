/*
 * Charge Now 30-Minute Latch Implementation
 *
 * RAM-only state: power loss = latch lost, safe default restored.
 */

#include <charge_now.h>
#include <charge_control.h>
#include <delay_window.h>
#include <led_engine.h>
#include <evse_sensors.h>
#include <platform_api.h>

static const struct platform_api *api;
static bool active;
static uint32_t start_ms;

void charge_now_set_api(const struct platform_api *platform)
{
	api = platform;
}

void charge_now_init(void)
{
	active = false;
	start_ms = 0;
}

void charge_now_activate(void)
{
	if (!api) {
		return;
	}

	active = true;
	start_ms = api->uptime_ms();

	/* Force charging on */
	charge_control_set_with_reason(true, 0, TRANSITION_REASON_CHARGE_NOW);

	/* Clear any active delay window */
	delay_window_clear();

	/* LED: 3 rapid blinks (ack) then 0.5Hz slow blink (override) */
	led_engine_button_ack();
	led_engine_set_charge_now_override(true);

	api->log_inf("Charge Now: activated (30 min)");
}

void charge_now_cancel(void)
{
	if (!active) {
		return;
	}

	active = false;
	led_engine_set_charge_now_override(false);

	if (api) {
		api->log_inf("Charge Now: cancelled");
	}
}

void charge_now_tick(uint8_t j1772_state)
{
	if (!active || !api) {
		return;
	}

	/* Check 30-minute expiry */
	uint32_t elapsed = api->uptime_ms() - start_ms;
	if (elapsed >= CHARGE_NOW_DURATION_MS) {
		api->log_inf("Charge Now: expired after 30 min");
		charge_now_cancel();
		return;
	}

	/* Unplug cancels latch (J1772 state A = 0) */
	if (j1772_state == (uint8_t)J1772_STATE_A) {
		api->log_inf("Charge Now: cancelled (vehicle unplugged)");
		charge_now_cancel();
	}
}

bool charge_now_is_active(void)
{
	return active;
}
