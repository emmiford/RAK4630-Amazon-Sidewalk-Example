/*
 * Host-side unit tests for the EVSE app modules.
 *
 * Tests the app code against a mock platform_api on the host machine.
 * This is the Grenning dual-target pattern: same app source, different
 * platform_api implementation (mock vs real hardware).
 *
 * Covers:
 * - evse_sensors.c: voltage-to-state classification
 * - charge_control.c: auto-resume timer
 * - thermostat_inputs.c: GPIO flag reading
 * - app_entry.c: on_timer() change detection and heartbeat
 * - app_tx.c: payload format and rate limiting
 */

#include "mock_platform.h"
#include <evse_sensors.h>
#include <charge_control.h>
#include <thermostat_inputs.h>
#include <app_tx.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* Access the app callback table (defined in app_entry.c) */
extern const struct app_callbacks app_cb;

static int tests_run;
static int tests_passed;

#define RUN_TEST(fn) do { \
	tests_run++; \
	printf("  %-60s", #fn); \
	fn(); \
	tests_passed++; \
	printf("PASS\n"); \
} while (0)

/* ================================================================== */
/*  evse_sensors: voltage-to-state classification                      */
/* ================================================================== */

static void test_j1772_state_a_high_voltage(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 2980;  /* ~12V after divider */
	evse_sensors_set_api(mock_api());

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_A);
	assert(mv == 2980);
}

static void test_j1772_state_b_connected(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 2234;  /* ~9V */
	evse_sensors_set_api(mock_api());

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_B);
}

static void test_j1772_state_c_charging(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 1489;  /* ~6V */
	evse_sensors_set_api(mock_api());

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_C);
}

static void test_j1772_state_d_ventilation(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 745;  /* ~3V */
	evse_sensors_set_api(mock_api());

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_D);
}

static void test_j1772_state_e_error(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 100;  /* ~0V */
	evse_sensors_set_api(mock_api());

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_E);
}

static void test_j1772_boundary_a_b(void)
{
	mock_reset();
	evse_sensors_set_api(mock_api());

	j1772_state_t state;
	uint16_t mv;

	/* Just above threshold -> A */
	mock_get()->adc_values[0] = 2601;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_A);

	/* At threshold -> B */
	mock_get()->adc_values[0] = 2600;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_B);
}

static void test_j1772_boundary_b_c(void)
{
	mock_reset();
	evse_sensors_set_api(mock_api());

	j1772_state_t state;
	uint16_t mv;

	mock_get()->adc_values[0] = 1851;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_B);

	mock_get()->adc_values[0] = 1850;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_C);
}

static void test_j1772_null_api_returns_error(void)
{
	evse_sensors_set_api(NULL);

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) != 0);
}

static void test_current_read_conversion(void)
{
	mock_reset();
	mock_get()->adc_values[1] = 1650;  /* half scale -> 15A */
	evse_sensors_set_api(mock_api());

	uint16_t current_ma;
	assert(evse_current_read(&current_ma) == 0);
	assert(current_ma == 15000);
}

static void test_current_read_zero(void)
{
	mock_reset();
	mock_get()->adc_values[1] = 0;
	evse_sensors_set_api(mock_api());

	uint16_t current_ma;
	assert(evse_current_read(&current_ma) == 0);
	assert(current_ma == 0);
}

static void test_simulation_overrides_adc(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 2980;  /* real = State A */
	mock_get()->uptime = 1000;
	evse_sensors_set_api(mock_api());

	/* Simulate State C for 10s */
	evse_sensors_simulate_state(J1772_STATE_C, 10000);

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_C);  /* simulated, not A */

	/* Expire the simulation */
	mock_get()->uptime = 12000;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_A);  /* back to real */
}

/* ================================================================== */
/*  thermostat_inputs: GPIO flag reading                               */
/* ================================================================== */

static void test_thermostat_no_calls(void)
{
	mock_reset();
	mock_get()->gpio_values[1] = 0;  /* heat off */
	mock_get()->gpio_values[2] = 0;  /* cool off */
	thermostat_inputs_set_api(mock_api());

	assert(thermostat_flags_get() == 0x00);
}

static void test_thermostat_heat_only(void)
{
	mock_reset();
	mock_get()->gpio_values[1] = 1;  /* heat on */
	mock_get()->gpio_values[2] = 0;  /* cool off */
	thermostat_inputs_set_api(mock_api());

	assert(thermostat_flags_get() == 0x01);
}

static void test_thermostat_cool_only(void)
{
	mock_reset();
	mock_get()->gpio_values[1] = 0;  /* heat off */
	mock_get()->gpio_values[2] = 1;  /* cool on */
	thermostat_inputs_set_api(mock_api());

	assert(thermostat_flags_get() == 0x02);
}

static void test_thermostat_both_calls(void)
{
	mock_reset();
	mock_get()->gpio_values[1] = 1;
	mock_get()->gpio_values[2] = 1;
	thermostat_inputs_set_api(mock_api());

	assert(thermostat_flags_get() == 0x03);
}

/* ================================================================== */
/*  charge_control: GPIO control and auto-resume                       */
/* ================================================================== */

static void test_charge_control_defaults_to_allowed(void)
{
	mock_reset();
	charge_control_set_api(mock_api());
	charge_control_init();

	assert(charge_control_is_allowed() == true);
	/* Init should set GPIO pin 0 high */
	assert(mock_get()->gpio_last_pin == 0);
	assert(mock_get()->gpio_last_val == 1);
}

static void test_charge_control_pause_sets_gpio_low(void)
{
	mock_reset();
	charge_control_set_api(mock_api());
	charge_control_init();

	charge_control_set(false, 0);
	assert(charge_control_is_allowed() == false);
	assert(mock_get()->gpio_last_val == 0);
}

static void test_charge_control_allow_sets_gpio_high(void)
{
	mock_reset();
	charge_control_set_api(mock_api());
	charge_control_init();

	charge_control_set(false, 0);
	charge_control_set(true, 0);
	assert(charge_control_is_allowed() == true);
	assert(mock_get()->gpio_last_val == 1);
}

static void test_charge_control_auto_resume(void)
{
	mock_reset();
	mock_get()->uptime = 10000;
	charge_control_set_api(mock_api());
	charge_control_init();

	/* Pause with 1-minute auto-resume */
	charge_control_set(false, 1);
	assert(charge_control_is_allowed() == false);

	/* Tick at 30s — still paused */
	mock_get()->uptime = 40000;
	charge_control_tick();
	assert(charge_control_is_allowed() == false);

	/* Tick at 61s — should auto-resume */
	mock_get()->uptime = 71000;
	charge_control_tick();
	assert(charge_control_is_allowed() == true);
	assert(mock_get()->gpio_last_val == 1);
}

static void test_charge_control_no_auto_resume_when_zero(void)
{
	mock_reset();
	mock_get()->uptime = 10000;
	charge_control_set_api(mock_api());
	charge_control_init();

	/* Pause without auto-resume */
	charge_control_set(false, 0);

	mock_get()->uptime = 1000000;  /* way later */
	charge_control_tick();
	assert(charge_control_is_allowed() == false);
}

/* ================================================================== */
/*  app_tx: payload format and rate limiting                           */
/* ================================================================== */

static void test_app_tx_sends_8_byte_payload(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 2980;  /* State A */
	mock_get()->adc_values[1] = 0;     /* 0 current */
	mock_get()->gpio_values[1] = 0;    /* no heat */
	mock_get()->gpio_values[2] = 0;    /* no cool */
	mock_get()->uptime = 10000;
	mock_get()->ready = true;

	evse_sensors_set_api(mock_api());
	thermostat_inputs_set_api(mock_api());
	app_tx_set_api(mock_api());
	app_tx_set_ready(true);

	int ret = app_tx_send_evse_data();
	assert(ret == 0);
	assert(mock_get()->send_count == 1);
	assert(mock_get()->sends[0].len == 8);

	/* Check magic and version bytes */
	assert(mock_get()->sends[0].data[0] == 0xE5);  /* EVSE_MAGIC */
	assert(mock_get()->sends[0].data[1] == 0x06);  /* EVSE_VERSION */
}

static void test_app_tx_rate_limits(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 2980;
	mock_get()->uptime = 100000;  /* well past any previous test's send */
	mock_get()->ready = true;

	evse_sensors_set_api(mock_api());
	thermostat_inputs_set_api(mock_api());
	app_tx_set_api(mock_api());
	app_tx_set_ready(true);

	/* First send succeeds */
	assert(app_tx_send_evse_data() == 0);
	assert(mock_get()->send_count == 1);

	/* Second send within 5s is rate-limited */
	mock_get()->uptime = 102000;
	assert(app_tx_send_evse_data() == 0);  /* returns 0 (rate-limited, not error) */
	assert(mock_get()->send_count == 1);   /* still 1 */

	/* After 5s, send works */
	mock_get()->uptime = 106000;
	assert(app_tx_send_evse_data() == 0);
	assert(mock_get()->send_count == 2);
}

static void test_app_tx_not_ready_skips(void)
{
	mock_reset();
	mock_get()->ready = false;

	app_tx_set_api(mock_api());
	app_tx_set_ready(false);

	assert(app_tx_send_evse_data() == -1);
	assert(mock_get()->send_count == 0);
}

/* ================================================================== */
/*  app_entry on_timer: change detection and heartbeat                 */
/* ================================================================== */

static uint32_t timer_test_base;

static void init_app_for_timer_tests(void)
{
	/* Use a high base uptime to avoid rate-limiter bleed from prior tests */
	timer_test_base = 500000;

	mock_reset();
	mock_get()->adc_values[0] = 2980;  /* State A */
	mock_get()->adc_values[1] = 0;     /* no current */
	mock_get()->gpio_values[1] = 0;    /* no heat */
	mock_get()->gpio_values[2] = 0;    /* no cool */
	mock_get()->uptime = timer_test_base;
	mock_get()->ready = true;

	app_cb.init(mock_api());
	/* Clear sends from init */
	mock_get()->send_count = 0;
}

static void test_on_timer_no_change_no_send(void)
{
	init_app_for_timer_tests();

	/* Tick with no changes and heartbeat not due */
	mock_get()->uptime = timer_test_base + 1000;
	app_cb.on_timer();
	assert(mock_get()->send_count == 0);
}

static void test_on_timer_j1772_change_triggers_send(void)
{
	init_app_for_timer_tests();

	/* Change J1772 from A to C */
	mock_get()->adc_values[0] = 1489;
	mock_get()->uptime = timer_test_base + 1000;
	app_cb.on_timer();
	assert(mock_get()->send_count == 1);
}

static void test_on_timer_current_change_triggers_send(void)
{
	init_app_for_timer_tests();

	/* Turn on current (above 500mA threshold) */
	mock_get()->adc_values[1] = 1650;  /* = 15000 mA */
	mock_get()->uptime = timer_test_base + 1000;
	app_cb.on_timer();
	assert(mock_get()->send_count == 1);
}

static void test_on_timer_thermostat_change_triggers_send(void)
{
	init_app_for_timer_tests();

	/* Turn on heat call */
	mock_get()->gpio_values[1] = 1;
	mock_get()->uptime = timer_test_base + 1000;
	app_cb.on_timer();
	assert(mock_get()->send_count == 1);
}

static void test_on_timer_heartbeat_sends_after_60s(void)
{
	init_app_for_timer_tests();

	/* No changes, but 60s passes */
	mock_get()->uptime = timer_test_base + 61000;
	app_cb.on_timer();
	assert(mock_get()->send_count == 1);
}

static void test_on_timer_no_heartbeat_before_60s(void)
{
	init_app_for_timer_tests();

	/* No changes, only 30s passed */
	mock_get()->uptime = timer_test_base + 30000;
	app_cb.on_timer();
	assert(mock_get()->send_count == 0);
}

static void test_on_timer_multiple_changes_one_send(void)
{
	init_app_for_timer_tests();

	/* Change J1772 AND thermostat in the same tick */
	mock_get()->adc_values[0] = 1489;  /* A -> C */
	mock_get()->gpio_values[1] = 1;    /* heat on */
	mock_get()->uptime = timer_test_base + 1000;
	app_cb.on_timer();
	assert(mock_get()->send_count == 1);  /* one send, not two */
}

static void test_on_timer_settled_after_change_no_send(void)
{
	init_app_for_timer_tests();

	/* First tick: change triggers send */
	mock_get()->adc_values[0] = 1489;
	mock_get()->uptime = timer_test_base + 1000;
	app_cb.on_timer();
	assert(mock_get()->send_count == 1);

	/* Second tick: same values, no change, no heartbeat */
	mock_get()->uptime = timer_test_base + 7000;  /* past rate limit but not heartbeat */
	app_cb.on_timer();
	assert(mock_get()->send_count == 1);  /* still 1 */
}

static void test_init_sets_timer_interval(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 2980;
	mock_get()->uptime = 900000;

	app_cb.init(mock_api());
	assert(mock_get()->timer_interval == 500);
}

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(void)
{
	printf("\n=== EVSE App Unit Tests ===\n\n");

	printf("evse_sensors:\n");
	RUN_TEST(test_j1772_state_a_high_voltage);
	RUN_TEST(test_j1772_state_b_connected);
	RUN_TEST(test_j1772_state_c_charging);
	RUN_TEST(test_j1772_state_d_ventilation);
	RUN_TEST(test_j1772_state_e_error);
	RUN_TEST(test_j1772_boundary_a_b);
	RUN_TEST(test_j1772_boundary_b_c);
	RUN_TEST(test_j1772_null_api_returns_error);
	RUN_TEST(test_current_read_conversion);
	RUN_TEST(test_current_read_zero);
	RUN_TEST(test_simulation_overrides_adc);

	printf("\nthermostat_inputs:\n");
	RUN_TEST(test_thermostat_no_calls);
	RUN_TEST(test_thermostat_heat_only);
	RUN_TEST(test_thermostat_cool_only);
	RUN_TEST(test_thermostat_both_calls);

	printf("\ncharge_control:\n");
	RUN_TEST(test_charge_control_defaults_to_allowed);
	RUN_TEST(test_charge_control_pause_sets_gpio_low);
	RUN_TEST(test_charge_control_allow_sets_gpio_high);
	RUN_TEST(test_charge_control_auto_resume);
	RUN_TEST(test_charge_control_no_auto_resume_when_zero);

	printf("\napp_tx:\n");
	RUN_TEST(test_app_tx_sends_8_byte_payload);
	RUN_TEST(test_app_tx_rate_limits);
	RUN_TEST(test_app_tx_not_ready_skips);

	printf("\non_timer change detection:\n");
	RUN_TEST(test_on_timer_no_change_no_send);
	RUN_TEST(test_on_timer_j1772_change_triggers_send);
	RUN_TEST(test_on_timer_current_change_triggers_send);
	RUN_TEST(test_on_timer_thermostat_change_triggers_send);
	RUN_TEST(test_on_timer_heartbeat_sends_after_60s);
	RUN_TEST(test_on_timer_no_heartbeat_before_60s);
	RUN_TEST(test_on_timer_multiple_changes_one_send);
	RUN_TEST(test_on_timer_settled_after_change_no_send);
	RUN_TEST(test_init_sets_timer_interval);

	printf("\n=== %d/%d tests passed ===\n\n", tests_passed, tests_run);
	return (tests_passed == tests_run) ? 0 : 1;
}
