/*
 * LED Blink Priority State Machine
 *
 * Table-driven blink engine.  Each priority level has a pattern of
 * {duration_ticks, led_on} steps at 100ms resolution.  The highest-priority
 * active condition wins and drives the LED.
 */

#include <led_engine.h>
#include <selftest_trigger.h>
#include <selftest.h>
#include <evse_sensors.h>
#include <charge_control.h>
#include <thermostat_inputs.h>
#include <platform_api.h>

/* ------------------------------------------------------------------ */
/*  Pattern table                                                      */
/* ------------------------------------------------------------------ */

struct blink_step {
	uint8_t duration;   /* ticks (100ms each) */
	bool    on;
};

/* Maximum steps in any pattern */
#define MAX_STEPS 6

struct blink_pattern {
	uint8_t step_count;
	struct blink_step steps[MAX_STEPS];
};

/* Priority 0 — Error: 5Hz = 100ms on, 100ms off */
static const struct blink_pattern pat_error = {
	.step_count = 2,
	.steps = { {1, true}, {1, false} }
};

/* Priority 1 — OTA: double-blink = on-off-on-pause */
static const struct blink_pattern pat_ota = {
	.step_count = 4,
	.steps = { {1, true}, {1, false}, {1, true}, {7, false} }
};

/* Priority 2 — Commissioning: 1Hz = 500ms on, 500ms off */
static const struct blink_pattern pat_commission = {
	.step_count = 2,
	.steps = { {5, true}, {5, false} }
};

/* Priority 3 — Disconnected: triple-blink */
static const struct blink_pattern pat_disconnected = {
	.step_count = 6,
	.steps = { {1, true}, {1, false}, {1, true}, {1, false}, {1, true}, {15, false} }
};

/* Priority 4 — Charge Now: 0.5Hz = 1s on, 1s off */
static const struct blink_pattern pat_charge_now = {
	.step_count = 2,
	.steps = { {10, true}, {10, false} }
};

/* Priority 5 — AC Priority: heartbeat = short on, long off */
static const struct blink_pattern pat_ac_priority = {
	.step_count = 2,
	.steps = { {2, true}, {18, false} }
};

/* Priority 6 — Charging: solid on */
static const struct blink_pattern pat_charging = {
	.step_count = 1,
	.steps = { {1, true} }
};

/* Priority 7 — Idle: blip every 10s */
static const struct blink_pattern pat_idle = {
	.step_count = 2,
	.steps = { {1, true}, {99, false} }
};

static const struct blink_pattern *patterns[LED_PRI_COUNT] = {
	&pat_error,
	&pat_ota,
	&pat_commission,
	&pat_disconnected,
	&pat_charge_now,
	&pat_ac_priority,
	&pat_charging,
	&pat_idle,
};

/* ------------------------------------------------------------------ */
/*  Module state                                                       */
/* ------------------------------------------------------------------ */

static const struct platform_api *api;

/* Pattern playback */
static led_priority_t active_priority;
static uint8_t step_index;
static uint8_t remaining;       /* ticks left in current step */

/* Button-ack overlay */
static bool    ack_active;
static uint8_t ack_step;
static uint8_t ack_remaining;

/* Commissioning lifecycle */
static bool commissioning_active;
static bool first_uplink_sent;

/* External flags (set by notification APIs) */
static bool ota_active;
static bool charge_now_override;

/* Error counters */
static uint8_t adc_fail_count;
static uint8_t gpio_fail_count;
static bool    sidewalk_timeout_error;
static bool    ota_apply_error;
static bool    charge_gpio_error;

/* Sidewalk timeout tracking */
static bool    sidewalk_timeout_started;
static uint32_t sidewalk_timeout_start_ms;

/* ------------------------------------------------------------------ */
/*  Priority evaluation helpers                                        */
/* ------------------------------------------------------------------ */

static bool has_error(void)
{
	if (selftest_get_fault_flags() != 0) {
		return true;
	}
	if (adc_fail_count >= LED_ERROR_THRESHOLD) {
		return true;
	}
	if (gpio_fail_count >= LED_ERROR_THRESHOLD) {
		return true;
	}
	if (sidewalk_timeout_error) {
		return true;
	}
	if (ota_apply_error) {
		return true;
	}
	if (charge_gpio_error) {
		return true;
	}
	return false;
}

static bool has_ota(void)
{
	return ota_active;
}

static bool has_commissioning(void)
{
	if (!commissioning_active) {
		return false;
	}

	/* Exit on first uplink */
	if (first_uplink_sent) {
		commissioning_active = false;
		return false;
	}

	/* Exit on timeout */
	if (api && api->uptime_ms() >= LED_COMMISSION_TIMEOUT_MS) {
		commissioning_active = false;
		return false;
	}

	return true;
}

static bool has_disconnected(void)
{
	/* Only after commissioning exits */
	if (commissioning_active) {
		return false;
	}
	return api && !api->is_ready();
}

static bool has_charge_now(void)
{
	return charge_now_override;
}

static bool has_ac_priority(void)
{
	return thermostat_cool_call_get() && !charge_control_is_allowed();
}

static bool has_charging(void)
{
	j1772_state_t state;
	uint16_t mv;
	if (evse_j1772_state_get(&state, &mv) == 0) {
		return state == J1772_STATE_C;
	}
	return false;
}

typedef bool (*condition_fn)(void);

static const condition_fn conditions[LED_PRI_COUNT] = {
	has_error,
	has_ota,
	has_commissioning,
	has_disconnected,
	has_charge_now,
	has_ac_priority,
	has_charging,
	NULL,  /* idle is always true (fallthrough) */
};

static led_priority_t evaluate_priority(void)
{
	for (int i = 0; i < LED_PRI_COUNT - 1; i++) {
		if (conditions[i] && conditions[i]()) {
			return (led_priority_t)i;
		}
	}
	return LED_PRI_IDLE;
}

/* ------------------------------------------------------------------ */
/*  Button-ack overlay pattern: 3 blinks (on-off-on-off-on-off)       */
/* ------------------------------------------------------------------ */

#define ACK_STEPS 6

static const struct blink_step ack_pattern[ACK_STEPS] = {
	{1, true}, {1, false},
	{1, true}, {1, false},
	{1, true}, {1, false},
};

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

void led_engine_set_api(const struct platform_api *platform)
{
	api = platform;
}

void led_engine_init(void)
{
	active_priority = LED_PRI_IDLE;
	step_index = 0;
	remaining = 0;

	ack_active = false;
	ack_step = 0;
	ack_remaining = 0;

	commissioning_active = true;
	first_uplink_sent = false;

	ota_active = false;
	charge_now_override = false;

	adc_fail_count = 0;
	gpio_fail_count = 0;
	sidewalk_timeout_error = false;
	ota_apply_error = false;
	charge_gpio_error = false;

	sidewalk_timeout_started = false;
	sidewalk_timeout_start_ms = 0;
}

void led_engine_tick(void)
{
	if (!api) {
		return;
	}

	/* Yield to selftest_trigger blink codes */
	if (selftest_trigger_is_running()) {
		step_index = 0;
		remaining = 0;
		return;
	}

	/* Sidewalk timeout tracking: start timer on first tick if not ready */
	if (!sidewalk_timeout_error && !sidewalk_timeout_started && !api->is_ready()) {
		sidewalk_timeout_started = true;
		sidewalk_timeout_start_ms = api->uptime_ms();
	}
	/* Check for sidewalk timeout */
	if (sidewalk_timeout_started && !sidewalk_timeout_error && !api->is_ready()) {
		if ((api->uptime_ms() - sidewalk_timeout_start_ms) >= LED_SIDEWALK_TIMEOUT_MS) {
			sidewalk_timeout_error = true;
		}
	}
	/* Clear sidewalk timeout when connected */
	if (sidewalk_timeout_error && api->is_ready()) {
		sidewalk_timeout_error = false;
		sidewalk_timeout_started = false;
	}

	/* Button-ack overlay */
	if (ack_active) {
		if (ack_step >= ACK_STEPS) {
			ack_active = false;
		} else {
			if (ack_remaining == 0) {
				ack_remaining = ack_pattern[ack_step].duration;
			}
			api->led_set(LED_GREEN, ack_pattern[ack_step].on);
			ack_remaining--;
			if (ack_remaining == 0) {
				ack_step++;
			}
			return;
		}
	}

	/* Evaluate current priority */
	led_priority_t pri = evaluate_priority();

	/* Priority changed — reset playback */
	if (pri != active_priority) {
		active_priority = pri;
		step_index = 0;
		remaining = 0;
	}

	const struct blink_pattern *pat = patterns[active_priority];

	/* Start step if remaining is 0 */
	if (remaining == 0) {
		if (step_index >= pat->step_count) {
			step_index = 0;
		}
		remaining = pat->steps[step_index].duration;
	}

	/* Drive LED */
	api->led_set(LED_GREEN, pat->steps[step_index].on);

	/* Advance */
	remaining--;
	if (remaining == 0) {
		step_index++;
		if (step_index >= pat->step_count) {
			step_index = 0;
		}
	}
}

void led_engine_notify_uplink_sent(void)
{
	first_uplink_sent = true;
}

void led_engine_set_ota_active(bool active)
{
	ota_active = active;
}

void led_engine_set_charge_now_override(bool active)
{
	charge_now_override = active;
}

void led_engine_report_adc_result(bool success)
{
	if (success) {
		adc_fail_count = 0;
	} else {
		if (adc_fail_count < 255) {
			adc_fail_count++;
		}
	}
}

void led_engine_report_gpio_result(bool success)
{
	if (success) {
		gpio_fail_count = 0;
	} else {
		if (gpio_fail_count < 255) {
			gpio_fail_count++;
		}
	}
}

void led_engine_report_charge_gpio_error(void)
{
	charge_gpio_error = true;
}

void led_engine_button_ack(void)
{
	/* Don't start ack if error or OTA is active */
	if (has_error() || has_ota()) {
		return;
	}
	ack_active = true;
	ack_step = 0;
	ack_remaining = 0;
}

led_priority_t led_engine_get_active_priority(void)
{
	return active_priority;
}

bool led_engine_is_commissioning(void)
{
	return commissioning_active;
}
