/*
 * Unit tests for app_rx.c — downlink message dispatch
 */

#include "unity.h"
#include "mock_platform_api.h"
#include "app_rx.h"
#include "charge_control.h"

static const struct platform_api *api;

void setUp(void)
{
	api = mock_platform_api_init();
	charge_control_set_api(api);
	charge_control_init();
	app_rx_set_api(api);
}

void tearDown(void) {}

/* --- Charge control dispatch --- */

void test_charge_cmd_dispatched(void)
{
	uint8_t cmd[] = {0x10, 1, 0, 0};
	app_rx_process_msg(cmd, sizeof(cmd));
	TEST_ASSERT_TRUE(charge_control_is_allowed());
}

void test_charge_allow(void)
{
	/* First pause, then allow */
	charge_control_set(false, 0);
	uint8_t cmd[] = {0x10, 1, 0, 0};
	app_rx_process_msg(cmd, sizeof(cmd));
	TEST_ASSERT_TRUE(charge_control_is_allowed());
}

void test_charge_pause(void)
{
	/* Pause with 30 min auto-resume (LE: 30,0) */
	uint8_t cmd[] = {0x10, 0, 30, 0};
	app_rx_process_msg(cmd, sizeof(cmd));
	TEST_ASSERT_FALSE(charge_control_is_allowed());

	charge_control_state_t state;
	charge_control_get_state(&state);
	TEST_ASSERT_EQUAL_UINT16(30, state.auto_resume_min);
}

void test_unknown_cmd_type_logged(void)
{
	uint8_t cmd[] = {0xFF, 0, 0, 0};
	app_rx_process_msg(cmd, sizeof(cmd));
	TEST_ASSERT_GREATER_THAN(0, mock_log_wrn_count);
}

/* --- Safety: NULL/zero inputs --- */

void test_null_data_safe(void)
{
	/* Should not crash */
	app_rx_process_msg(NULL, 4);
	TEST_ASSERT_EQUAL_INT(0, mock_send_count);
}

void test_zero_length_safe(void)
{
	uint8_t cmd[] = {0x10, 1, 0, 0};
	app_rx_process_msg(cmd, 0);
	/* Should not dispatch anything */
	TEST_ASSERT_EQUAL_INT(0, mock_send_count);
}

void test_short_charge_cmd_ignored(void)
{
	/* len=2, too short for charge_control_cmd_t (4 bytes) */
	charge_control_set(false, 0);
	uint8_t cmd[] = {0x10, 1};
	app_rx_process_msg(cmd, 2);
	/* Should NOT have dispatched — stays paused */
	TEST_ASSERT_FALSE(charge_control_is_allowed());
}

/* --- main --- */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_charge_cmd_dispatched);
	RUN_TEST(test_charge_allow);
	RUN_TEST(test_charge_pause);
	RUN_TEST(test_unknown_cmd_type_logged);
	RUN_TEST(test_null_data_safe);
	RUN_TEST(test_zero_length_safe);
	RUN_TEST(test_short_charge_cmd_ignored);

	return UNITY_END();
}
