/*
 * Unit tests for selftest_trigger.c — 5-press button detection + LED blink codes
 */

#include "unity.h"
#include "mock_platform_api.h"
#include "selftest_trigger.h"
#include "selftest.h"
#include <string.h>

static const struct platform_api *api;
static int mock_send_called;

static int mock_send(void)
{
	mock_send_called++;
	return 0;
}

/* Helper: set all ADC/GPIO to make selftest_boot pass all 5 checks */
static void setup_all_pass(void)
{
	mock_adc_values[0] = 9000;  /* pilot ADC OK */
	mock_adc_values[1] = 100;   /* current ADC OK */
	mock_gpio_values[1] = 0;    /* heat GPIO readable */
	mock_gpio_values[2] = 0;    /* cool GPIO readable */
	/* charge_en (pin 0) passes because gpio_set now updates mock_gpio_values */
}

/* Helper: set ADC/GPIO so some checks fail */
static void setup_partial_fail(int adc0, int adc1, int gpio1, int gpio2)
{
	mock_adc_values[0] = adc0;
	mock_adc_values[1] = adc1;
	mock_gpio_values[1] = gpio1;
	mock_gpio_values[2] = gpio2;
}

/* Helper: simulate N button presses within a time window.
 * Stops immediately after a press triggers the selftest (no extra tick). */
static void simulate_presses(int count, uint32_t start_ms, uint32_t interval_ms)
{
	for (int i = 0; i < count; i++) {
		/* Button down */
		mock_uptime_ms = start_ms + (i * interval_ms * 2);
		mock_gpio_values[EVSE_PIN_BUTTON] = 1;
		selftest_trigger_tick();

		/* If selftest was triggered, release button and stop */
		if (selftest_trigger_is_running()) {
			mock_gpio_values[EVSE_PIN_BUTTON] = 0;
			return;
		}

		/* Button up */
		mock_uptime_ms = start_ms + (i * interval_ms * 2) + interval_ms;
		mock_gpio_values[EVSE_PIN_BUTTON] = 0;
		selftest_trigger_tick();
	}
}

/* Helper: run the blink state machine to completion, counting ticks */
static int run_blinks_to_completion(void)
{
	int ticks = 0;
	int max_ticks = 100;
	while (selftest_trigger_is_running() && ticks < max_ticks) {
		selftest_trigger_tick();
		ticks++;
	}
	return ticks;
}

void setUp(void)
{
	api = mock_platform_api_init();
	selftest_set_api(api);
	selftest_reset();
	selftest_trigger_set_api(api);
	selftest_trigger_set_send_fn(mock_send);
	selftest_trigger_init();
	mock_send_called = 0;
	setup_all_pass();
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */
/*  Idle state                                                         */
/* ------------------------------------------------------------------ */

void test_init_state_idle(void)
{
	TEST_ASSERT_FALSE(selftest_trigger_is_running());
}

/* ------------------------------------------------------------------ */
/*  Button press detection                                             */
/* ------------------------------------------------------------------ */

void test_single_press_no_trigger(void)
{
	simulate_presses(1, 1000, 200);
	TEST_ASSERT_FALSE(selftest_trigger_is_running());
}

void test_four_presses_no_trigger(void)
{
	simulate_presses(4, 1000, 200);
	TEST_ASSERT_FALSE(selftest_trigger_is_running());
}

void test_five_presses_triggers(void)
{
	simulate_presses(5, 1000, 200);
	TEST_ASSERT_TRUE(selftest_trigger_is_running());
}

void test_five_presses_outside_window_no_trigger(void)
{
	/* 5 presses spread across > 5 seconds */
	simulate_presses(5, 1000, 2000);
	TEST_ASSERT_FALSE(selftest_trigger_is_running());
}

void test_old_presses_expire(void)
{
	/* 3 presses at t=1000 */
	simulate_presses(3, 1000, 200);
	TEST_ASSERT_FALSE(selftest_trigger_is_running());

	/* Wait for them to expire (> 5s later), then 5 new presses */
	simulate_presses(5, 10000, 200);
	TEST_ASSERT_TRUE(selftest_trigger_is_running());
}

void test_button_ignored_while_running(void)
{
	/* Trigger the selftest */
	simulate_presses(5, 1000, 200);
	TEST_ASSERT_TRUE(selftest_trigger_is_running());

	/* More presses while running — should be ignored (still blinking) */
	mock_gpio_values[EVSE_PIN_BUTTON] = 1;
	selftest_trigger_tick();
	mock_gpio_values[EVSE_PIN_BUTTON] = 0;
	selftest_trigger_tick();

	/* Should still be in blinking state (not restarted) */
	TEST_ASSERT_TRUE(selftest_trigger_is_running());
}

/* ------------------------------------------------------------------ */
/*  Blink codes — all pass (5 green, 0 red)                           */
/* ------------------------------------------------------------------ */

void test_blink_all_pass_green_count(void)
{
	simulate_presses(5, 1000, 200);
	TEST_ASSERT_TRUE(selftest_trigger_is_running());

	/* Snapshot LED counts after trigger (excludes selftest_boot LED flash) */
	int green_start = mock_led_on_count[LED_GREEN];
	int red_start = mock_led_on_count[LED_RED];

	int ticks = run_blinks_to_completion();
	TEST_ASSERT_FALSE(selftest_trigger_is_running());

	/* 5 passed = 5 green blinks (5 LED-on events) */
	TEST_ASSERT_EQUAL_INT(5, mock_led_on_count[LED_GREEN] - green_start);
	/* 0 failed = 0 red blinks */
	TEST_ASSERT_EQUAL_INT(0, mock_led_on_count[LED_RED] - red_start);

	/* Total ticks: 5*2 (green on+off) = 10 ticks + 1 done tick */
	TEST_ASSERT_EQUAL_INT(11, ticks);
}

void test_all_pass_no_uplink(void)
{
	simulate_presses(5, 1000, 200);
	run_blinks_to_completion();
	TEST_ASSERT_EQUAL_INT(0, mock_send_called);
}

/* ------------------------------------------------------------------ */
/*  Blink codes — max fail (1 green, 4 red)                           */
/*  Note: charge_en toggle always passes because gpio_set updates      */
/*  readable value. So 4 of 5 is the maximum failure count.            */
/* ------------------------------------------------------------------ */

void test_blink_max_fail_counts(void)
{
	setup_partial_fail(-1, -1, -1, -1);

	simulate_presses(5, 1000, 200);
	TEST_ASSERT_TRUE(selftest_trigger_is_running());

	int green_start = mock_led_on_count[LED_GREEN];
	int red_start = mock_led_on_count[LED_RED];

	int ticks = run_blinks_to_completion();
	TEST_ASSERT_FALSE(selftest_trigger_is_running());

	/* 1 passed (charge_en), 4 failed (pilot, current, heat, cool) */
	TEST_ASSERT_EQUAL_INT(1, mock_led_on_count[LED_GREEN] - green_start);
	TEST_ASSERT_EQUAL_INT(4, mock_led_on_count[LED_RED] - red_start);

	/* Total: 1*2 (green) + 2 (pause) + 4*2 (red) = 12 + 1 done = 13 */
	TEST_ASSERT_EQUAL_INT(13, ticks);
}

void test_failures_send_uplink(void)
{
	setup_partial_fail(-1, -1, -1, -1);
	simulate_presses(5, 1000, 200);
	run_blinks_to_completion();
	TEST_ASSERT_EQUAL_INT(1, mock_send_called);
}

/* ------------------------------------------------------------------ */
/*  Blink codes — mixed (3 pass, 2 fail)                              */
/* ------------------------------------------------------------------ */

void test_blink_mixed_counts(void)
{
	/* 3 pass (pilot, heat, charge_en), 2 fail (current, cool) */
	mock_adc_values[0] = 9000;  /* pilot OK */
	mock_adc_values[1] = -1;    /* current FAIL */
	mock_gpio_values[1] = 0;    /* heat OK */
	mock_gpio_values[2] = -1;   /* cool FAIL */

	simulate_presses(5, 1000, 200);
	TEST_ASSERT_TRUE(selftest_trigger_is_running());

	int green_start = mock_led_on_count[LED_GREEN];
	int red_start = mock_led_on_count[LED_RED];

	int ticks = run_blinks_to_completion();
	TEST_ASSERT_FALSE(selftest_trigger_is_running());

	/* 3 passed (pilot, heat, charge_en), 2 failed (current, cool) */
	TEST_ASSERT_EQUAL_INT(3, mock_led_on_count[LED_GREEN] - green_start);
	TEST_ASSERT_EQUAL_INT(2, mock_led_on_count[LED_RED] - red_start);

	/* Total ticks: 3*2 (green) + 2 (pause) + 2*2 (red) = 12 + 1 done = 13 */
	TEST_ASSERT_EQUAL_INT(13, ticks);
}

void test_mixed_sends_uplink(void)
{
	mock_adc_values[1] = -1;    /* current FAIL */
	mock_gpio_values[2] = -1;   /* cool FAIL */

	simulate_presses(5, 1000, 200);
	run_blinks_to_completion();
	TEST_ASSERT_EQUAL_INT(1, mock_send_called);
}

/* ------------------------------------------------------------------ */
/*  Blink LED state transitions                                        */
/* ------------------------------------------------------------------ */

void test_green_blink_led_on_off_pattern(void)
{
	simulate_presses(5, 1000, 200);

	/* First blink tick: green ON */
	selftest_trigger_tick();
	TEST_ASSERT_TRUE(mock_led_states[LED_GREEN]);

	/* Second blink tick: green OFF */
	selftest_trigger_tick();
	TEST_ASSERT_FALSE(mock_led_states[LED_GREEN]);

	/* Third blink tick: green ON (blink 2) */
	selftest_trigger_tick();
	TEST_ASSERT_TRUE(mock_led_states[LED_GREEN]);
}

void test_leds_off_after_completion(void)
{
	simulate_presses(5, 1000, 200);
	run_blinks_to_completion();

	TEST_ASSERT_FALSE(mock_led_states[LED_GREEN]);
	TEST_ASSERT_FALSE(mock_led_states[LED_RED]);
}

/* ------------------------------------------------------------------ */
/*  Returns to idle after completion                                   */
/* ------------------------------------------------------------------ */

void test_returns_to_idle_after_completion(void)
{
	simulate_presses(5, 1000, 200);
	run_blinks_to_completion();
	TEST_ASSERT_FALSE(selftest_trigger_is_running());

	/* One idle tick with button released to reset edge detection */
	mock_gpio_values[EVSE_PIN_BUTTON] = 0;
	selftest_trigger_tick();

	/* Can trigger again */
	simulate_presses(5, 20000, 200);
	TEST_ASSERT_TRUE(selftest_trigger_is_running());
}

/* ------------------------------------------------------------------ */
/*  Edge case: no send callback set                                    */
/* ------------------------------------------------------------------ */

void test_no_send_fn_does_not_crash(void)
{
	selftest_trigger_set_send_fn(NULL);
	setup_partial_fail(-1, -1, -1, -1);

	simulate_presses(5, 1000, 200);
	run_blinks_to_completion();

	/* Should complete without crashing, no send */
	TEST_ASSERT_FALSE(selftest_trigger_is_running());
	TEST_ASSERT_EQUAL_INT(0, mock_send_called);
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
	UNITY_BEGIN();

	/* Idle state */
	RUN_TEST(test_init_state_idle);

	/* Button detection */
	RUN_TEST(test_single_press_no_trigger);
	RUN_TEST(test_four_presses_no_trigger);
	RUN_TEST(test_five_presses_triggers);
	RUN_TEST(test_five_presses_outside_window_no_trigger);
	RUN_TEST(test_old_presses_expire);
	RUN_TEST(test_button_ignored_while_running);

	/* Blink codes */
	RUN_TEST(test_blink_all_pass_green_count);
	RUN_TEST(test_all_pass_no_uplink);
	RUN_TEST(test_blink_max_fail_counts);
	RUN_TEST(test_failures_send_uplink);
	RUN_TEST(test_blink_mixed_counts);
	RUN_TEST(test_mixed_sends_uplink);

	/* LED patterns */
	RUN_TEST(test_green_blink_led_on_off_pattern);
	RUN_TEST(test_leds_off_after_completion);

	/* Lifecycle */
	RUN_TEST(test_returns_to_idle_after_completion);
	RUN_TEST(test_no_send_fn_does_not_crash);

	return UNITY_END();
}
