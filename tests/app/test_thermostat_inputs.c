/*
 * Unit tests for thermostat_inputs.c — GPIO input reading
 */

#include "unity.h"
#include "mock_platform_api.h"
#include "app_platform.h"
#include "thermostat_inputs.h"

void setUp(void)
{
	platform = mock_platform_api_init();
	thermostat_inputs_init();
}

void tearDown(void) {}

/* --- Individual calls --- */

void test_cool_call_high(void)
{
	mock_gpio_values[2] = 1;
	TEST_ASSERT_TRUE(thermostat_inputs_cool_call_get());
}

void test_cool_call_low(void)
{
	mock_gpio_values[2] = 0;
	TEST_ASSERT_FALSE(thermostat_inputs_cool_call_get());
}

/* --- Flags byte --- */

void test_flags_cool_only(void)
{
	mock_gpio_values[2] = 1;
	TEST_ASSERT_EQUAL_UINT8(THERMOSTAT_FLAG_COOL, thermostat_inputs_flags_get());
}

void test_flags_none(void)
{
	mock_gpio_values[2] = 0;
	TEST_ASSERT_EQUAL_UINT8(0x00, thermostat_inputs_flags_get());
}

/* --- main --- */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_cool_call_high);
	RUN_TEST(test_cool_call_low);
	RUN_TEST(test_flags_cool_only);
	RUN_TEST(test_flags_none);

	return UNITY_END();
}
