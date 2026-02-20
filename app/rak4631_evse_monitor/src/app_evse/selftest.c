/*
 * Self-Test and Continuous Monitoring Implementation
 *
 * Boot self-test checks hardware path integrity (<100ms).
 * Continuous monitoring runs every 500ms tick to detect runtime faults.
 * Shell handler provides on-demand commissioning verification.
 */

#include <selftest.h>
#include <evse_sensors.h>
#include <charge_control.h>
#include <thermostat_inputs.h>
#include <app_platform.h>

/* GPIO pin indices — must match platform board-level mapping */
#define EVSE_PIN_CHARGE_BLOCK  0
#define EVSE_PIN_COOL       2

/* Continuous monitoring thresholds */
#define CLAMP_MISMATCH_TIMEOUT_MS  10000
#define INTERLOCK_TIMEOUT_MS       30000
#define PILOT_FAULT_TIMEOUT_MS     5000
#define CHATTER_WINDOW_MS          60000
#define CHATTER_MAX_TOGGLES        10

/* J1772 state constants (matching evse_sensors.h enum values) */
#define J1772_C  2   /* J1772_STATE_C */
#define J1772_UNKNOWN 6  /* J1772_STATE_UNKNOWN */

/* Module state (~85 bytes RAM) */
static uint8_t fault_flags;
static uint32_t clamp_mismatch_start;
static uint32_t interlock_start;
static uint32_t pilot_fault_start;

/* Thermostat chatter detection — circular buffer of toggle timestamps */
#define CHATTER_BUF_SIZE 16
static uint32_t cool_toggle_times[CHATTER_BUF_SIZE];
static uint8_t cool_toggle_head;
static uint8_t cool_toggle_count;
static bool last_cool_call;
static bool last_charge_allowed;

void selftest_reset(void)
{
	fault_flags = 0;
	clamp_mismatch_start = 0;
	interlock_start = 0;
	pilot_fault_start = 0;
	cool_toggle_head = 0;
	cool_toggle_count = 0;
	last_cool_call = false;
	last_charge_allowed = false;
}

/* ------------------------------------------------------------------ */
/*  Boot self-test                                                     */
/* ------------------------------------------------------------------ */

int selftest_boot(selftest_boot_result_t *result)
{
	if (!result || !platform) {
		return -1;
	}

	result->adc_pilot_ok = false;
	result->gpio_cool_ok = false;
	result->charge_block_ok = false;
	result->all_pass = false;

	/* 1. ADC pilot channel readable */
	result->adc_pilot_ok = (platform->adc_read_mv(0) >= 0);

	/* 2. GPIO cool input readable */
	result->gpio_cool_ok = (platform->gpio_get(EVSE_PIN_COOL) >= 0);

	/* 3. Toggle-and-verify on charge block pin:
	 *    Save current → set 1 → readback → set 0 → readback → restore */
	int saved = platform->gpio_get(EVSE_PIN_CHARGE_BLOCK);
	bool toggle_ok = true;

	platform->gpio_set(EVSE_PIN_CHARGE_BLOCK, 1);
	if (platform->gpio_get(EVSE_PIN_CHARGE_BLOCK) != 1) {
		toggle_ok = false;
	}

	platform->gpio_set(EVSE_PIN_CHARGE_BLOCK, 0);
	if (platform->gpio_get(EVSE_PIN_CHARGE_BLOCK) != 0) {
		toggle_ok = false;
	}

	/* Restore original state */
	if (saved >= 0) {
		platform->gpio_set(EVSE_PIN_CHARGE_BLOCK, saved);
	}
	result->charge_block_ok = toggle_ok;

	/* Overall result */
	result->all_pass = result->adc_pilot_ok &&
			   result->gpio_cool_ok &&
			   result->charge_block_ok;

	if (!result->all_pass) {
		fault_flags |= FAULT_SELFTEST;
		/* Brief LED flash to signal boot failure */
		platform->led_set(2, true);
		platform->led_set(2, false);
	} else {
		fault_flags &= ~FAULT_SELFTEST;
	}

	return result->all_pass ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/*  Continuous monitoring                                              */
/* ------------------------------------------------------------------ */

void selftest_continuous_tick(uint8_t j1772_state, uint16_t pilot_mv,
			      uint16_t current_ma, bool charge_allowed,
			      uint8_t thermostat_flags)
{
	if (!platform) {
		return;
	}

	uint32_t now = platform->uptime_ms();
	bool cool_call = (thermostat_flags & THERMOSTAT_FLAG_COOL) != 0;

	/* --- Clamp mismatch: State C + no current, OR not-C + current --- */
	bool state_c = (j1772_state == J1772_C);
	bool current_on = (current_ma >= CURRENT_ON_THRESHOLD_MA);
	bool clamp_bad = (state_c && !current_on) || (!state_c && current_on);

	if (clamp_bad) {
		if (clamp_mismatch_start == 0) {
			clamp_mismatch_start = now;
		} else if ((now - clamp_mismatch_start) >= CLAMP_MISMATCH_TIMEOUT_MS) {
			fault_flags |= FAULT_CLAMP;
		}
	} else {
		clamp_mismatch_start = 0;
		fault_flags &= ~FAULT_CLAMP;
	}

	/* --- Interlock effectiveness: charge_allowed went false but current persists --- */
	if (!last_charge_allowed && last_charge_allowed != charge_allowed) {
		/* Ignore: transition from not-allowed to allowed is not a fault trigger */
	}

	if (last_charge_allowed && !charge_allowed) {
		/* Just transitioned to paused — start watching for current */
		interlock_start = now;
	}

	if (!charge_allowed && current_on) {
		if (interlock_start == 0) {
			interlock_start = now;
		}
		if ((now - interlock_start) >= INTERLOCK_TIMEOUT_MS) {
			fault_flags |= FAULT_INTERLOCK;
		}
	} else if (charge_allowed || !current_on) {
		interlock_start = 0;
		fault_flags &= ~FAULT_INTERLOCK;
	}
	last_charge_allowed = charge_allowed;

	/* --- Pilot out-of-range: UNKNOWN state (covers ADC failure + out-of-range) --- */
	(void)pilot_mv;  /* state already encodes pilot validity via evse_sensors */
	bool pilot_bad = (j1772_state == J1772_UNKNOWN);

	if (pilot_bad) {
		if (pilot_fault_start == 0) {
			pilot_fault_start = now;
		} else if ((now - pilot_fault_start) >= PILOT_FAULT_TIMEOUT_MS) {
			fault_flags |= FAULT_SENSOR;
		}
	} else {
		pilot_fault_start = 0;
		fault_flags &= ~FAULT_SENSOR;
	}

	/* --- Thermostat chatter: >10 toggles in 60s window --- */
	if (cool_call != last_cool_call) {
		/* Record toggle timestamp */
		cool_toggle_times[cool_toggle_head] = now;
		cool_toggle_head = (cool_toggle_head + 1) % CHATTER_BUF_SIZE;
		if (cool_toggle_count < CHATTER_BUF_SIZE) {
			cool_toggle_count++;
		}
		last_cool_call = cool_call;
	}

	/* Count toggles within the last 60s */
	int recent_toggles = 0;
	for (int i = 0; i < cool_toggle_count; i++) {
		int idx = (cool_toggle_head - 1 - i + CHATTER_BUF_SIZE) % CHATTER_BUF_SIZE;
		if ((now - cool_toggle_times[idx]) <= CHATTER_WINDOW_MS) {
			recent_toggles++;
		}
	}

	if (recent_toggles > CHATTER_MAX_TOGGLES) {
		fault_flags |= FAULT_SENSOR;
	}
}

uint8_t selftest_get_fault_flags(void)
{
	return fault_flags;
}

/* ------------------------------------------------------------------ */
/*  Shell handler — on-demand commissioning self-test                  */
/* ------------------------------------------------------------------ */

int selftest_run_shell(void (*print)(const char *, ...),
		       void (*error)(const char *, ...))
{
	if (!platform || !print || !error) {
		return -1;
	}

	print("=== Self-Test ===");

	/* Run boot checks */
	selftest_boot_result_t result;
	selftest_boot(&result);

	print("  ADC pilot:     %s", result.adc_pilot_ok ? "PASS" : "FAIL");
	print("  GPIO cool:     %s", result.gpio_cool_ok ? "PASS" : "FAIL");
	print("  Charge block:  %s", result.charge_block_ok ? "PASS" : "FAIL");

	/* Snapshot cross-checks against current sensor readings */
	j1772_state_t state;
	uint16_t voltage_mv = 0;
	uint16_t current_ma = 0;
	bool cross_ok = true;

	int err = evse_j1772_state_get(&state, &voltage_mv);
	if (err) {
		print("  J1772 read:    FAIL (err=%d)", err);
		cross_ok = false;
	} else {
		print("  J1772 state:   %s (%d mV)", j1772_state_to_string(state), voltage_mv);
	}

	err = evse_current_read(&current_ma);
	if (err) {
		print("  Current read:  FAIL (err=%d)", err);
		cross_ok = false;
	} else {
		print("  Current:       %d mA", current_ma);
	}

	/* Instantaneous cross-checks */
	bool state_c = (state == J1772_STATE_C);
	bool current_on = (current_ma >= CURRENT_ON_THRESHOLD_MA);
	bool clamp_ok = !((state_c && !current_on) || (!state_c && current_on));
	print("  Clamp match:   %s", clamp_ok ? "PASS" : "WARN (mismatch)");
	if (!clamp_ok) {
		cross_ok = false;
	}

	if (!charge_control_is_allowed() && current_on) {
		print("  Interlock:     WARN (current while paused)");
		cross_ok = false;
	} else {
		print("  Interlock:     PASS");
	}

	/* Overall */
	bool all_ok = result.all_pass && cross_ok;
	print("  Fault flags:   0x%02x", fault_flags);
	print("=== %s ===", all_ok ? "ALL PASS" : "FAIL");

	if (!all_ok) {
		fault_flags |= FAULT_SELFTEST;
	}

	return all_ok ? 0 : -1;
}
