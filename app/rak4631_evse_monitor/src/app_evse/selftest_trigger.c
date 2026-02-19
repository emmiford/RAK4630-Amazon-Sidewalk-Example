/*
 * Button Event Handler + Self-Test Trigger
 *
 * Polls the Charge Now button GPIO every 500ms tick.  Dispatches:
 *   - Single press (1 press, 1.5s timeout) → charge_now_activate()
 *   - Long press (held 3s) → charge_now_cancel()
 *   - 5 presses within 5s → self-test with LED blink codes
 *
 * Each blink = one tick ON (500ms) + one tick OFF (500ms) = 1 blink/sec.
 * Green blinks = passed count, pause, red blinks = failed count.
 */

#include <selftest_trigger.h>
#include <selftest.h>
#include <charge_now.h>
#include <app_platform.h>
#include <string.h>

static selftest_send_fn send_fn;

/* ------------------------------------------------------------------ */
/*  Button press detection                                             */
/* ------------------------------------------------------------------ */

static uint32_t press_times[TRIGGER_PRESS_COUNT];
static int press_count;
static bool last_button_pressed;

/* Single-press detection */
static bool single_press_pending;
static uint32_t single_press_time;

/* Long-press detection */
static uint32_t button_held_since;
static bool tracking_hold;
static bool long_press_fired;

/* ------------------------------------------------------------------ */
/*  Blink state machine                                                */
/* ------------------------------------------------------------------ */

typedef enum {
	TRIG_IDLE,
	TRIG_BLINKING,
} trigger_state_t;

static trigger_state_t state;
static int blink_tick;
static int passed_count;
static int failed_count;
static int green_end_tick;
static int red_start_tick;
static int total_blink_ticks;
static bool send_pending;

void selftest_trigger_set_send_fn(selftest_send_fn fn)
{
	send_fn = fn;
}

void selftest_trigger_init(void)
{
	state = TRIG_IDLE;
	press_count = 0;
	last_button_pressed = false;
	blink_tick = 0;
	passed_count = 0;
	failed_count = 0;
	green_end_tick = 0;
	red_start_tick = 0;
	total_blink_ticks = 0;
	send_pending = false;

	single_press_pending = false;
	single_press_time = 0;
	button_held_since = 0;
	tracking_hold = false;
	long_press_fired = false;
}

bool selftest_trigger_is_running(void)
{
	return state != TRIG_IDLE;
}

/* ------------------------------------------------------------------ */
/*  Self-test execution                                                */
/* ------------------------------------------------------------------ */

static void start_selftest(void)
{
	selftest_boot_result_t result;
	selftest_boot(&result);

	passed_count = 0;
	if (result.adc_pilot_ok)   passed_count++;
	if (result.adc_current_ok) passed_count++;
	if (result.gpio_cool_ok)   passed_count++;
	if (result.charge_en_ok)   passed_count++;
	failed_count = SELFTEST_CHECK_COUNT - passed_count;

	/* Compute blink sequence tick layout:
	 * Green: passed_count * 2 ticks (on + off per blink)
	 * Pause: BLINK_PAUSE_TICKS (only if both green and red needed)
	 * Red:   failed_count * 2 ticks (on + off per blink)
	 */
	green_end_tick = passed_count * 2;

	if (passed_count == 0) {
		/* No green blinks — skip straight to red */
		red_start_tick = 0;
		total_blink_ticks = failed_count * 2;
	} else if (failed_count == 0) {
		/* No red blinks — green only, no pause */
		red_start_tick = green_end_tick;
		total_blink_ticks = green_end_tick;
	} else {
		/* Both green and red — insert pause */
		red_start_tick = green_end_tick + BLINK_PAUSE_TICKS;
		total_blink_ticks = red_start_tick + failed_count * 2;
	}

	send_pending = (failed_count > 0);
	blink_tick = 0;
	state = TRIG_BLINKING;

	if (platform) {
		platform->log_inf("Self-test triggered: %d pass, %d fail",
			     passed_count, failed_count);
	}
}

/* ------------------------------------------------------------------ */
/*  Button polling                                                     */
/* ------------------------------------------------------------------ */

static void poll_button(void)
{
	if (!platform) {
		return;
	}

	bool pressed = (platform->gpio_get(EVSE_PIN_BUTTON) == 1);
	uint32_t now = platform->uptime_ms();

	/* Rising edge — new press */
	if (pressed && !last_button_pressed) {
		/* Expire old presses outside window */
		while (press_count > 0 &&
		       (now - press_times[0]) > TRIGGER_WINDOW_MS) {
			memmove(press_times, press_times + 1,
				(press_count - 1) * sizeof(uint32_t));
			press_count--;
		}

		/* Record new press */
		if (press_count < TRIGGER_PRESS_COUNT) {
			press_times[press_count++] = now;
		}

		/* Check 5-press trigger */
		if (press_count >= TRIGGER_PRESS_COUNT) {
			press_count = 0;
			single_press_pending = false;
			start_selftest();
		} else if (press_count == 1) {
			/* First press — start single-press timer */
			single_press_pending = true;
			single_press_time = now;
		} else {
			/* 2-4 presses — not a single press */
			single_press_pending = false;
		}

		/* Start long-press tracking */
		button_held_since = now;
		tracking_hold = true;
		long_press_fired = false;
	}

	/* Falling edge — button released */
	if (!pressed && last_button_pressed) {
		tracking_hold = false;
	}

	/* Long press check: held continuously for 3s */
	if (pressed && tracking_hold && !long_press_fired) {
		if ((now - button_held_since) >= LONG_PRESS_MS) {
			long_press_fired = true;
			single_press_pending = false;
			press_count = 0;
			if (charge_now_is_active()) {
				charge_now_cancel();
			}
		}
	}

	/* Single press timeout: 1.5s after press with button released */
	if (single_press_pending && !pressed) {
		if ((now - single_press_time) >= SINGLE_PRESS_TIMEOUT_MS) {
			single_press_pending = false;
			press_count = 0;
			charge_now_activate();
		}
	}

	last_button_pressed = pressed;
}

/* ------------------------------------------------------------------ */
/*  LED blink driver                                                   */
/* ------------------------------------------------------------------ */

static void drive_blinks(void)
{
	if (!platform) {
		return;
	}

	if (blink_tick < green_end_tick) {
		/* Green phase: even ticks = ON, odd ticks = OFF */
		bool on = (blink_tick % 2 == 0);
		platform->led_set(LED_GREEN, on);
	} else if (blink_tick < red_start_tick) {
		/* Pause phase: all LEDs off */
		platform->led_set(LED_GREEN, false);
		platform->led_set(LED_RED, false);
	} else if (blink_tick < total_blink_ticks) {
		/* Red phase: even offset ticks = ON, odd = OFF */
		int rt = blink_tick - red_start_tick;
		bool on = (rt % 2 == 0);
		platform->led_set(LED_RED, on);
	} else {
		/* Done */
		platform->led_set(LED_GREEN, false);
		platform->led_set(LED_RED, false);

		if (send_pending && send_fn) {
			send_fn();
			send_pending = false;
		}

		state = TRIG_IDLE;
		return;
	}

	blink_tick++;
}

/* ------------------------------------------------------------------ */
/*  Tick handler — call from app_on_timer every 500ms                  */
/* ------------------------------------------------------------------ */

void selftest_trigger_tick(void)
{
	if (!platform) {
		return;
	}

	switch (state) {
	case TRIG_IDLE:
		poll_button();
		break;
	case TRIG_BLINKING:
		drive_blinks();
		break;
	}
}
