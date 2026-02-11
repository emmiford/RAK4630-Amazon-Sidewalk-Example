/*
 * Unit tests for evse_sensors.c — J1772 state machine and current clamp
 */

#include "unity.h"
#include "mock_platform_api.h"
#include "evse_sensors.h"

static const struct platform_api *api;

void setUp(void)
{
	api = mock_platform_api_init();
	evse_sensors_set_api(api);
	evse_sensors_init();
	/* Cancel any leftover simulation */
	evse_sensors_simulate_state(0, 0);
}

void tearDown(void) {}

/* --- J1772 state classification --- */

void test_j1772_state_A_above_2600mv(void)
{
	mock_adc_values[0] = 3000;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_A, state);
}

void test_j1772_state_B_1850_to_2600(void)
{
	mock_adc_values[0] = 2200;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_B, state);
}

void test_j1772_state_C_1100_to_1850(void)
{
	mock_adc_values[0] = 1500;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_C, state);
}

void test_j1772_state_D_350_to_1100(void)
{
	mock_adc_values[0] = 700;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_D, state);
}

void test_j1772_state_E_below_350(void)
{
	mock_adc_values[0] = 100;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_E, state);
}

/* --- Boundary tests (thresholds use >) --- */

void test_j1772_boundary_A_B_exact_2600(void)
{
	mock_adc_values[0] = 2600;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_B, state);
}

void test_j1772_boundary_B_C_exact_1850(void)
{
	mock_adc_values[0] = 1850;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_C, state);
}

void test_j1772_boundary_C_D_exact_1100(void)
{
	mock_adc_values[0] = 1100;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_D, state);
}

void test_j1772_boundary_D_E_exact_350(void)
{
	mock_adc_values[0] = 350;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_E, state);
}

/* --- Error handling --- */

void test_pilot_voltage_null_output(void)
{
	TEST_ASSERT_EQUAL_INT(-1, evse_pilot_voltage_read(NULL));
}

void test_pilot_voltage_no_api(void)
{
	evse_sensors_set_api(NULL);
	uint16_t mv;
	TEST_ASSERT_EQUAL_INT(-1, evse_pilot_voltage_read(&mv));
	evse_sensors_set_api(api);
}

void test_adc_error_propagated(void)
{
	mock_adc_values[0] = -5;
	uint16_t mv;
	TEST_ASSERT_EQUAL_INT(-5, evse_pilot_voltage_read(&mv));
}

/* --- Current clamp --- */

void test_current_clamp_zero(void)
{
	mock_adc_values[1] = 0;
	uint16_t current_ma;
	TEST_ASSERT_EQUAL_INT(0, evse_current_read(&current_ma));
	TEST_ASSERT_EQUAL_UINT16(0, current_ma);
}

void test_current_clamp_midscale(void)
{
	mock_adc_values[1] = 1650;
	uint16_t current_ma;
	TEST_ASSERT_EQUAL_INT(0, evse_current_read(&current_ma));
	TEST_ASSERT_EQUAL_UINT16(15000, current_ma);
}

void test_current_clamp_fullscale(void)
{
	mock_adc_values[1] = 3300;
	uint16_t current_ma;
	TEST_ASSERT_EQUAL_INT(0, evse_current_read(&current_ma));
	TEST_ASSERT_EQUAL_UINT16(30000, current_ma);
}

/* --- Simulation mode --- */

void test_simulation_overrides_adc(void)
{
	mock_adc_values[0] = 3000; /* Would be state A */
	mock_uptime_ms = 1000;
	evse_sensors_simulate_state(J1772_STATE_B, 10000);

	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_B, state);
}

void test_simulation_expiry(void)
{
	mock_uptime_ms = 1000;
	evse_sensors_simulate_state(J1772_STATE_C, 5000);

	/* Still simulating */
	mock_uptime_ms = 5999;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_C, state);

	/* Expired — should read real ADC */
	mock_uptime_ms = 6000;
	mock_adc_values[0] = 3000;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_A, state);
}

void test_simulation_cancel(void)
{
	mock_uptime_ms = 1000;
	evse_sensors_simulate_state(J1772_STATE_C, 10000);
	TEST_ASSERT_TRUE(evse_sensors_is_simulating());

	evse_sensors_simulate_state(0, 0);
	TEST_ASSERT_FALSE(evse_sensors_is_simulating());

	/* Should read real ADC */
	mock_adc_values[0] = 3000;
	j1772_state_t state;
	TEST_ASSERT_EQUAL_INT(0, evse_j1772_state_get(&state, NULL));
	TEST_ASSERT_EQUAL(J1772_STATE_A, state);
}

/* --- main --- */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_j1772_state_A_above_2600mv);
	RUN_TEST(test_j1772_state_B_1850_to_2600);
	RUN_TEST(test_j1772_state_C_1100_to_1850);
	RUN_TEST(test_j1772_state_D_350_to_1100);
	RUN_TEST(test_j1772_state_E_below_350);

	RUN_TEST(test_j1772_boundary_A_B_exact_2600);
	RUN_TEST(test_j1772_boundary_B_C_exact_1850);
	RUN_TEST(test_j1772_boundary_C_D_exact_1100);
	RUN_TEST(test_j1772_boundary_D_E_exact_350);

	RUN_TEST(test_pilot_voltage_null_output);
	RUN_TEST(test_pilot_voltage_no_api);
	RUN_TEST(test_adc_error_propagated);

	RUN_TEST(test_current_clamp_zero);
	RUN_TEST(test_current_clamp_midscale);
	RUN_TEST(test_current_clamp_fullscale);

	RUN_TEST(test_simulation_overrides_adc);
	RUN_TEST(test_simulation_expiry);
	RUN_TEST(test_simulation_cancel);

	return UNITY_END();
}
