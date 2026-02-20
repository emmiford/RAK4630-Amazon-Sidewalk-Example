/*
 * Unit tests for charge_control.c — relay control + auto-resume
 */

#include "unity.h"
#include "mock_platform_api.h"
#include "app_platform.h"
#include "charge_control.h"

void setUp(void)
{
	platform = mock_platform_api_init();
	charge_control_init();
}

void tearDown(void) {}

/* --- Init behavior --- */

void test_init_sets_gpio_low(void)
{
	TEST_ASSERT_EQUAL_INT(0, mock_gpio_set_last_pin);
	TEST_ASSERT_EQUAL_INT(0, mock_gpio_set_last_val);
}

void test_default_is_allowed(void)
{
	TEST_ASSERT_TRUE(charge_control_is_allowed());
}

/* --- Set allowed/paused --- */

void test_set_allowed_true(void)
{
	charge_control_set(false, 0);
	charge_control_set(true, 0);
	TEST_ASSERT_TRUE(charge_control_is_allowed());
	TEST_ASSERT_EQUAL_INT(0, mock_gpio_set_last_pin);
	TEST_ASSERT_EQUAL_INT(0, mock_gpio_set_last_val);
}

void test_set_paused(void)
{
	charge_control_set(false, 0);
	TEST_ASSERT_FALSE(charge_control_is_allowed());
	TEST_ASSERT_EQUAL_INT(0, mock_gpio_set_last_pin);
	TEST_ASSERT_EQUAL_INT(1, mock_gpio_set_last_val);
}

/* --- Auto-resume --- */

void test_pause_with_auto_resume(void)
{
	mock_uptime_ms = 10000;
	charge_control_set(false, 30);

	charge_control_state_t state;
	charge_control_get_state(&state);
	TEST_ASSERT_FALSE(state.charging_allowed);
	TEST_ASSERT_EQUAL_UINT16(30, state.auto_resume_min);
	TEST_ASSERT_EQUAL_INT64(10000, state.pause_timestamp_ms);
}

void test_auto_resume_fires(void)
{
	mock_uptime_ms = 10000;
	charge_control_set(false, 1); /* 1 minute auto-resume */

	/* Not yet — 59 seconds */
	mock_uptime_ms = 10000 + 59000;
	charge_control_tick();
	TEST_ASSERT_FALSE(charge_control_is_allowed());

	/* Now — 60 seconds */
	mock_uptime_ms = 10000 + 60000;
	charge_control_tick();
	TEST_ASSERT_TRUE(charge_control_is_allowed());
}

void test_auto_resume_not_yet(void)
{
	mock_uptime_ms = 10000;
	charge_control_set(false, 5); /* 5 minute auto-resume */

	mock_uptime_ms = 10000 + 120000; /* 2 minutes */
	charge_control_tick();
	TEST_ASSERT_FALSE(charge_control_is_allowed());
}

void test_tick_noop_when_allowed(void)
{
	/* Already allowed — tick should do nothing */
	int count_before = mock_gpio_set_call_count;
	charge_control_tick();
	TEST_ASSERT_EQUAL_INT(count_before, mock_gpio_set_call_count);
}

void test_tick_noop_without_resume(void)
{
	charge_control_set(false, 0); /* Paused, no auto-resume */
	int count_before = mock_gpio_set_call_count;

	mock_uptime_ms = 999999;
	charge_control_tick();
	TEST_ASSERT_FALSE(charge_control_is_allowed());
	/* gpio_set should not have been called again */
	TEST_ASSERT_EQUAL_INT(count_before, mock_gpio_set_call_count);
}

/* --- Process command --- */

void test_process_cmd_valid_allow(void)
{
	uint8_t cmd[] = {0x10, 1, 0, 0};
	TEST_ASSERT_EQUAL_INT(0, charge_control_process_cmd(cmd, sizeof(cmd)));
	TEST_ASSERT_TRUE(charge_control_is_allowed());
}

void test_process_cmd_valid_pause(void)
{
	/* duration_min=30 in little-endian */
	uint8_t cmd[] = {0x10, 0, 30, 0};
	TEST_ASSERT_EQUAL_INT(0, charge_control_process_cmd(cmd, sizeof(cmd)));
	TEST_ASSERT_FALSE(charge_control_is_allowed());

	charge_control_state_t state;
	charge_control_get_state(&state);
	TEST_ASSERT_EQUAL_UINT16(30, state.auto_resume_min);
}

void test_process_cmd_wrong_type(void)
{
	uint8_t cmd[] = {0x20, 1, 0, 0};
	TEST_ASSERT_EQUAL_INT(-1, charge_control_process_cmd(cmd, sizeof(cmd)));
}

void test_process_cmd_short_buf(void)
{
	uint8_t cmd[] = {0x10, 1};
	TEST_ASSERT_EQUAL_INT(-1, charge_control_process_cmd(cmd, 2));
}

void test_process_cmd_null_data(void)
{
	TEST_ASSERT_EQUAL_INT(-1, charge_control_process_cmd(NULL, 4));
}

/* --- main --- */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_init_sets_gpio_low);
	RUN_TEST(test_default_is_allowed);
	RUN_TEST(test_set_allowed_true);
	RUN_TEST(test_set_paused);
	RUN_TEST(test_pause_with_auto_resume);
	RUN_TEST(test_auto_resume_fires);
	RUN_TEST(test_auto_resume_not_yet);
	RUN_TEST(test_tick_noop_when_allowed);
	RUN_TEST(test_tick_noop_without_resume);
	RUN_TEST(test_process_cmd_valid_allow);
	RUN_TEST(test_process_cmd_valid_pause);
	RUN_TEST(test_process_cmd_wrong_type);
	RUN_TEST(test_process_cmd_short_buf);
	RUN_TEST(test_process_cmd_null_data);

	return UNITY_END();
}
