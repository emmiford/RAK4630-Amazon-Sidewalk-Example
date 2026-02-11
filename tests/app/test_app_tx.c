/*
 * Unit tests for app_tx.c — payload formatting and rate-limited sending
 */

#include "unity.h"
#include "mock_platform_api.h"
#include "app_tx.h"
#include "evse_sensors.h"
#include "thermostat_inputs.h"
#include "rak_sidewalk.h"

static const struct platform_api *api;

void setUp(void)
{
	api = mock_platform_api_init();

	/* Wire up all modules that app_tx depends on */
	evse_sensors_set_api(api);
	evse_sensors_init();
	evse_sensors_simulate_state(0, 0);

	thermostat_inputs_set_api(api);
	thermostat_inputs_init();

	rak_sidewalk_set_api(api);

	app_tx_set_api(api);
	app_tx_set_ready(false);

	mock_sidewalk_ready = true;
}

void tearDown(void) {}

/* --- Payload format --- */

void test_send_encodes_magic_0xE5(void)
{
	TEST_ASSERT_EQUAL_INT(0, app_tx_send_evse_data());
	TEST_ASSERT_EQUAL_UINT8(0xE5, mock_last_send_buf[0]);
}

void test_send_encodes_version_0x06(void)
{
	app_tx_send_evse_data();
	TEST_ASSERT_EQUAL_UINT8(0x06, mock_last_send_buf[1]);
}

void test_send_8_bytes(void)
{
	app_tx_send_evse_data();
	TEST_ASSERT_EQUAL(8, mock_last_send_len);
}

void test_not_ready_skips(void)
{
	mock_sidewalk_ready = false;
	TEST_ASSERT_EQUAL_INT(-1, app_tx_send_evse_data());
	TEST_ASSERT_EQUAL_INT(0, mock_send_count);
}

/* --- Rate limiting --- */

void test_rate_limit_blocks(void)
{
	mock_uptime_ms = 1000;
	app_tx_send_evse_data();
	TEST_ASSERT_EQUAL_INT(1, mock_send_count);

	/* Within 5s, should be rate-limited */
	mock_uptime_ms = 4000;
	app_tx_send_evse_data();
	TEST_ASSERT_EQUAL_INT(1, mock_send_count);
}

void test_rate_limit_allows_after_interval(void)
{
	mock_uptime_ms = 1000;
	app_tx_send_evse_data();
	TEST_ASSERT_EQUAL_INT(1, mock_send_count);

	/* After 5s, should succeed */
	mock_uptime_ms = 6000;
	app_tx_send_evse_data();
	TEST_ASSERT_EQUAL_INT(2, mock_send_count);
}

/* --- Payload field encoding --- */

void test_j1772_state_at_byte2(void)
{
	/* Simulate state C */
	mock_uptime_ms = 100;
	evse_sensors_simulate_state(J1772_STATE_C, 10000);

	app_tx_send_evse_data();
	TEST_ASSERT_EQUAL_UINT8(J1772_STATE_C, mock_last_send_buf[2]);
}

void test_voltage_little_endian(void)
{
	/* Simulate state B (voltage 2234 = 0x08BA) */
	mock_uptime_ms = 100;
	evse_sensors_simulate_state(J1772_STATE_B, 10000);

	app_tx_send_evse_data();
	uint16_t voltage = mock_last_send_buf[3] | (mock_last_send_buf[4] << 8);
	TEST_ASSERT_EQUAL_UINT16(2234, voltage);
}

void test_current_little_endian(void)
{
	/* Set current clamp ADC: 1650mV = 15000mA */
	mock_adc_values[1] = 1650;

	/* Need non-simulated path for current to be read from ADC */
	mock_adc_values[0] = 3000; /* State A */

	app_tx_send_evse_data();
	uint16_t current = mock_last_send_buf[5] | (mock_last_send_buf[6] << 8);
	TEST_ASSERT_EQUAL_UINT16(15000, current);
}

void test_thermostat_flags_at_byte7(void)
{
	mock_gpio_values[1] = 1; /* heat */
	mock_gpio_values[2] = 1; /* cool */
	mock_adc_values[0] = 3000;

	app_tx_send_evse_data();
	TEST_ASSERT_EQUAL_UINT8(0x03, mock_last_send_buf[7]);
}

/* --- Edge cases --- */

void test_no_api_returns_error(void)
{
	app_tx_set_api(NULL);
	TEST_ASSERT_EQUAL_INT(-1, app_tx_send_evse_data());
	app_tx_set_api(api);
}

void test_set_ready_flag(void)
{
	app_tx_set_ready(true);
	TEST_ASSERT_TRUE(app_tx_is_ready());
	app_tx_set_ready(false);
	TEST_ASSERT_FALSE(app_tx_is_ready());
}

/* --- main --- */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_send_encodes_magic_0xE5);
	RUN_TEST(test_send_encodes_version_0x06);
	RUN_TEST(test_send_8_bytes);
	RUN_TEST(test_not_ready_skips);

	RUN_TEST(test_rate_limit_blocks);
	RUN_TEST(test_rate_limit_allows_after_interval);

	RUN_TEST(test_j1772_state_at_byte2);
	RUN_TEST(test_voltage_little_endian);
	RUN_TEST(test_current_little_endian);
	RUN_TEST(test_thermostat_flags_at_byte7);

	RUN_TEST(test_no_api_returns_error);
	RUN_TEST(test_set_ready_flag);

	return UNITY_END();
}
