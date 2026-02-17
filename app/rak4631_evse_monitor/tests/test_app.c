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
#include <selftest.h>
#include <diag_request.h>
#include <app_tx.h>
#include <time_sync.h>
#include <event_buffer.h>
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
	mock_get()->gpio_values[2] = 0;  /* cool off */
	thermostat_inputs_set_api(mock_api());

	assert(thermostat_flags_get() == 0x00);
}

static void test_thermostat_cool_only(void)
{
	mock_reset();
	mock_get()->gpio_values[2] = 1;  /* cool on */
	thermostat_inputs_set_api(mock_api());

	assert(thermostat_flags_get() == 0x02);
}

static void test_thermostat_both_calls(void)
{
	mock_reset();
	mock_get()->gpio_values[2] = 1;
	thermostat_inputs_set_api(mock_api());

	assert(thermostat_flags_get() == 0x02);
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

static void test_app_tx_sends_12_byte_payload(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 2980;  /* State A */
	mock_get()->adc_values[1] = 0;     /* 0 current */
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
	assert(mock_get()->sends[0].len == 12);

	/* Check magic and version bytes */
	assert(mock_get()->sends[0].data[0] == 0xE5);  /* EVSE_MAGIC */
	assert(mock_get()->sends[0].data[1] == 0x08);  /* EVSE_VERSION v0x08 */
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

	/* Turn on cool call */
	mock_get()->gpio_values[2] = 1;
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
	mock_get()->gpio_values[2] = 1;    /* cool on */
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
/*  selftest_boot: hardware path checks                                */
/* ================================================================== */

static void init_selftest(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 2980;  /* pilot OK (State A) */
	mock_get()->adc_values[1] = 0;     /* current OK (0 mA, consistent with State A) */
	mock_get()->gpio_values[0] = 1;    /* charge enable */
	mock_get()->gpio_values[2] = 0;    /* cool */
	mock_get()->uptime = 1000000;      /* high base */
	selftest_set_api(mock_api());
	selftest_reset();
}

static void test_selftest_boot_all_pass(void)
{
	init_selftest();
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == 0);
	assert(result.adc_pilot_ok == true);
	assert(result.adc_current_ok == true);
	assert(result.gpio_cool_ok == true);
	assert(result.charge_en_ok == true);
	assert(result.all_pass == true);
	assert((selftest_get_fault_flags() & FAULT_SELFTEST) == 0);
}

static void test_selftest_boot_adc_pilot_fail(void)
{
	init_selftest();
	mock_get()->adc_fail[0] = true;
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == -1);
	assert(result.adc_pilot_ok == false);
	assert(result.all_pass == false);
	assert(selftest_get_fault_flags() & FAULT_SELFTEST);
}

static void test_selftest_boot_adc_current_fail(void)
{
	init_selftest();
	mock_get()->adc_fail[1] = true;
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == -1);
	assert(result.adc_current_ok == false);
	assert(result.all_pass == false);
}

static void test_selftest_boot_gpio_cool_fail(void)
{
	init_selftest();
	mock_get()->gpio_fail[2] = true;
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == -1);
	assert(result.gpio_cool_ok == false);
	assert(result.all_pass == false);
}

static void test_selftest_boot_charge_en_toggle_pass(void)
{
	init_selftest();
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == 0);
	assert(result.charge_en_ok == true);
}

static void test_selftest_boot_charge_en_readback_fail(void)
{
	init_selftest();
	mock_get()->gpio_readback_fail[0] = true;
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == -1);
	assert(result.charge_en_ok == false);
	assert(result.all_pass == false);
}

static void test_selftest_boot_flag_latched(void)
{
	/* After boot failure, FAULT_SELFTEST is latched */
	init_selftest();
	mock_get()->adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);
	assert(selftest_get_fault_flags() & FAULT_SELFTEST);

	/* Running boot again with everything OK still has SELFTEST set (latched) */
	mock_get()->adc_fail[0] = false;
	selftest_boot(&result);
	/* Second boot passes, so FAULT_SELFTEST gets cleared in the "all_pass" path.
	 * Actually per plan: SELFTEST_FAIL is latched until reboot. But selftest_boot
	 * only sets it on failure, doesn't clear it. So it stays latched. */
	assert(selftest_get_fault_flags() & FAULT_SELFTEST);
}

static void test_selftest_boot_led_flash_on_failure(void)
{
	init_selftest();
	mock_get()->adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);
	/* Should have called led_set at least twice (on + off) */
	assert(mock_get()->led_set_count >= 2);
	assert(mock_get()->led_last_id == 2);
}

/* ================================================================== */
/*  selftest_continuous: runtime fault monitoring                       */
/* ================================================================== */

static void test_continuous_clamp_mismatch_state_c_no_current(void)
{
	init_selftest();
	/* State C (charging) but no current — mismatch */
	/* Before 10s: no fault */
	selftest_continuous_tick(2, 1489, 0, true, false);  /* t=1000000 */
	assert((selftest_get_fault_flags() & FAULT_CLAMP) == 0);

	/* Tick at +9s: still no fault */
	mock_get()->uptime = 1009000;
	selftest_continuous_tick(2, 1489, 0, true, false);
	assert((selftest_get_fault_flags() & FAULT_CLAMP) == 0);

	/* Tick at +10s: fault triggers */
	mock_get()->uptime = 1010000;
	selftest_continuous_tick(2, 1489, 0, true, false);
	assert(selftest_get_fault_flags() & FAULT_CLAMP);
}

static void test_continuous_clamp_mismatch_not_c_with_current(void)
{
	init_selftest();
	/* State A (idle) but current flowing — mismatch */
	mock_get()->uptime = 2000000;
	selftest_continuous_tick(0, 2980, 5000, true, false);
	assert((selftest_get_fault_flags() & FAULT_CLAMP) == 0);

	/* After 10s */
	mock_get()->uptime = 2010000;
	selftest_continuous_tick(0, 2980, 5000, true, false);
	assert(selftest_get_fault_flags() & FAULT_CLAMP);
}

static void test_continuous_clamp_mismatch_clears_on_resolve(void)
{
	init_selftest();
	/* Trigger mismatch */
	mock_get()->uptime = 3000000;
	selftest_continuous_tick(2, 1489, 0, true, false);
	mock_get()->uptime = 3010000;
	selftest_continuous_tick(2, 1489, 0, true, false);
	assert(selftest_get_fault_flags() & FAULT_CLAMP);

	/* Resolve: State C with current */
	mock_get()->uptime = 3011000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	assert((selftest_get_fault_flags() & FAULT_CLAMP) == 0);
}

static void test_continuous_normal_operation_no_fault(void)
{
	init_selftest();
	/* State C + current = normal */
	mock_get()->uptime = 4000000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	mock_get()->uptime = 4010000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	assert((selftest_get_fault_flags() & FAULT_CLAMP) == 0);
}

static void test_continuous_interlock_current_after_pause(void)
{
	init_selftest();
	/* Start allowed */
	mock_get()->uptime = 5000000;
	selftest_continuous_tick(2, 1489, 5000, true, false);

	/* Transition to paused, but current persists */
	mock_get()->uptime = 5001000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert((selftest_get_fault_flags() & FAULT_INTERLOCK) == 0);

	/* Before 30s: no fault */
	mock_get()->uptime = 5029000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert((selftest_get_fault_flags() & FAULT_INTERLOCK) == 0);

	/* After 30s: fault */
	mock_get()->uptime = 5031000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert(selftest_get_fault_flags() & FAULT_INTERLOCK);
}

static void test_continuous_interlock_clears_when_current_drops(void)
{
	init_selftest();
	/* Trigger interlock fault */
	mock_get()->uptime = 6000000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	mock_get()->uptime = 6001000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	mock_get()->uptime = 6031000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert(selftest_get_fault_flags() & FAULT_INTERLOCK);

	/* Current drops — clears */
	mock_get()->uptime = 6032000;
	selftest_continuous_tick(2, 1489, 0, false, false);
	assert((selftest_get_fault_flags() & FAULT_INTERLOCK) == 0);
}

static void test_continuous_interlock_clears_when_charge_resumes(void)
{
	init_selftest();
	/* Trigger interlock fault */
	mock_get()->uptime = 7000000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	mock_get()->uptime = 7001000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	mock_get()->uptime = 7031000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert(selftest_get_fault_flags() & FAULT_INTERLOCK);

	/* Charge resumed — clears */
	mock_get()->uptime = 7032000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	assert((selftest_get_fault_flags() & FAULT_INTERLOCK) == 0);
}

static void test_continuous_pilot_out_of_range_sets_after_5s(void)
{
	init_selftest();
	mock_get()->adc_fail[0] = true;  /* pilot ADC fails */
	mock_get()->uptime = 8000000;
	selftest_continuous_tick(6, 0, 0, true, false);  /* J1772_STATE_UNKNOWN */
	assert((selftest_get_fault_flags() & FAULT_SENSOR) == 0);

	/* Before 5s */
	mock_get()->uptime = 8004000;
	selftest_continuous_tick(6, 0, 0, true, false);
	assert((selftest_get_fault_flags() & FAULT_SENSOR) == 0);

	/* After 5s */
	mock_get()->uptime = 8005000;
	selftest_continuous_tick(6, 0, 0, true, false);
	assert(selftest_get_fault_flags() & FAULT_SENSOR);
}

static void test_continuous_pilot_clears_on_resolve(void)
{
	init_selftest();
	/* Trigger pilot fault */
	mock_get()->adc_fail[0] = true;
	mock_get()->uptime = 9000000;
	selftest_continuous_tick(6, 0, 0, true, false);
	mock_get()->uptime = 9005000;
	selftest_continuous_tick(6, 0, 0, true, false);
	assert(selftest_get_fault_flags() & FAULT_SENSOR);

	/* Resolve */
	mock_get()->adc_fail[0] = false;
	mock_get()->uptime = 9006000;
	selftest_continuous_tick(0, 2980, 0, true, false);
	assert((selftest_get_fault_flags() & FAULT_SENSOR) == 0);
}

static void test_continuous_thermostat_chatter_fault(void)
{
	init_selftest();
	mock_get()->uptime = 10000000;

	/* Toggle cool_call >10 times within 60s */
	for (int i = 0; i < 12; i++) {
		mock_get()->uptime = 10000000 + (i * 2000);
		bool cool = (i % 2 == 0);
		selftest_continuous_tick(0, 2980, 0, true, cool);
	}

	assert(selftest_get_fault_flags() & FAULT_SENSOR);
}

static void test_continuous_thermostat_no_chatter(void)
{
	init_selftest();
	mock_get()->uptime = 11000000;

	/* Toggle <10 times in 60s — no fault */
	for (int i = 0; i < 6; i++) {
		mock_get()->uptime = 11000000 + (i * 5000);
		bool cool = (i % 2 == 0);
		selftest_continuous_tick(0, 2980, 0, true, cool);
	}

	assert((selftest_get_fault_flags() & FAULT_SENSOR) == 0);
}

/* ================================================================== */
/*  selftest shell + payload integration                               */
/* ================================================================== */

static int shell_print_count;
static void test_shell_print(const char *fmt, ...) { (void)fmt; shell_print_count++; }
static void test_shell_error(const char *fmt, ...) { (void)fmt; }

static void test_selftest_shell_all_pass(void)
{
	init_selftest();
	evse_sensors_set_api(mock_api());
	charge_control_set_api(mock_api());
	charge_control_init();
	thermostat_inputs_set_api(mock_api());
	shell_print_count = 0;

	int ret = selftest_run_shell(test_shell_print, test_shell_error);
	assert(ret == 0);
	assert(shell_print_count > 0);
}

static void test_selftest_fault_flags_in_uplink_byte7(void)
{
	init_selftest();
	/* Cause a boot failure to set FAULT_SELFTEST */
	mock_get()->adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);
	uint8_t flags = selftest_get_fault_flags();
	assert(flags & FAULT_SELFTEST);
	/* Should be in upper nibble */
	assert((flags & 0x0F) == 0);
	assert((flags & 0xF0) != 0);
}

static void test_selftest_fault_flags_coexist_with_thermostat(void)
{
	init_selftest();
	/* Set thermostat bits */
	mock_get()->gpio_values[2] = 1;  /* cool */
	thermostat_inputs_set_api(mock_api());
	uint8_t therm = thermostat_flags_get();  /* 0x02 */

	/* Cause selftest fault */
	mock_get()->adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);
	uint8_t fault = selftest_get_fault_flags();  /* 0x80 */

	/* Combined byte should have both */
	uint8_t combined = therm | fault;
	assert((combined & 0x02) == 0x02);  /* thermostat bits preserved */
	assert((combined & 0x80) == 0x80);  /* fault bit set */
}

/* ================================================================== */
/*  diag_request: 0x40 downlink and 0xE6 response                      */
/* ================================================================== */

static void init_diag(void)
{
	mock_reset();
	mock_get()->adc_values[0] = 2980;  /* pilot OK */
	mock_get()->adc_values[1] = 0;
	mock_get()->gpio_values[0] = 1;    /* charge enable */
	mock_get()->gpio_values[2] = 0;
	mock_get()->uptime = 120000;       /* 120 seconds */
	mock_get()->ready = true;

	selftest_set_api(mock_api());
	selftest_reset();
	charge_control_set_api(mock_api());
	charge_control_init();
	charge_control_set(true, 0);  /* Ensure clean state after prior tests */
	app_tx_set_api(mock_api());
	app_tx_set_ready(true);
	time_sync_set_api(mock_api());
	time_sync_init();
	event_buffer_init();
	diag_request_set_api(mock_api());
}

static void test_diag_build_response_format(void)
{
	init_diag();

	uint8_t buf[DIAG_PAYLOAD_SIZE];
	int ret = diag_request_build_response(buf);
	assert(ret == DIAG_PAYLOAD_SIZE);
	assert(buf[0] == DIAG_MAGIC);       /* 0xE6 */
	assert(buf[1] == DIAG_VERSION);     /* 0x01 */
}

static void test_diag_app_version(void)
{
	init_diag();

	uint8_t buf[DIAG_PAYLOAD_SIZE];
	diag_request_build_response(buf);

	uint16_t ver = buf[2] | (buf[3] << 8);
	assert(ver == APP_CALLBACK_VERSION);
}

static void test_diag_uptime(void)
{
	init_diag();
	mock_get()->uptime = 300000;  /* 300 seconds */

	uint8_t buf[DIAG_PAYLOAD_SIZE];
	diag_request_build_response(buf);

	uint32_t uptime = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
	assert(uptime == 300);
}

static void test_diag_boot_count_zero(void)
{
	init_diag();

	uint8_t buf[DIAG_PAYLOAD_SIZE];
	diag_request_build_response(buf);

	uint16_t boot = buf[8] | (buf[9] << 8);
	assert(boot == 0);  /* No persistent storage yet */
}

static void test_diag_no_fault_error_code(void)
{
	init_diag();

	/* No faults active */
	selftest_boot_result_t result;
	selftest_boot(&result);
	assert(result.all_pass == true);

	uint8_t err = diag_request_get_error_code();
	assert(err == DIAG_ERR_NONE);
}

static void test_diag_selftest_error_code(void)
{
	init_diag();

	/* Cause selftest failure */
	mock_get()->adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);

	uint8_t err = diag_request_get_error_code();
	assert(err == DIAG_ERR_SELFTEST);
}

static void test_diag_sensor_error_code(void)
{
	init_diag();

	/* Trigger sensor fault via continuous tick */
	mock_get()->adc_fail[0] = true;
	mock_get()->uptime = 8000000;
	selftest_continuous_tick(6, 0, 0, true, false);
	mock_get()->uptime = 8005000;
	selftest_continuous_tick(6, 0, 0, true, false);
	assert(selftest_get_fault_flags() & FAULT_SENSOR);

	uint8_t err = diag_request_get_error_code();
	assert(err == DIAG_ERR_SENSOR);
}

static void test_diag_state_flags_sidewalk_ready(void)
{
	init_diag();

	uint8_t flags = diag_request_get_state_flags();
	assert(flags & DIAG_FLAG_SIDEWALK_READY);
}

static void test_diag_state_flags_charge_allowed(void)
{
	init_diag();

	/* Default is allowed */
	uint8_t flags = diag_request_get_state_flags();
	assert(flags & DIAG_FLAG_CHARGE_ALLOWED);

	/* Pause charging */
	charge_control_set(false, 0);
	flags = diag_request_get_state_flags();
	assert(!(flags & DIAG_FLAG_CHARGE_ALLOWED));
}

static void test_diag_state_flags_selftest_pass(void)
{
	init_diag();

	/* Boot passes → selftest pass flag set */
	selftest_boot_result_t result;
	selftest_boot(&result);

	uint8_t flags = diag_request_get_state_flags();
	assert(flags & DIAG_FLAG_SELFTEST_PASS);
}

static void test_diag_state_flags_selftest_fail(void)
{
	init_diag();

	/* Cause boot failure */
	mock_get()->adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);

	uint8_t flags = diag_request_get_state_flags();
	assert(!(flags & DIAG_FLAG_SELFTEST_PASS));
}

static void test_diag_state_flags_time_synced(void)
{
	init_diag();

	/* Not synced initially */
	uint8_t flags = diag_request_get_state_flags();
	assert(!(flags & DIAG_FLAG_TIME_SYNCED));

	/* Sync time */
	uint8_t sync_cmd[] = {0x30,
		0x39, 0xA2, 0x04, 0x00,   /* epoch = 304697 */
		0x00, 0x00, 0x00, 0x00};  /* watermark = 0 */
	time_sync_process_cmd(sync_cmd, sizeof(sync_cmd));

	flags = diag_request_get_state_flags();
	assert(flags & DIAG_FLAG_TIME_SYNCED);
}

static void test_diag_event_buffer_pending(void)
{
	init_diag();

	uint8_t buf[DIAG_PAYLOAD_SIZE];

	/* Empty buffer */
	diag_request_build_response(buf);
	assert(buf[12] == 0);

	/* Add an event */
	struct event_snapshot snap = {
		.timestamp = 1000,
		.j1772_state = 3,
		.current_ma = 5000,
	};
	event_buffer_add(&snap);

	diag_request_build_response(buf);
	assert(buf[12] == 1);
}

static void test_diag_process_cmd_sends_response(void)
{
	init_diag();
	mock_get()->send_count = 0;

	uint8_t cmd[] = {DIAG_REQUEST_CMD_TYPE};
	int ret = diag_request_process_cmd(cmd, sizeof(cmd));
	assert(ret == 0);
	assert(mock_get()->send_count == 1);
	assert(mock_get()->sends[0].len == DIAG_PAYLOAD_SIZE);
	assert(mock_get()->sends[0].data[0] == DIAG_MAGIC);
}

static void test_diag_process_cmd_wrong_type(void)
{
	init_diag();

	uint8_t cmd[] = {0x99};
	int ret = diag_request_process_cmd(cmd, sizeof(cmd));
	assert(ret < 0);
}

static void test_diag_process_cmd_null_data(void)
{
	init_diag();

	int ret = diag_request_process_cmd(NULL, 0);
	assert(ret < 0);
}

static void test_diag_reserved_byte_zero(void)
{
	init_diag();

	uint8_t buf[DIAG_PAYLOAD_SIZE];
	diag_request_build_response(buf);
	assert(buf[13] == 0x00);
}

static void test_diag_rx_dispatches_0x40(void)
{
	/* Full integration: app_rx dispatches 0x40 to diag_request */
	mock_reset();
	mock_get()->adc_values[0] = 2980;
	mock_get()->uptime = 1000000;
	mock_get()->ready = true;

	app_cb.init(mock_api());
	mock_get()->send_count = 0;

	uint8_t cmd[] = {0x40};
	app_cb.on_msg_received(cmd, sizeof(cmd));
	assert(mock_get()->send_count == 1);
	assert(mock_get()->sends[0].data[0] == DIAG_MAGIC);
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
	RUN_TEST(test_thermostat_cool_only);
	RUN_TEST(test_thermostat_both_calls);

	printf("\ncharge_control:\n");
	RUN_TEST(test_charge_control_defaults_to_allowed);
	RUN_TEST(test_charge_control_pause_sets_gpio_low);
	RUN_TEST(test_charge_control_allow_sets_gpio_high);
	RUN_TEST(test_charge_control_auto_resume);
	RUN_TEST(test_charge_control_no_auto_resume_when_zero);

	printf("\napp_tx:\n");
	RUN_TEST(test_app_tx_sends_12_byte_payload);
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

	printf("\nselftest_boot:\n");
	RUN_TEST(test_selftest_boot_all_pass);
	RUN_TEST(test_selftest_boot_adc_pilot_fail);
	RUN_TEST(test_selftest_boot_adc_current_fail);
	RUN_TEST(test_selftest_boot_gpio_cool_fail);
	RUN_TEST(test_selftest_boot_charge_en_toggle_pass);
	RUN_TEST(test_selftest_boot_charge_en_readback_fail);
	RUN_TEST(test_selftest_boot_flag_latched);
	RUN_TEST(test_selftest_boot_led_flash_on_failure);

	printf("\nselftest_continuous:\n");
	RUN_TEST(test_continuous_clamp_mismatch_state_c_no_current);
	RUN_TEST(test_continuous_clamp_mismatch_not_c_with_current);
	RUN_TEST(test_continuous_clamp_mismatch_clears_on_resolve);
	RUN_TEST(test_continuous_normal_operation_no_fault);
	RUN_TEST(test_continuous_interlock_current_after_pause);
	RUN_TEST(test_continuous_interlock_clears_when_current_drops);
	RUN_TEST(test_continuous_interlock_clears_when_charge_resumes);
	RUN_TEST(test_continuous_pilot_out_of_range_sets_after_5s);
	RUN_TEST(test_continuous_pilot_clears_on_resolve);
	RUN_TEST(test_continuous_thermostat_chatter_fault);
	RUN_TEST(test_continuous_thermostat_no_chatter);

	printf("\nselftest shell + payload:\n");
	RUN_TEST(test_selftest_shell_all_pass);
	RUN_TEST(test_selftest_fault_flags_in_uplink_byte7);
	RUN_TEST(test_selftest_fault_flags_coexist_with_thermostat);

	printf("\ndiag_request:\n");
	RUN_TEST(test_diag_build_response_format);
	RUN_TEST(test_diag_app_version);
	RUN_TEST(test_diag_uptime);
	RUN_TEST(test_diag_boot_count_zero);
	RUN_TEST(test_diag_no_fault_error_code);
	RUN_TEST(test_diag_selftest_error_code);
	RUN_TEST(test_diag_sensor_error_code);
	RUN_TEST(test_diag_state_flags_sidewalk_ready);
	RUN_TEST(test_diag_state_flags_charge_allowed);
	RUN_TEST(test_diag_state_flags_selftest_pass);
	RUN_TEST(test_diag_state_flags_selftest_fail);
	RUN_TEST(test_diag_state_flags_time_synced);
	RUN_TEST(test_diag_event_buffer_pending);
	RUN_TEST(test_diag_process_cmd_sends_response);
	RUN_TEST(test_diag_process_cmd_wrong_type);
	RUN_TEST(test_diag_process_cmd_null_data);
	RUN_TEST(test_diag_reserved_byte_zero);
	RUN_TEST(test_diag_rx_dispatches_0x40);

	printf("\n=== %d/%d tests passed ===\n\n", tests_passed, tests_run);
	return (tests_passed == tests_run) ? 0 : 1;
}
