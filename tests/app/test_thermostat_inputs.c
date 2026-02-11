/*
 * Unit tests for thermostat_inputs.c — GPIO input reading
 */

#include "unity.h"
#include "mock_platform_api.h"
#include "thermostat_inputs.h"

static const struct platform_api *api;

void setUp(void)
{
	api = mock_platform_api_init();
	thermostat_inputs_set_api(api);
	thermostat_inputs_init();
}

void tearDown(void) {}

/* --- Individual calls --- */

void test_heat_call_high(void)
{
	mock_gpio_values[1] = 1;
	TEST_ASSERT_TRUE(thermostat_heat_call_get());
}

void test_heat_call_low(void)
{
	mock_gpio_values[1] = 0;
	TEST_ASSERT_FALSE(thermostat_heat_call_get());
}

void test_cool_call_high(void)
{
	mock_gpio_values[2] = 1;
	TEST_ASSERT_TRUE(thermostat_cool_call_get());
}

void test_cool_call_low(void)
{
	mock_gpio_values[2] = 0;
	TEST_ASSERT_FALSE(thermostat_cool_call_get());
}

/* --- Flags byte --- */

void test_flags_both_on(void)
{
	mock_gpio_values[1] = 1;
	mock_gpio_values[2] = 1;
	TEST_ASSERT_EQUAL_UINT8(0x03, thermostat_flags_get());
}

void test_flags_heat_only(void)
{
	mock_gpio_values[1] = 1;
	mock_gpio_values[2] = 0;
	TEST_ASSERT_EQUAL_UINT8(THERMOSTAT_FLAG_HEAT, thermostat_flags_get());
}

void test_flags_cool_only(void)
{
	mock_gpio_values[1] = 0;
	mock_gpio_values[2] = 1;
	TEST_ASSERT_EQUAL_UINT8(THERMOSTAT_FLAG_COOL, thermostat_flags_get());
}

void test_flags_both_off(void)
{
	mock_gpio_values[1] = 0;
	mock_gpio_values[2] = 0;
	TEST_ASSERT_EQUAL_UINT8(0x00, thermostat_flags_get());
}

/* --- main --- */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_heat_call_high);
	RUN_TEST(test_heat_call_low);
	RUN_TEST(test_cool_call_high);
	RUN_TEST(test_cool_call_low);
	RUN_TEST(test_flags_both_on);
	RUN_TEST(test_flags_heat_only);
	RUN_TEST(test_flags_cool_only);
	RUN_TEST(test_flags_both_off);

	return UNITY_END();
}
