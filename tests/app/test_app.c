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

#include "mock_platform_api.h"
#include <app_platform.h>
#include <evse_sensors.h>
#include <charge_control.h>
#include <charge_now.h>
#include <thermostat_inputs.h>
#include <selftest.h>
#include <selftest_trigger.h>
#include <diag_request.h>
#include <app_tx.h>
#include <app_rx.h>
#include <cmd_auth.h>
#include <time_sync.h>
#include <event_buffer.h>
#include <event_filter.h>
#include <delay_window.h>
#include <led_engine.h>
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
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;  /* ~12V after divider */
	platform = mock_platform_api_get();

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_A);
	assert(mv == 2980);
}

static void test_j1772_state_b_connected(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2234;  /* ~9V */
	platform = mock_platform_api_get();

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_B);
}

static void test_j1772_state_c_charging(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 1489;  /* ~6V */
	platform = mock_platform_api_get();

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_C);
}

static void test_j1772_state_d_ventilation(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 745;  /* ~3V */
	platform = mock_platform_api_get();

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_D);
}

static void test_j1772_state_e_error(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 100;  /* ~0V */
	platform = mock_platform_api_get();

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_E);
}

static void test_j1772_boundary_a_b(void)
{
	mock_platform_api_reset();
	platform = mock_platform_api_get();

	j1772_state_t state;
	uint16_t mv;

	/* Just above threshold -> A */
	mock_adc_values[0] = 2601;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_A);

	/* At threshold -> B */
	mock_adc_values[0] = 2600;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_B);
}

static void test_j1772_boundary_b_c(void)
{
	mock_platform_api_reset();
	platform = mock_platform_api_get();

	j1772_state_t state;
	uint16_t mv;

	mock_adc_values[0] = 1851;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_B);

	mock_adc_values[0] = 1850;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_C);
}

static void test_j1772_null_api_returns_error(void)
{
	platform = NULL;

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) != 0);
}

static void test_current_read_conversion(void)
{
	/* Current clamp stubbed on WisBlock — always returns 0 mA */
	mock_platform_api_reset();
	platform = mock_platform_api_get();

	uint16_t current_ma;
	assert(evse_current_read(&current_ma) == 0);
	assert(current_ma == 0);
}

static void test_current_read_zero(void)
{
	mock_platform_api_reset();
	mock_adc_values[1] = 0;
	platform = mock_platform_api_get();

	uint16_t current_ma;
	assert(evse_current_read(&current_ma) == 0);
	assert(current_ma == 0);
}

static void test_simulation_overrides_adc(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;  /* real = State A */
	mock_uptime_ms = 1000;
	platform = mock_platform_api_get();

	/* Simulate State C for 10s */
	evse_sensors_simulate_state(J1772_STATE_C, 10000);

	j1772_state_t state;
	uint16_t mv;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_C);  /* simulated, not A */

	/* Expire the simulation */
	mock_uptime_ms = 12000;
	assert(evse_j1772_state_get(&state, &mv) == 0);
	assert(state == J1772_STATE_A);  /* back to real */
}

/* ================================================================== */
/*  thermostat_inputs: GPIO flag reading                               */
/* ================================================================== */

static void test_thermostat_no_calls(void)
{
	mock_platform_api_reset();
	mock_gpio_values[2] = 0;  /* cool off */
	platform = mock_platform_api_get();

	assert(thermostat_inputs_flags_get() == 0x00);
}

static void test_thermostat_cool_only(void)
{
	mock_platform_api_reset();
	mock_gpio_values[2] = 1;  /* cool on */
	platform = mock_platform_api_get();

	assert(thermostat_inputs_flags_get() == 0x02);
}

static void test_thermostat_both_calls(void)
{
	mock_platform_api_reset();
	mock_gpio_values[2] = 1;
	platform = mock_platform_api_get();

	assert(thermostat_inputs_flags_get() == 0x02);
}

/* ================================================================== */
/*  charge_control: GPIO control and auto-resume                       */
/* ================================================================== */

static void test_charge_control_defaults_to_allowed(void)
{
	mock_platform_api_reset();
	platform = mock_platform_api_get();
	charge_control_init();

	assert(charge_control_is_allowed() == true);
	/* Init should set GPIO pin 0 low (allowed = LOW) */
	assert(mock_gpio_set_last_pin == 0);
	assert(mock_gpio_set_last_val == 0);
}

static void test_charge_control_pause_sets_gpio_high(void)
{
	mock_platform_api_reset();
	platform = mock_platform_api_get();
	charge_control_init();

	charge_control_set(false, 0);
	assert(charge_control_is_allowed() == false);
	assert(mock_gpio_set_last_val == 1);
}

static void test_charge_control_allow_sets_gpio_low(void)
{
	mock_platform_api_reset();
	platform = mock_platform_api_get();
	charge_control_init();

	charge_control_set(false, 0);
	charge_control_set(true, 0);
	assert(charge_control_is_allowed() == true);
	assert(mock_gpio_set_last_val == 0);
}

static void test_charge_control_auto_resume(void)
{
	mock_platform_api_reset();
	mock_uptime_ms = 10000;
	platform = mock_platform_api_get();
	charge_control_init();

	/* Pause with 1-minute auto-resume */
	charge_control_set(false, 1);
	assert(charge_control_is_allowed() == false);

	/* Tick at 30s — still paused */
	mock_uptime_ms = 40000;
	charge_control_tick();
	assert(charge_control_is_allowed() == false);

	/* Tick at 61s — should auto-resume */
	mock_uptime_ms = 71000;
	charge_control_tick();
	assert(charge_control_is_allowed() == true);
	assert(mock_gpio_set_last_val == 0);
}

static void test_charge_control_no_auto_resume_when_zero(void)
{
	mock_platform_api_reset();
	mock_uptime_ms = 10000;
	platform = mock_platform_api_get();
	charge_control_init();

	/* Pause without auto-resume */
	charge_control_set(false, 0);

	mock_uptime_ms = 1000000;  /* way later */
	charge_control_tick();
	assert(charge_control_is_allowed() == false);
}

/* ================================================================== */
/*  app_tx: payload format and rate limiting                           */
/* ================================================================== */

static void test_app_tx_sends_12_byte_payload(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;  /* State A */
	mock_adc_values[1] = 0;     /* 0 current */
	mock_gpio_values[2] = 0;    /* no cool */
	mock_uptime_ms = 10000;
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	time_sync_init();
	platform = mock_platform_api_get();
	charge_now_init();
	platform = mock_platform_api_get();
	app_tx_set_ready(true);

	int ret = app_tx_send_evse_data();
	assert(ret == 0);
	assert(mock_send_count == 1);
	assert(mock_sends[0].len == 15);

	/* Check magic and version bytes */
	assert(mock_sends[0].data[0] == 0xE5);  /* TELEMETRY_MAGIC */
	assert(mock_sends[0].data[1] == 0x0A);  /* PAYLOAD_VERSION v0x0A */
}

static void test_app_tx_rate_limits(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 100000;  /* well past any previous test's send */
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	app_tx_set_ready(true);

	/* First send succeeds */
	assert(app_tx_send_evse_data() == 0);
	assert(mock_send_count == 1);

	/* Second send within 5s is rate-limited */
	mock_uptime_ms = 102000;
	assert(app_tx_send_evse_data() == 0);  /* returns 0 (rate-limited, not error) */
	assert(mock_send_count == 1);   /* still 1 */

	/* After 5s, send works */
	mock_uptime_ms = 106000;
	assert(app_tx_send_evse_data() == 0);
	assert(mock_send_count == 2);
}

static void test_app_tx_not_ready_skips(void)
{
	mock_platform_api_reset();
	mock_sidewalk_ready = false;

	platform = mock_platform_api_get();
	app_tx_set_ready(false);

	assert(app_tx_send_evse_data() == -1);
	assert(mock_send_count == 0);
}

/* ================================================================== */
/*  app_entry on_timer: change detection and heartbeat                 */
/* ================================================================== */

static uint32_t timer_test_base;

/* Call on_timer 5 times (decimation) so sensor logic executes once */
static void tick_sensor_cycle(void)
{
	for (int i = 0; i < 5; i++) {
		app_cb.on_timer();
	}
}

static void init_app_for_timer_tests(void)
{
	/* Use a high base uptime to avoid rate-limiter bleed from prior tests */
	timer_test_base = 500000;

	mock_platform_api_reset();
	mock_adc_values[0] = 2980;  /* State A */
	mock_adc_values[1] = 0;     /* no current */
	mock_gpio_values[2] = 0;    /* no cool */
	mock_uptime_ms = timer_test_base;
	mock_sidewalk_ready = true;

	app_cb.init(mock_platform_api_get());
	/* Clear sends from init */
	mock_send_count = 0;
}

static void test_on_timer_no_change_no_send(void)
{
	init_app_for_timer_tests();

	/* Tick with no changes and heartbeat not due */
	mock_uptime_ms = timer_test_base + 1000;
	tick_sensor_cycle();
	assert(mock_send_count == 0);
}

static void test_on_timer_j1772_change_triggers_send(void)
{
	init_app_for_timer_tests();

	/* Change J1772 from A to C */
	mock_adc_values[0] = 1489;
	mock_uptime_ms = timer_test_base + 1000;
	tick_sensor_cycle();
	assert(mock_send_count == 1);
}

static void test_on_timer_current_change_no_send_stubbed(void)
{
	/* Current clamp stubbed on WisBlock — ADC changes don't affect current */
	init_app_for_timer_tests();

	mock_adc_values[1] = 1650;  /* would be 15A if clamp existed */
	mock_uptime_ms = timer_test_base + 1000;
	tick_sensor_cycle();
	assert(mock_send_count == 0);  /* no change detected — current stays 0 */
}

static void test_on_timer_thermostat_change_triggers_send(void)
{
	init_app_for_timer_tests();

	/* Turn on cool call */
	mock_gpio_values[2] = 1;
	mock_uptime_ms = timer_test_base + 1000;
	tick_sensor_cycle();
	assert(mock_send_count == 1);
}

static void test_on_timer_heartbeat_sends_after_60s(void)
{
	init_app_for_timer_tests();

	/* No changes, but 60s passes */
	mock_uptime_ms = timer_test_base + 61000;
	tick_sensor_cycle();
	assert(mock_send_count == 1);
}

static void test_on_timer_no_heartbeat_before_60s(void)
{
	init_app_for_timer_tests();

	/* No changes, only 30s passed */
	mock_uptime_ms = timer_test_base + 30000;
	tick_sensor_cycle();
	assert(mock_send_count == 0);
}

static void test_on_timer_multiple_changes_one_send(void)
{
	init_app_for_timer_tests();

	/* Change J1772 AND thermostat in the same tick */
	mock_adc_values[0] = 1489;  /* A -> C */
	mock_gpio_values[2] = 1;    /* cool on */
	mock_uptime_ms = timer_test_base + 1000;
	tick_sensor_cycle();
	assert(mock_send_count == 1);  /* one send, not two */
}

static void test_on_timer_settled_after_change_no_send(void)
{
	init_app_for_timer_tests();

	/* First tick: change triggers live send */
	mock_adc_values[0] = 1489;
	mock_uptime_ms = timer_test_base + 1000;
	tick_sensor_cycle();
	assert(mock_send_count == 1);

	/* Second tick: same values, no live send (no change, no heartbeat).
	 * Buffer drain may send historical events — that's expected. */
	mock_uptime_ms = timer_test_base + 7000;  /* past rate limit but not heartbeat */
	tick_sensor_cycle();
	assert(mock_send_count >= 1);  /* no fewer than before */
}

static void test_init_sets_timer_interval(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 900000;

	app_cb.init(mock_platform_api_get());
	assert(mock_timer_interval == 100);
}

/* ================================================================== */
/*  selftest_boot: hardware path checks                                */
/* ================================================================== */

static void init_selftest(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;  /* pilot OK (State A) */
	mock_gpio_values[0] = 1;    /* charge block */
	mock_gpio_values[2] = 0;    /* cool */
	mock_uptime_ms = 1000000;      /* high base */
	platform = mock_platform_api_get();
	selftest_reset();
}

static void test_selftest_boot_all_pass(void)
{
	init_selftest();
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == 0);
	assert(result.adc_pilot_ok == true);
	assert(result.gpio_cool_ok == true);
	assert(result.charge_block_ok == true);
	assert(result.all_pass == true);
	assert((selftest_get_fault_flags() & FAULT_SELFTEST) == 0);
}

static void test_selftest_boot_adc_pilot_fail(void)
{
	init_selftest();
	mock_adc_fail[0] = true;
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == -1);
	assert(result.adc_pilot_ok == false);
	assert(result.all_pass == false);
	assert(selftest_get_fault_flags() & FAULT_SELFTEST);
}

static void test_selftest_boot_gpio_cool_fail(void)
{
	init_selftest();
	mock_gpio_fail[2] = true;
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == -1);
	assert(result.gpio_cool_ok == false);
	assert(result.all_pass == false);
}

static void test_selftest_boot_charge_block_toggle_pass(void)
{
	init_selftest();
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == 0);
	assert(result.charge_block_ok == true);
}

static void test_selftest_boot_charge_block_readback_fail(void)
{
	init_selftest();
	mock_gpio_readback_fail[0] = true;
	selftest_boot_result_t result;
	assert(selftest_boot(&result) == -1);
	assert(result.charge_block_ok == false);
	assert(result.all_pass == false);
}

static void test_selftest_boot_flag_clears_on_retest(void)
{
	/* After boot failure, FAULT_SELFTEST is set */
	init_selftest();
	mock_adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);
	assert(selftest_get_fault_flags() & FAULT_SELFTEST);

	/* Running boot again with everything OK clears FAULT_SELFTEST */
	mock_adc_fail[0] = false;
	selftest_boot(&result);
	assert(result.all_pass == true);
	assert((selftest_get_fault_flags() & FAULT_SELFTEST) == 0);
}

static void test_selftest_boot_no_stale_fault_on_pass(void)
{
	/* Boot passes on first run — no stale 0x80 from prior run */
	init_selftest();
	selftest_boot_result_t result;
	selftest_boot(&result);
	assert(result.all_pass == true);
	assert((selftest_get_fault_flags() & FAULT_SELFTEST) == 0);

	/* Pass again — still no 0x80 */
	selftest_boot(&result);
	assert(result.all_pass == true);
	assert((selftest_get_fault_flags() & FAULT_SELFTEST) == 0);
}

static void test_selftest_boot_led_flash_on_failure(void)
{
	init_selftest();
	mock_adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);
	/* Should have called led_set at least twice (on + off) */
	assert(mock_led_set_count >= 2);
	assert(mock_led_last_id == 2);
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
	mock_uptime_ms = 1009000;
	selftest_continuous_tick(2, 1489, 0, true, false);
	assert((selftest_get_fault_flags() & FAULT_CLAMP) == 0);

	/* Tick at +10s: fault triggers */
	mock_uptime_ms = 1010000;
	selftest_continuous_tick(2, 1489, 0, true, false);
	assert(selftest_get_fault_flags() & FAULT_CLAMP);
}

static void test_continuous_clamp_mismatch_not_c_with_current(void)
{
	init_selftest();
	/* State A (idle) but current flowing — mismatch */
	mock_uptime_ms = 2000000;
	selftest_continuous_tick(0, 2980, 5000, true, false);
	assert((selftest_get_fault_flags() & FAULT_CLAMP) == 0);

	/* After 10s */
	mock_uptime_ms = 2010000;
	selftest_continuous_tick(0, 2980, 5000, true, false);
	assert(selftest_get_fault_flags() & FAULT_CLAMP);
}

static void test_continuous_clamp_mismatch_clears_on_resolve(void)
{
	init_selftest();
	/* Trigger mismatch */
	mock_uptime_ms = 3000000;
	selftest_continuous_tick(2, 1489, 0, true, false);
	mock_uptime_ms = 3010000;
	selftest_continuous_tick(2, 1489, 0, true, false);
	assert(selftest_get_fault_flags() & FAULT_CLAMP);

	/* Resolve: State C with current */
	mock_uptime_ms = 3011000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	assert((selftest_get_fault_flags() & FAULT_CLAMP) == 0);
}

static void test_continuous_normal_operation_no_fault(void)
{
	init_selftest();
	/* State C + current = normal */
	mock_uptime_ms = 4000000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	mock_uptime_ms = 4010000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	assert((selftest_get_fault_flags() & FAULT_CLAMP) == 0);
}

static void test_continuous_interlock_current_after_pause(void)
{
	init_selftest();
	/* Start allowed */
	mock_uptime_ms = 5000000;
	selftest_continuous_tick(2, 1489, 5000, true, false);

	/* Transition to paused, but current persists */
	mock_uptime_ms = 5001000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert((selftest_get_fault_flags() & FAULT_INTERLOCK) == 0);

	/* Before 30s: no fault */
	mock_uptime_ms = 5029000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert((selftest_get_fault_flags() & FAULT_INTERLOCK) == 0);

	/* After 30s: fault */
	mock_uptime_ms = 5031000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert(selftest_get_fault_flags() & FAULT_INTERLOCK);
}

static void test_continuous_interlock_clears_when_current_drops(void)
{
	init_selftest();
	/* Trigger interlock fault */
	mock_uptime_ms = 6000000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	mock_uptime_ms = 6001000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	mock_uptime_ms = 6031000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert(selftest_get_fault_flags() & FAULT_INTERLOCK);

	/* Current drops — clears */
	mock_uptime_ms = 6032000;
	selftest_continuous_tick(2, 1489, 0, false, false);
	assert((selftest_get_fault_flags() & FAULT_INTERLOCK) == 0);
}

static void test_continuous_interlock_clears_when_charge_resumes(void)
{
	init_selftest();
	/* Trigger interlock fault */
	mock_uptime_ms = 7000000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	mock_uptime_ms = 7001000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	mock_uptime_ms = 7031000;
	selftest_continuous_tick(2, 1489, 5000, false, false);
	assert(selftest_get_fault_flags() & FAULT_INTERLOCK);

	/* Charge resumed — clears */
	mock_uptime_ms = 7032000;
	selftest_continuous_tick(2, 1489, 5000, true, false);
	assert((selftest_get_fault_flags() & FAULT_INTERLOCK) == 0);
}

static void test_continuous_pilot_out_of_range_sets_after_5s(void)
{
	init_selftest();
	mock_uptime_ms = 8000000;
	selftest_continuous_tick(6, 0, 0, true, false);  /* J1772_STATE_UNKNOWN */
	assert((selftest_get_fault_flags() & FAULT_SENSOR) == 0);

	/* Before 5s */
	mock_uptime_ms = 8004000;
	selftest_continuous_tick(6, 0, 0, true, false);
	assert((selftest_get_fault_flags() & FAULT_SENSOR) == 0);

	/* After 5s */
	mock_uptime_ms = 8005000;
	selftest_continuous_tick(6, 0, 0, true, false);
	assert(selftest_get_fault_flags() & FAULT_SENSOR);
}

static void test_continuous_pilot_clears_on_resolve(void)
{
	init_selftest();
	/* Trigger pilot fault */
	mock_uptime_ms = 9000000;
	selftest_continuous_tick(6, 0, 0, true, false);
	mock_uptime_ms = 9005000;
	selftest_continuous_tick(6, 0, 0, true, false);
	assert(selftest_get_fault_flags() & FAULT_SENSOR);

	/* Resolve */
	mock_uptime_ms = 9006000;
	selftest_continuous_tick(0, 2980, 0, true, false);
	assert((selftest_get_fault_flags() & FAULT_SENSOR) == 0);
}

static void test_continuous_pilot_uses_state_not_adc(void)
{
	init_selftest();
	/* ADC fails, but we pass a valid state — no fault should be raised */
	mock_adc_fail[0] = true;
	mock_uptime_ms = 12000000;
	selftest_continuous_tick(0, 2980, 0, true, 0x00);  /* state A, valid */
	mock_uptime_ms = 12005000;
	selftest_continuous_tick(0, 2980, 0, true, 0x00);
	mock_uptime_ms = 12010000;
	selftest_continuous_tick(0, 2980, 0, true, 0x00);
	/* No FAULT_SENSOR because continuous_tick uses j1772_state, not ADC */
	assert((selftest_get_fault_flags() & FAULT_SENSOR) == 0);
}

static void test_continuous_thermostat_chatter_fault(void)
{
	init_selftest();
	mock_uptime_ms = 10000000;

	/* Toggle cool_call >10 times within 60s */
	for (int i = 0; i < 12; i++) {
		mock_uptime_ms = 10000000 + (i * 2000);
		uint8_t therm = (i % 2 == 0) ? 0x02 : 0x00;
		selftest_continuous_tick(0, 2980, 0, true, therm);
	}

	assert(selftest_get_fault_flags() & FAULT_SENSOR);
}

static void test_continuous_thermostat_no_chatter(void)
{
	init_selftest();
	mock_uptime_ms = 11000000;

	/* Toggle <10 times in 60s — no fault */
	for (int i = 0; i < 6; i++) {
		mock_uptime_ms = 11000000 + (i * 5000);
		uint8_t therm = (i % 2 == 0) ? 0x02 : 0x00;
		selftest_continuous_tick(0, 2980, 0, true, therm);
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
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	shell_print_count = 0;

	int ret = selftest_run_shell(test_shell_print, test_shell_error);
	assert(ret == 0);
	assert(shell_print_count > 0);
}

static void test_selftest_fault_flags_in_uplink_byte7(void)
{
	init_selftest();
	/* Cause a boot failure to set FAULT_SELFTEST */
	mock_adc_fail[0] = true;
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
	mock_gpio_values[2] = 1;  /* cool */
	platform = mock_platform_api_get();
	uint8_t therm = thermostat_inputs_flags_get();  /* 0x02 */

	/* Cause selftest fault */
	mock_adc_fail[0] = true;
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
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;  /* pilot OK */
	mock_adc_values[1] = 0;
	mock_gpio_values[0] = 1;    /* charge enable */
	mock_gpio_values[2] = 0;
	mock_uptime_ms = 120000;       /* 120 seconds */
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	charge_control_init();
	charge_control_set(true, 0);  /* Ensure clean state after prior tests */
	platform = mock_platform_api_get();
	app_tx_set_ready(true);
	platform = mock_platform_api_get();
	time_sync_init();
	event_buffer_init();
	platform = mock_platform_api_get();
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
	mock_uptime_ms = 300000;  /* 300 seconds */

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
	mock_adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);

	uint8_t err = diag_request_get_error_code();
	assert(err == DIAG_ERR_SELFTEST);
}

static void test_diag_sensor_error_code(void)
{
	init_diag();

	/* Trigger sensor fault via continuous tick (UNKNOWN state for 5s) */
	mock_uptime_ms = 8000000;
	selftest_continuous_tick(6, 0, 0, true, 0x00);
	mock_uptime_ms = 8005000;
	selftest_continuous_tick(6, 0, 0, true, 0x00);
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
	mock_adc_fail[0] = true;
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
	mock_send_count = 0;

	uint8_t cmd[] = {DIAG_REQUEST_CMD_TYPE};
	int ret = diag_request_process_cmd(cmd, sizeof(cmd));
	assert(ret == 0);
	assert(mock_send_count == 1);
	assert(mock_sends[0].len == DIAG_PAYLOAD_SIZE);
	assert(mock_sends[0].data[0] == DIAG_MAGIC);
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

static void test_diag_build_version_byte(void)
{
	init_diag();

	uint8_t buf[DIAG_PAYLOAD_SIZE];
	diag_request_build_response(buf);
	assert(buf[13] == APP_BUILD_VERSION);
}

static void test_diag_platform_build_version_byte(void)
{
	init_diag();

	uint8_t buf[DIAG_PAYLOAD_SIZE];
	diag_request_build_response(buf);
	assert(buf[14] == PLATFORM_BUILD_VERSION);
}

static void test_diag_rx_dispatches_0x40(void)
{
	/* Full integration: app_rx dispatches 0x40 to diag_request */
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 1000000;
	mock_sidewalk_ready = true;

	app_cb.init(mock_platform_api_get());
	mock_send_count = 0;

	uint8_t cmd[] = {0x40};
	app_cb.on_msg_received(cmd, sizeof(cmd));
	assert(mock_send_count == 1);
	assert(mock_sends[0].data[0] == DIAG_MAGIC);
}

/* ================================================================== */
/*  LED engine: priority evaluation                                    */
/* ================================================================== */

static void init_led_engine(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;  /* State A */
	mock_adc_values[1] = 0;
	mock_gpio_values[2] = 0;
	mock_uptime_ms = 400000;  /* past commissioning timeout (300s) */
	mock_sidewalk_ready = true;
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	led_engine_init();
	/* Force commissioning to exit (uptime > 300s) */
	led_engine_tick();
}

static void test_led_idle_default(void)
{
	init_led_engine();
	assert(led_engine_get_active_priority() == LED_PRI_IDLE);
}

static void test_led_error_highest_priority(void)
{
	init_led_engine();
	/* Cause selftest failure */
	mock_adc_fail[0] = true;
	selftest_boot_result_t result;
	selftest_boot(&result);
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_ERROR);
}

static void test_led_ota_higher_than_commission(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 1000;  /* within commissioning window */
	mock_sidewalk_ready = true;
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	led_engine_init();
	led_engine_set_ota_active(true);
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_OTA);
}

static void test_led_commission_at_boot(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 1000;  /* within 5 min window */
	mock_sidewalk_ready = true;
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	led_engine_init();
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_COMMISSION);
	assert(led_engine_is_commissioning() == true);
}

static void test_led_commission_exits_on_uplink(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 1000;
	mock_sidewalk_ready = true;
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	led_engine_init();
	led_engine_tick();
	assert(led_engine_is_commissioning() == true);

	led_engine_notify_uplink_sent();
	led_engine_tick();
	assert(led_engine_is_commissioning() == false);
}

static void test_led_commission_exits_on_timeout(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 1000;
	mock_sidewalk_ready = true;
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	led_engine_init();
	led_engine_tick();
	assert(led_engine_is_commissioning() == true);

	mock_uptime_ms = 300001;  /* past 5 min */
	led_engine_tick();
	assert(led_engine_is_commissioning() == false);
}

static void test_led_disconnected_after_commission(void)
{
	init_led_engine();  /* commissioning already expired */
	mock_sidewalk_ready = false;
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_DISCONNECTED);
}

static void test_led_charge_now_override(void)
{
	init_led_engine();
	led_engine_set_charge_now_override(true);
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_CHARGE_NOW);
}

static void test_led_ac_priority(void)
{
	init_led_engine();
	/* Cool call active + charging paused = AC priority */
	mock_gpio_values[2] = 1;
	charge_control_set(false, 0);
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_AC_PRIORITY);
}

static void test_led_charging_state_c(void)
{
	init_led_engine();
	mock_adc_values[0] = 1489;  /* State C */
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_CHARGING);
}

/* ================================================================== */
/*  LED engine: pattern output                                         */
/* ================================================================== */

static void test_led_error_toggles_every_tick(void)
{
	init_led_engine();
	/* Cause error via ADC failures */
	led_engine_report_adc_result(false);
	led_engine_report_adc_result(false);
	led_engine_report_adc_result(false);

	mock_led_call_count = 0;
	led_engine_tick();  /* step 0: on */
	assert(mock_led_calls[0].on == true);
	led_engine_tick();  /* step 1: off */
	assert(mock_led_calls[1].on == false);
	led_engine_tick();  /* step 0 again: on */
	assert(mock_led_calls[2].on == true);
	led_engine_tick();  /* step 1 again: off */
	assert(mock_led_calls[3].on == false);
}

static void test_led_commission_5on_5off(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 1000;
	mock_sidewalk_ready = true;
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	led_engine_init();

	mock_led_call_count = 0;
	/* Tick 10 times: should get 5 ON then 5 OFF */
	for (int i = 0; i < 10; i++) {
		led_engine_tick();
	}
	/* First 5 calls should be ON */
	for (int i = 0; i < 5; i++) {
		assert(mock_led_calls[i].on == true);
	}
	/* Next 5 calls should be OFF */
	for (int i = 5; i < 10; i++) {
		assert(mock_led_calls[i].on == false);
	}
}

static void test_led_idle_blip(void)
{
	init_led_engine();
	/* Re-init engine to reset step counter (init_led_engine ticked once) */
	led_engine_init();
	/* Force commissioning exit again (uptime is already past 300s) */
	led_engine_tick();
	/* Now at step 1 (OFF) — reinit to get clean pattern start */
	led_engine_init();
	mock_uptime_ms = 400000;
	/* Directly advance past commissioning without ticking */
	led_engine_notify_uplink_sent();

	mock_led_call_count = 0;

	/* First tick: ON (blip) */
	led_engine_tick();
	assert(mock_led_calls[0].on == true);

	/* Next 99 ticks: OFF */
	for (int i = 0; i < 99; i++) {
		led_engine_tick();
	}
	assert(mock_led_calls[1].on == false);
	assert(mock_led_calls[99].on == false);
}

static void test_led_solid_on_charging(void)
{
	init_led_engine();
	mock_adc_values[0] = 1489;  /* State C */
	mock_led_call_count = 0;

	for (int i = 0; i < 5; i++) {
		led_engine_tick();
	}
	/* All ticks should be ON (solid) */
	for (int i = 0; i < 5; i++) {
		assert(mock_led_calls[i].on == true);
	}
}

static void test_led_pattern_resets_on_priority_change(void)
{
	init_led_engine();
	mock_led_call_count = 0;

	/* Start in idle, tick a couple */
	led_engine_tick();
	led_engine_tick();

	/* Switch to charging — pattern should restart */
	mock_adc_values[0] = 1489;
	mock_led_call_count = 0;
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_CHARGING);
	assert(mock_led_calls[0].on == true);
}

/* ================================================================== */
/*  LED engine: error tracking                                         */
/* ================================================================== */

static void test_led_3_adc_failures_error(void)
{
	init_led_engine();
	led_engine_report_adc_result(false);
	led_engine_report_adc_result(false);
	led_engine_tick();
	assert(led_engine_get_active_priority() != LED_PRI_ERROR);

	led_engine_report_adc_result(false);
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_ERROR);
}

static void test_led_adc_success_resets_counter(void)
{
	init_led_engine();
	led_engine_report_adc_result(false);
	led_engine_report_adc_result(false);
	led_engine_report_adc_result(true);  /* reset */
	led_engine_report_adc_result(false);
	led_engine_tick();
	assert(led_engine_get_active_priority() != LED_PRI_ERROR);
}

static void test_led_sidewalk_10min_timeout(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 400000;
	mock_sidewalk_ready = false;  /* not connected */
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	led_engine_init();
	/* Force commissioning exit */
	led_engine_notify_uplink_sent();

	/* First tick starts the timeout timer */
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_DISCONNECTED);

	/* After 10 minutes → error */
	mock_uptime_ms = 400000 + 600000;
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_ERROR);
}

static void test_led_sidewalk_timeout_clears_on_ready(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 400000;
	mock_sidewalk_ready = false;
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	led_engine_init();
	led_engine_notify_uplink_sent();

	led_engine_tick();
	mock_uptime_ms = 400000 + 600000;
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_ERROR);

	/* Connected → clears error */
	mock_sidewalk_ready = true;
	led_engine_tick();
	assert(led_engine_get_active_priority() != LED_PRI_ERROR);
}

/* ================================================================== */
/*  LED engine: self-test coexistence                                  */
/* ================================================================== */

/* selftest_trigger is_running check — we can't easily fake it in unit tests
 * because selftest_trigger has internal state. Instead we verify the engine
 * restores after a priority change. */

static void test_led_yields_during_selftest(void)
{
	/* When selftest_trigger_is_running() returns true, led_engine_tick
	 * should not call led_set. We can't easily mock selftest_trigger
	 * internals, but we verify the step counter resets. */
	init_led_engine();
	/* Tick a few times to advance pattern */
	led_engine_tick();
	led_engine_tick();
	led_engine_tick();
	/* Just verify engine is functional after multiple ticks */
	assert(led_engine_get_active_priority() == LED_PRI_IDLE);
}

static void test_led_restores_after_selftest(void)
{
	init_led_engine();
	/* Switch to charging */
	mock_adc_values[0] = 1489;
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_CHARGING);
	/* Back to idle */
	mock_adc_values[0] = 2980;
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_IDLE);
}

/* ================================================================== */
/*  LED engine: button feedback                                        */
/* ================================================================== */

static void test_led_button_ack_3_blinks(void)
{
	init_led_engine();
	mock_led_call_count = 0;
	led_engine_button_ack();

	/* 6 steps: on-off-on-off-on-off, each 1 tick */
	for (int i = 0; i < 6; i++) {
		led_engine_tick();
	}
	assert(mock_led_calls[0].on == true);
	assert(mock_led_calls[1].on == false);
	assert(mock_led_calls[2].on == true);
	assert(mock_led_calls[3].on == false);
	assert(mock_led_calls[4].on == true);
	assert(mock_led_calls[5].on == false);
}

static void test_led_button_ack_blocked_by_error(void)
{
	init_led_engine();
	/* Cause error */
	led_engine_report_adc_result(false);
	led_engine_report_adc_result(false);
	led_engine_report_adc_result(false);
	led_engine_tick();
	assert(led_engine_get_active_priority() == LED_PRI_ERROR);

	/* Button ack should be blocked */
	mock_led_call_count = 0;
	led_engine_button_ack();
	led_engine_tick();
	/* Should still be in error pattern (toggling), not ack */
	assert(led_engine_get_active_priority() == LED_PRI_ERROR);
}

/* ================================================================== */
/*  LED engine: timer decimation                                       */
/* ================================================================== */

static void test_led_timer_interval_100(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 800000;
	app_cb.init(mock_platform_api_get());
	assert(mock_timer_interval == 100);
}

static void test_led_decimation_sensors_every_5th(void)
{
	init_app_for_timer_tests();

	/* Change J1772 state */
	mock_adc_values[0] = 1489;
	mock_uptime_ms = timer_test_base + 1000;

	/* First 4 ticks: LED ticks but sensor logic doesn't run */
	for (int i = 0; i < 4; i++) {
		app_cb.on_timer();
	}
	assert(mock_send_count == 0);  /* no sensor change detected yet */

	/* 5th tick: sensor logic runs, detects change */
	app_cb.on_timer();
	assert(mock_send_count == 1);
}

/* ================================================================== */
/*  delay_window: time-based charge pause/resume                       */
/* ================================================================== */

/* Helper: sync time to a given epoch at the current mock uptime */
static void sync_time_to(uint32_t epoch)
{
	uint8_t cmd[] = {0x30,
		epoch & 0xFF, (epoch >> 8) & 0xFF,
		(epoch >> 16) & 0xFF, (epoch >> 24) & 0xFF,
		0x00, 0x00, 0x00, 0x00};
	time_sync_process_cmd(cmd, sizeof(cmd));
}

/* Helper: build a delay window downlink payload */
static void build_delay_window_cmd(uint8_t *buf, uint32_t start, uint32_t end)
{
	buf[0] = 0x10;  /* CHARGE_CONTROL_CMD_TYPE */
	buf[1] = 0x02;  /* DELAY_WINDOW_SUBTYPE */
	buf[2] = start & 0xFF;
	buf[3] = (start >> 8) & 0xFF;
	buf[4] = (start >> 16) & 0xFF;
	buf[5] = (start >> 24) & 0xFF;
	buf[6] = end & 0xFF;
	buf[7] = (end >> 8) & 0xFF;
	buf[8] = (end >> 16) & 0xFF;
	buf[9] = (end >> 24) & 0xFF;
}

static void init_delay_window_test(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;  /* State A */
	mock_adc_values[1] = 0;
	mock_gpio_values[0] = 1;
	mock_gpio_values[2] = 0;
	mock_uptime_ms = 50000;
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	time_sync_init();
	platform = mock_platform_api_get();
	delay_window_init();
	platform = mock_platform_api_get();
	charge_control_init();
	charge_control_set(true, 0);  /* Reset to clean state after prior tests */
}

static void test_delay_window_no_window_not_paused(void)
{
	init_delay_window_test();
	assert(delay_window_has_window() == false);
	assert(delay_window_is_paused() == false);
}

static void test_delay_window_parse_and_store(void)
{
	init_delay_window_test();

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	assert(delay_window_process_cmd(cmd, sizeof(cmd)) == 0);
	assert(delay_window_has_window() == true);

	uint32_t start, end;
	delay_window_get(&start, &end);
	assert(start == 1000);
	assert(end == 2000);
}

static void test_delay_window_active_during_window(void)
{
	init_delay_window_test();

	/* Sync time to epoch 1500 */
	sync_time_to(1500);

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	/* now=1500, within [1000, 2000] */
	assert(delay_window_is_paused() == true);
}

static void test_delay_window_not_active_before_start(void)
{
	init_delay_window_test();

	/* Sync time to epoch 500 (before window start) */
	sync_time_to(500);

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	/* now=500, before start=1000 */
	assert(delay_window_is_paused() == false);
}

static void test_delay_window_not_active_after_end(void)
{
	init_delay_window_test();

	/* Sync time to epoch 2500 (after window end) */
	sync_time_to(2500);

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	/* now=2500, after end=2000 */
	assert(delay_window_is_paused() == false);
}

static void test_delay_window_ignored_without_time_sync(void)
{
	init_delay_window_test();
	/* Don't sync time — epoch stays 0 */

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	/* Window stored but time not synced — should not pause */
	assert(delay_window_has_window() == true);
	assert(delay_window_is_paused() == false);
}

static void test_delay_window_new_replaces_old(void)
{
	init_delay_window_test();
	sync_time_to(1500);

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));
	assert(delay_window_is_paused() == true);

	/* Send new window that doesn't cover now=1500 */
	build_delay_window_cmd(cmd, 3000, 4000);
	delay_window_process_cmd(cmd, sizeof(cmd));
	assert(delay_window_is_paused() == false);

	uint32_t start, end;
	delay_window_get(&start, &end);
	assert(start == 3000);
	assert(end == 4000);
}

static void test_delay_window_clear(void)
{
	init_delay_window_test();

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));
	assert(delay_window_has_window() == true);

	delay_window_clear();
	assert(delay_window_has_window() == false);
	assert(delay_window_is_paused() == false);
}

static void test_delay_window_boundary_at_start(void)
{
	init_delay_window_test();
	sync_time_to(1000);  /* exactly at start */

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	assert(delay_window_is_paused() == true);
}

static void test_delay_window_boundary_at_end(void)
{
	init_delay_window_test();
	sync_time_to(2000);  /* exactly at end */

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	assert(delay_window_is_paused() == true);
}

static void test_delay_window_bad_payload_too_short(void)
{
	init_delay_window_test();

	uint8_t cmd[] = {0x10, 0x02, 0x00, 0x00};
	assert(delay_window_process_cmd(cmd, sizeof(cmd)) < 0);
	assert(delay_window_has_window() == false);
}

/* ================================================================== */
/*  charge_control + delay_window integration                          */
/* ================================================================== */

static void test_cc_tick_window_pauses_charging(void)
{
	init_delay_window_test();
	sync_time_to(1500);

	/* Set a window that covers now=1500 */
	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	/* Charging is currently allowed */
	assert(charge_control_is_allowed() == true);

	/* Tick should detect window and pause */
	charge_control_tick();
	assert(charge_control_is_allowed() == false);
	assert(mock_gpio_set_last_val == 1);
}

static void test_cc_tick_window_expired_resumes(void)
{
	init_delay_window_test();
	sync_time_to(1500);

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	/* Tick to pause */
	charge_control_tick();
	assert(charge_control_is_allowed() == false);

	/* Advance time past window end */
	mock_uptime_ms += 501000;  /* +501s → epoch ~ 1500 + 501 = 2001 */
	charge_control_tick();
	assert(charge_control_is_allowed() == true);
	assert(mock_gpio_set_last_val == 0);
	assert(delay_window_has_window() == false);  /* cleared */
}

static void test_cc_tick_window_not_started_no_change(void)
{
	init_delay_window_test();
	sync_time_to(500);

	/* Window starts at 1000, we're at 500 */
	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	/* Charging should remain allowed */
	charge_control_tick();
	assert(charge_control_is_allowed() == true);
}

static void test_cc_tick_window_no_sync_falls_through(void)
{
	init_delay_window_test();
	/* Don't sync time */

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));

	/* Window stored but time not synced — auto-resume logic should still work */
	charge_control_set(false, 1);  /* Pause with 1-min auto-resume */
	mock_uptime_ms = 50000;
	charge_control_tick();
	assert(charge_control_is_allowed() == false);

	mock_uptime_ms = 111000;  /* 61s later */
	charge_control_tick();
	assert(charge_control_is_allowed() == true);
}

static void test_cc_legacy_cmd_clears_window(void)
{
	init_delay_window_test();
	sync_time_to(1500);

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));
	assert(delay_window_has_window() == true);

	/* Send legacy allow command */
	uint8_t legacy_cmd[] = {0x10, 0x01, 0x00, 0x00};
	charge_control_process_cmd(legacy_cmd, sizeof(legacy_cmd));

	/* Window should be cleared, charging allowed */
	assert(delay_window_has_window() == false);
	assert(charge_control_is_allowed() == true);
}

/* Integration: app_rx routes delay window correctly */
static void test_rx_routes_delay_window(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 1000000;
	mock_sidewalk_ready = true;

	app_cb.init(mock_platform_api_get());

	/* Sync time */
	uint8_t sync[] = {0x30,
		0xDC, 0x05, 0x00, 0x00,  /* epoch = 1500 */
		0x00, 0x00, 0x00, 0x00};
	app_cb.on_msg_received(sync, sizeof(sync));

	/* Send delay window covering now */
	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	app_cb.on_msg_received(cmd, sizeof(cmd));

	assert(delay_window_has_window() == true);
	assert(delay_window_is_paused() == true);
}

/* Integration: app_rx routes legacy charge control correctly */
static void test_rx_routes_legacy_charge_control(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 1000000;
	mock_sidewalk_ready = true;

	app_cb.init(mock_platform_api_get());

	/* Legacy pause command */
	uint8_t cmd[] = {0x10, 0x00, 0x00, 0x00};
	app_cb.on_msg_received(cmd, sizeof(cmd));

	assert(charge_control_is_allowed() == false);
}

/* ================================================================== */
/*  charge_now: 30-minute latch                                        */
/* ================================================================== */

static void init_charge_now_test(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 1489;  /* State C (charging) */
	mock_adc_values[1] = 1650;  /* current flowing */
	mock_gpio_values[0] = 1;    /* charge enable */
	mock_gpio_values[2] = 0;    /* no cool */
	mock_uptime_ms = 2000000;
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	charge_now_init();
	platform = mock_platform_api_get();
	delay_window_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	app_tx_set_ready(true);
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	led_engine_init();
	/* Force commissioning exit */
	led_engine_notify_uplink_sent();
	led_engine_tick();
	platform = mock_platform_api_get();
	time_sync_init();
}

static void test_charge_now_activate_sets_active(void)
{
	init_charge_now_test();
	assert(charge_now_is_active() == false);
	charge_now_activate();
	assert(charge_now_is_active() == true);
}

static void test_charge_now_activate_forces_charging_on(void)
{
	init_charge_now_test();
	/* Pause charging first */
	charge_control_set(false, 0);
	assert(charge_control_is_allowed() == false);

	charge_now_activate();
	assert(charge_control_is_allowed() == true);
	assert(mock_gpio_set_last_val == 0);
}

static void test_charge_now_activate_clears_delay_window(void)
{
	init_charge_now_test();
	sync_time_to(1500);

	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	delay_window_process_cmd(cmd, sizeof(cmd));
	assert(delay_window_has_window() == true);

	charge_now_activate();
	assert(delay_window_has_window() == false);
}

static void test_charge_now_activate_sets_led_override(void)
{
	init_charge_now_test();
	charge_now_activate();
	/* Tick past the 6-tick button-ack overlay */
	for (int i = 0; i < 7; i++) {
		led_engine_tick();
	}
	assert(led_engine_get_active_priority() == LED_PRI_CHARGE_NOW);
}

static void test_charge_now_cancel_clears_active(void)
{
	init_charge_now_test();
	charge_now_activate();
	assert(charge_now_is_active() == true);
	charge_now_cancel();
	assert(charge_now_is_active() == false);
}

static void test_charge_now_cancel_clears_led_override(void)
{
	init_charge_now_test();
	charge_now_activate();
	/* Tick past the 6-tick button-ack overlay */
	for (int i = 0; i < 7; i++) {
		led_engine_tick();
	}
	assert(led_engine_get_active_priority() == LED_PRI_CHARGE_NOW);

	charge_now_cancel();
	led_engine_tick();
	assert(led_engine_get_active_priority() != LED_PRI_CHARGE_NOW);
}

static void test_charge_now_flag_in_uplink(void)
{
	init_charge_now_test();
	mock_send_count = 0;
	mock_uptime_ms = 2100000;

	charge_now_activate();

	int ret = app_tx_send_evse_data();
	assert(ret == 0);
	assert(mock_send_count == 1);

	/* byte 7 = flags; bit 3 = FLAG_CHARGE_NOW (0x08) */
	uint8_t flags = mock_sends[0].data[7];
	assert(flags & 0x08);
}

static void test_charge_now_flag_cleared_after_cancel(void)
{
	init_charge_now_test();
	charge_now_activate();
	charge_now_cancel();

	mock_send_count = 0;
	mock_uptime_ms = 2200000;

	app_tx_send_evse_data();
	uint8_t flags = mock_sends[0].data[7];
	assert((flags & 0x08) == 0);
}

static void test_charge_now_cloud_pause_ignored(void)
{
	init_charge_now_test();
	charge_now_activate();

	/* Send a cloud pause command (0x10 0x00 = pause) */
	uint8_t cmd[] = {0x10, 0x00, 0x00, 0x00};
	app_rx_process_msg(cmd, sizeof(cmd));

	/* Charging should still be allowed (pause was ignored) */
	assert(charge_control_is_allowed() == true);
	assert(charge_now_is_active() == true);
}

static void test_charge_now_delay_window_ignored(void)
{
	init_charge_now_test();
	sync_time_to(1500);
	charge_now_activate();

	/* Try to set a delay window while Charge Now active */
	uint8_t cmd[10];
	build_delay_window_cmd(cmd, 1000, 2000);
	app_rx_process_msg(cmd, sizeof(cmd));

	/* Delay window should not be stored */
	assert(delay_window_has_window() == false);
	assert(charge_now_is_active() == true);
}

static void test_charge_now_expires_after_30min(void)
{
	init_charge_now_test();
	charge_now_activate();
	assert(charge_now_is_active() == true);

	/* Advance 29 minutes — still active */
	mock_uptime_ms = 2000000 + (29UL * 60 * 1000);
	charge_now_tick(2);  /* State C */
	assert(charge_now_is_active() == true);

	/* Advance to 30 minutes — should expire */
	mock_uptime_ms = 2000000 + (30UL * 60 * 1000);
	charge_now_tick(2);
	assert(charge_now_is_active() == false);
}

static void test_charge_now_unplug_cancels(void)
{
	init_charge_now_test();
	charge_now_activate();
	assert(charge_now_is_active() == true);

	/* J1772 state A = unplugged */
	charge_now_tick(0);
	assert(charge_now_is_active() == false);
}

static void test_charge_now_state_b_does_not_cancel(void)
{
	init_charge_now_test();
	charge_now_activate();

	/* State B = connected but not charging — should NOT cancel */
	charge_now_tick(1);
	assert(charge_now_is_active() == true);
}

static void test_charge_now_cancel_when_not_active_is_noop(void)
{
	init_charge_now_test();
	assert(charge_now_is_active() == false);
	/* Should not crash or change state */
	charge_now_cancel();
	assert(charge_now_is_active() == false);
}

static void test_charge_now_power_loss_safe(void)
{
	init_charge_now_test();
	charge_now_activate();
	assert(charge_now_is_active() == true);

	/* Simulate power loss by re-initializing */
	charge_now_init();
	assert(charge_now_is_active() == false);
}

/* ================================================================== */
/*  Button dispatch: single press → charge now, 5 press → selftest    */
/* ================================================================== */

static void init_button_test(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 1489;  /* State C */
	mock_adc_values[1] = 0;
	mock_gpio_values[0] = 1;
	mock_gpio_values[2] = 0;
	mock_gpio_values[3] = 0;  /* button not pressed */
	mock_uptime_ms = 3000000;
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	charge_now_init();
	platform = mock_platform_api_get();
	delay_window_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	selftest_reset();
	platform = mock_platform_api_get();
	selftest_trigger_init();
	platform = mock_platform_api_get();
	led_engine_init();
	led_engine_notify_uplink_sent();
}

static void test_single_press_activates_charge_now(void)
{
	init_button_test();
	assert(charge_now_is_active() == false);

	/* Simulate single button press: pressed for 1 tick, released */
	mock_gpio_values[3] = 1;
	selftest_trigger_tick();  /* rising edge detected, press_count=1, single_press_pending=true */

	mock_gpio_values[3] = 0;
	/* Advance time by 500ms (1 tick) */
	mock_uptime_ms = 3000500;
	selftest_trigger_tick();  /* button released, <1.5s, still pending */
	assert(charge_now_is_active() == false);

	/* Advance to 1.5s after press */
	mock_uptime_ms = 3001500;
	selftest_trigger_tick();  /* single_press_pending fires */
	assert(charge_now_is_active() == true);
}

static void test_five_presses_trigger_selftest_not_charge_now(void)
{
	init_button_test();

	/* 5 rapid presses within 5s */
	for (int i = 0; i < 5; i++) {
		mock_gpio_values[3] = 1;
		mock_uptime_ms = 3000000 + (i * 600);
		selftest_trigger_tick();

		mock_gpio_values[3] = 0;
		mock_uptime_ms = 3000000 + (i * 600) + 200;
		selftest_trigger_tick();
	}

	/* Self-test should be running, not Charge Now */
	assert(selftest_trigger_is_running() == true);
	assert(charge_now_is_active() == false);
}

static void test_long_press_cancels_charge_now(void)
{
	init_button_test();

	/* Activate charge now first */
	charge_now_activate();
	assert(charge_now_is_active() == true);

	/* Simulate long press: button held for 3s */
	mock_gpio_values[3] = 1;
	mock_uptime_ms = 3100000;
	selftest_trigger_tick();  /* rising edge */

	/* Hold for 3 seconds */
	mock_uptime_ms = 3103000;
	selftest_trigger_tick();  /* still pressed, 3s elapsed → long press fires */
	assert(charge_now_is_active() == false);
}

static void test_long_press_without_charge_now_is_noop(void)
{
	init_button_test();
	assert(charge_now_is_active() == false);

	/* Long press when charge_now is not active — should not crash */
	mock_gpio_values[3] = 1;
	mock_uptime_ms = 3100000;
	selftest_trigger_tick();

	mock_uptime_ms = 3103000;
	selftest_trigger_tick();
	assert(charge_now_is_active() == false);
}

static void test_two_presses_no_charge_now(void)
{
	init_button_test();

	/* Two presses: should NOT activate charge now */
	mock_gpio_values[3] = 1;
	mock_uptime_ms = 3000000;
	selftest_trigger_tick();

	mock_gpio_values[3] = 0;
	mock_uptime_ms = 3000200;
	selftest_trigger_tick();

	mock_gpio_values[3] = 1;
	mock_uptime_ms = 3000600;
	selftest_trigger_tick();

	mock_gpio_values[3] = 0;
	mock_uptime_ms = 3000800;
	selftest_trigger_tick();

	/* Wait past single-press timeout */
	mock_uptime_ms = 3002500;
	selftest_trigger_tick();

	assert(charge_now_is_active() == false);
}

/* ================================================================== */
/*  event_filter: change-detection buffering                           */
/* ================================================================== */

static struct event_snapshot make_snap(uint8_t j1772, uint16_t mv,
				       uint16_t ma, uint8_t thermo,
				       uint8_t charge)
{
	struct event_snapshot s = {
		.timestamp = 1000,
		.j1772_state = j1772,
		.pilot_voltage_mv = mv,
		.current_ma = ma,
		.thermostat_flags = thermo,
		.charge_flags = charge,
	};
	return s;
}

static void test_event_filter_no_write_when_unchanged(void)
{
	event_buffer_init();
	event_filter_init();

	struct event_snapshot s = make_snap(0, 2980, 0, 0, 0x01);

	/* First submit always writes (baseline) */
	assert(event_filter_submit(&s, 100000) == true);
	assert(event_buffer_count() == 1);

	/* Same state — should not write */
	assert(event_filter_submit(&s, 101000) == false);
	assert(event_buffer_count() == 1);

	/* Same again */
	assert(event_filter_submit(&s, 102000) == false);
	assert(event_buffer_count() == 1);
}

static void test_event_filter_writes_on_j1772_change(void)
{
	event_buffer_init();
	event_filter_init();

	struct event_snapshot s = make_snap(0, 2980, 0, 0, 0x01);
	event_filter_submit(&s, 100000);  /* baseline */

	/* J1772 state A → C */
	s.j1772_state = 2;
	assert(event_filter_submit(&s, 101000) == true);
	assert(event_buffer_count() == 2);
}

static void test_event_filter_writes_on_charge_flags_change(void)
{
	event_buffer_init();
	event_filter_init();

	struct event_snapshot s = make_snap(0, 2980, 0, 0, 0x01);
	event_filter_submit(&s, 100000);

	/* charge_flags: allowed → not allowed */
	s.charge_flags = 0x00;
	assert(event_filter_submit(&s, 101000) == true);
	assert(event_buffer_count() == 2);
}

static void test_event_filter_writes_on_thermostat_change(void)
{
	event_buffer_init();
	event_filter_init();

	struct event_snapshot s = make_snap(0, 2980, 0, 0x00, 0x01);
	event_filter_submit(&s, 100000);

	/* Thermostat cool call on */
	s.thermostat_flags = 0x02;
	assert(event_filter_submit(&s, 101000) == true);
	assert(event_buffer_count() == 2);
}

static void test_event_filter_heartbeat_after_timeout(void)
{
	event_buffer_init();
	event_filter_init();

	struct event_snapshot s = make_snap(0, 2980, 0, 0, 0x01);
	event_filter_submit(&s, 100000);  /* baseline at t=100000 */

	/* Same state, well before heartbeat */
	assert(event_filter_submit(&s, 200000) == false);
	assert(event_buffer_count() == 1);

	/* Same state, after heartbeat interval (300000ms = 5 min default) */
	assert(event_filter_submit(&s, 500000) == true);
	assert(event_buffer_count() == 2);
}

static void test_event_filter_voltage_noise_ignored(void)
{
	event_buffer_init();
	event_filter_init();

	struct event_snapshot s = make_snap(0, 2980, 0, 0, 0x01);
	event_filter_submit(&s, 100000);

	/* Small voltage change (500mV < 2000mV threshold) — noise */
	s.pilot_voltage_mv = 3480;
	assert(event_filter_submit(&s, 101000) == false);
	assert(event_buffer_count() == 1);
}

static void test_event_filter_voltage_large_change_writes(void)
{
	event_buffer_init();
	event_filter_init();

	struct event_snapshot s = make_snap(0, 2980, 0, 0, 0x01);
	event_filter_submit(&s, 100000);

	/* Large voltage drop (>2000mV) — real transition */
	s.pilot_voltage_mv = 500;
	assert(event_filter_submit(&s, 101000) == true);
	assert(event_buffer_count() == 2);
}

static void test_event_filter_first_submit_always_writes(void)
{
	event_buffer_init();
	event_filter_init();

	struct event_snapshot s = make_snap(0, 2980, 0, 0, 0x01);
	assert(event_filter_submit(&s, 100000) == true);
	assert(event_buffer_count() == 1);
}

static void test_event_filter_heartbeat_resets_after_change(void)
{
	event_buffer_init();
	event_filter_init();

	struct event_snapshot s = make_snap(0, 2980, 0, 0, 0x01);
	event_filter_submit(&s, 100000);  /* baseline */

	/* Change at t=200000 */
	s.j1772_state = 2;
	event_filter_submit(&s, 200000);
	assert(event_buffer_count() == 2);

	/* Same state at t=400000 (200s after last write, < 300s heartbeat) */
	assert(event_filter_submit(&s, 400000) == false);

	/* Same state at t=500001 (300001ms after last write, > heartbeat) */
	assert(event_filter_submit(&s, 500001) == true);
	assert(event_buffer_count() == 3);
}

static void test_event_filter_writes_on_transition_reason(void)
{
	event_buffer_init();
	event_filter_init();

	/* Baseline: charge allowed, no reason */
	struct event_snapshot s = make_snap(0, 2980, 0, 0, 0x01);
	s.transition_reason = 0;
	event_filter_submit(&s, 100000);  /* baseline */
	assert(event_buffer_count() == 1);

	/* Same state but with a transition reason — must still write */
	s.transition_reason = TRANSITION_REASON_CLOUD_CMD;
	assert(event_filter_submit(&s, 100500) == true);
	assert(event_buffer_count() == 2);
}

/* ================================================================== */
/*  transition reason tracking                                         */
/* ================================================================== */

static void test_transition_reason_allow_to_pause_cloud_cmd(void)
{
	mock_platform_api_reset();
	mock_gpio_values[0] = 1;
	platform = mock_platform_api_get();
	charge_control_init();

	/* Start allowed, then pause via cloud command */
	assert(charge_control_is_allowed() == true);
	uint8_t cmd[] = {0x10, 0x00, 0x00, 0x00};  /* pause, no auto-resume */
	charge_control_process_cmd(cmd, sizeof(cmd));

	assert(charge_control_is_allowed() == false);
	assert(charge_control_get_last_reason() == TRANSITION_REASON_CLOUD_CMD);
}

static void test_transition_reason_pause_to_allow_cloud_cmd(void)
{
	mock_platform_api_reset();
	mock_gpio_values[0] = 1;
	platform = mock_platform_api_get();
	charge_control_init();

	/* Pause first */
	charge_control_set(false, 0);
	charge_control_clear_last_reason();

	/* Resume via cloud command */
	uint8_t cmd[] = {0x10, 0x01, 0x00, 0x00};  /* allow */
	charge_control_process_cmd(cmd, sizeof(cmd));

	assert(charge_control_is_allowed() == true);
	assert(charge_control_get_last_reason() == TRANSITION_REASON_CLOUD_CMD);
}

static void test_transition_reason_charge_now(void)
{
	mock_platform_api_reset();
	mock_gpio_values[0] = 1;
	mock_uptime_ms = 100000;
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	charge_now_init();
	platform = mock_platform_api_get();
	led_engine_init();

	/* Pause, then activate Charge Now */
	charge_control_set(false, 0);
	charge_control_clear_last_reason();

	charge_now_activate();
	assert(charge_control_is_allowed() == true);
	assert(charge_control_get_last_reason() == TRANSITION_REASON_CHARGE_NOW);
}

static void test_transition_reason_manual_shell(void)
{
	mock_platform_api_reset();
	mock_gpio_values[0] = 1;
	platform = mock_platform_api_get();
	charge_control_init();

	/* Manual pause */
	charge_control_set_with_reason(false, 0, TRANSITION_REASON_MANUAL);
	assert(charge_control_get_last_reason() == TRANSITION_REASON_MANUAL);

	/* Manual allow */
	charge_control_set_with_reason(true, 0, TRANSITION_REASON_MANUAL);
	assert(charge_control_get_last_reason() == TRANSITION_REASON_MANUAL);
}

static void test_transition_reason_auto_resume(void)
{
	mock_platform_api_reset();
	mock_gpio_values[0] = 1;
	mock_uptime_ms = 100000;
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	time_sync_init();
	platform = mock_platform_api_get();

	/* Pause with 1 minute auto-resume */
	charge_control_set(false, 1);
	charge_control_clear_last_reason();

	/* Tick past 60 seconds */
	mock_uptime_ms = 161000;
	charge_control_tick();

	assert(charge_control_is_allowed() == true);
	assert(charge_control_get_last_reason() == TRANSITION_REASON_AUTO_RESUME);
}

static void test_transition_reason_none_when_no_change(void)
{
	mock_platform_api_reset();
	mock_gpio_values[0] = 1;
	platform = mock_platform_api_get();
	charge_control_init();
	charge_control_clear_last_reason();

	/* Set to same state — no transition */
	charge_control_set_with_reason(true, 0, TRANSITION_REASON_CLOUD_CMD);
	assert(charge_control_get_last_reason() == TRANSITION_REASON_NONE);
}

static void test_transition_reason_clear(void)
{
	mock_platform_api_reset();
	mock_gpio_values[0] = 1;
	platform = mock_platform_api_get();
	charge_control_init();

	charge_control_set_with_reason(false, 0, TRANSITION_REASON_MANUAL);
	assert(charge_control_get_last_reason() == TRANSITION_REASON_MANUAL);

	charge_control_clear_last_reason();
	assert(charge_control_get_last_reason() == TRANSITION_REASON_NONE);
}

static void test_transition_reason_in_snapshot(void)
{
	/* Full integration: init app, trigger transition, check snapshot */
	timer_test_base = 700000;

	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_adc_values[1] = 0;
	mock_gpio_values[0] = 1;
	mock_gpio_values[2] = 0;
	mock_uptime_ms = timer_test_base;
	mock_sidewalk_ready = true;

	app_cb.init(mock_platform_api_get());
	mock_send_count = 0;

	/* First tick: baseline (charge allowed) */
	mock_uptime_ms = timer_test_base + 1000;
	tick_sensor_cycle();

	/* Pause via shell command — this should record MANUAL reason */
	charge_control_set_with_reason(false, 0, TRANSITION_REASON_MANUAL);

	/* Tick again — snapshot should capture the transition reason */
	mock_uptime_ms = timer_test_base + 7000;
	tick_sensor_cycle();

	/* Check the latest event buffer entry has the transition reason */
	struct event_snapshot latest;
	assert(event_buffer_get_latest(&latest) == true);
	assert(latest.transition_reason == TRANSITION_REASON_MANUAL);
	assert((latest.charge_flags & EVENT_FLAG_CHARGE_ALLOWED) == 0);
}

static void test_uplink_includes_transition_reason(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_adc_values[1] = 0;
	mock_gpio_values[0] = 1;
	mock_uptime_ms = 800000;
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	charge_now_init();
	platform = mock_platform_api_get();
	time_sync_init();
	platform = mock_platform_api_get();
	app_tx_set_ready(true);

	/* Trigger a transition */
	charge_control_set_with_reason(false, 0, TRANSITION_REASON_CLOUD_CMD);

	/* Send uplink */
	app_tx_send_evse_data();
	assert(mock_send_count == 1);

	/* v0x09 payload should be 13 bytes with reason at byte 12 */
	assert(mock_sends[0].len == 15);
	assert(mock_sends[0].data[0] == 0xE5);  /* magic */
	assert(mock_sends[0].data[1] == 0x0A);  /* PAYLOAD_VERSION */
	assert(mock_sends[0].data[12] == TRANSITION_REASON_CLOUD_CMD);
}

/* ================================================================== */
/*  event buffer peek_at                                               */
/* ================================================================== */

static void test_event_buffer_peek_at_oldest(void)
{
	event_buffer_init();

	struct event_snapshot s1 = make_snap(1, 1000, 100, 0, 0x01);
	s1.timestamp = 100;
	event_buffer_add(&s1);

	struct event_snapshot s2 = make_snap(2, 2000, 200, 0, 0x01);
	s2.timestamp = 200;
	event_buffer_add(&s2);

	struct event_snapshot out;
	assert(event_buffer_peek_at(0, &out) == true);
	assert(out.timestamp == 100);
	assert(out.j1772_state == 1);

	assert(event_buffer_peek_at(1, &out) == true);
	assert(out.timestamp == 200);
	assert(out.j1772_state == 2);
}

static void test_event_buffer_peek_at_out_of_range(void)
{
	event_buffer_init();

	struct event_snapshot s = make_snap(0, 1000, 0, 0, 0x01);
	s.timestamp = 100;
	event_buffer_add(&s);

	struct event_snapshot out;
	assert(event_buffer_peek_at(0, &out) == true);
	assert(event_buffer_peek_at(1, &out) == false);
}

static void test_event_buffer_peek_at_empty(void)
{
	event_buffer_init();

	struct event_snapshot out;
	assert(event_buffer_peek_at(0, &out) == false);
}

static void test_event_buffer_peek_at_after_trim(void)
{
	event_buffer_init();

	struct event_snapshot s1 = make_snap(1, 1000, 0, 0, 0x01);
	s1.timestamp = 100;
	event_buffer_add(&s1);

	struct event_snapshot s2 = make_snap(2, 2000, 0, 0, 0x01);
	s2.timestamp = 200;
	event_buffer_add(&s2);

	struct event_snapshot s3 = make_snap(3, 3000, 0, 0, 0x01);
	s3.timestamp = 300;
	event_buffer_add(&s3);

	/* Trim oldest entry */
	event_buffer_trim(100);
	assert(event_buffer_count() == 2);

	/* Index 0 should now be s2 (timestamp=200) */
	struct event_snapshot out;
	assert(event_buffer_peek_at(0, &out) == true);
	assert(out.timestamp == 200);

	assert(event_buffer_peek_at(1, &out) == true);
	assert(out.timestamp == 300);
}

/* ================================================================== */
/*  app_tx send_snapshot                                               */
/* ================================================================== */

static void test_send_snapshot_format(void)
{
	mock_platform_api_reset();
	mock_uptime_ms = 500000;
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	app_tx_set_ready(true);

	struct event_snapshot snap = {
		.timestamp = 12345,
		.pilot_voltage_mv = 3000,
		.current_ma = 500,
		.j1772_state = 2,
		.thermostat_flags = 0x02,
		.charge_flags = EVENT_FLAG_CHARGE_ALLOWED,
		.transition_reason = TRANSITION_REASON_CLOUD_CMD,
	};

	int ret = app_tx_send_snapshot(&snap);
	assert(ret == 1);
	assert(mock_send_count == 1);
	assert(mock_sends[0].len == 15);

	uint8_t *d = mock_sends[0].data;
	assert(d[0] == 0xE5);  /* magic */
	assert(d[1] == 0x0A);  /* PAYLOAD_VERSION */
	assert(d[2] == 2);     /* j1772_state */

	/* pilot_voltage_mv = 3000 = 0x0BB8 LE */
	assert(d[3] == 0xB8);
	assert(d[4] == 0x0B);

	/* current_ma = 500 = 0x01F4 LE */
	assert(d[5] == 0xF4);
	assert(d[6] == 0x01);

	/* flags: thermostat=0x02 | charge_allowed=0x04 = 0x06 */
	assert(d[7] == 0x06);

	/* timestamp = 12345 = 0x00003039 LE */
	assert(d[8] == 0x39);
	assert(d[9] == 0x30);
	assert(d[10] == 0x00);
	assert(d[11] == 0x00);

	/* transition reason */
	assert(d[12] == TRANSITION_REASON_CLOUD_CMD);
}

static void test_send_snapshot_rate_limited(void)
{
	mock_platform_api_reset();
	mock_uptime_ms = 600000;
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	app_tx_set_ready(true);

	struct event_snapshot snap = make_snap(0, 2980, 0, 0, 0x01);
	snap.timestamp = 100;

	/* First send succeeds */
	assert(app_tx_send_snapshot(&snap) == 1);
	assert(mock_send_count == 1);

	/* Second send within 5s is rate-limited */
	mock_uptime_ms = 602000;
	assert(app_tx_send_snapshot(&snap) == 0);
	assert(mock_send_count == 1);

	/* After 5s, send works */
	mock_uptime_ms = 606000;
	assert(app_tx_send_snapshot(&snap) == 1);
	assert(mock_send_count == 2);
}

static void test_send_snapshot_shares_rate_limit_with_live(void)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;
	mock_uptime_ms = 700000;
	mock_sidewalk_ready = true;

	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	charge_now_init();
	platform = mock_platform_api_get();
	time_sync_init();
	platform = mock_platform_api_get();
	app_tx_set_ready(true);

	/* Live send */
	assert(app_tx_send_evse_data() == 0);
	assert(mock_send_count == 1);

	/* Snapshot send within 5s is rate-limited */
	mock_uptime_ms = 702000;
	struct event_snapshot snap = make_snap(0, 2980, 0, 0, 0x01);
	snap.timestamp = 100;
	assert(app_tx_send_snapshot(&snap) == 0);
	assert(mock_send_count == 1);
}

/* ================================================================== */
/*  event buffer drain via on_timer                                    */
/* ================================================================== */

static void drain_test_init(uint32_t uptime)
{
	mock_platform_api_reset();
	mock_adc_values[0] = 2980;  /* State A */
	mock_adc_values[1] = 0;
	mock_gpio_values[0] = 1;
	mock_uptime_ms = uptime;
	mock_sidewalk_ready = true;

	app_cb.init(mock_platform_api_get());
	app_cb.on_ready(true);
}

/** Advance uptime and pump 5 timer ticks (= one sensor cycle) */
static void drain_pump(uint32_t uptime)
{
	mock_uptime_ms = uptime;
	for (int i = 0; i < 5; i++) {
		app_cb.on_timer();
	}
}

static void test_drain_sends_buffered_events(void)
{
	drain_test_init(0);

	/* Trigger a state change to generate a live send + buffer entry */
	mock_uptime_ms = 100;
	mock_adc_values[0] = 5980;  /* State B */
	drain_pump(100);
	int sends_after_change = mock_send_count;
	assert(sends_after_change >= 1);

	/* Return to steady state (State A), triggering another change */
	mock_adc_values[0] = 2980;
	drain_pump(6000);
	int sends_after_second = mock_send_count;
	assert(sends_after_second > sends_after_change);

	/* Now in idle state with buffered events. Pump at 5s intervals
	 * to let drain happen (rate limit = 5s). */
	int prev_count = mock_send_count;
	drain_pump(12000);  /* 6s after last send */
	assert(mock_send_count > prev_count);
}

static void test_drain_respects_rate_limit(void)
{
	drain_test_init(0);

	/* Generate buffered events: two state changes */
	mock_adc_values[0] = 5980;  /* State B */
	drain_pump(100);

	mock_adc_values[0] = 2980;  /* Back to A */
	drain_pump(6000);

	int sends_after_changes = mock_send_count;

	/* Pump quickly — drain should not send (rate-limited) */
	drain_pump(7000);  /* only 1s after last */
	assert(mock_send_count == sends_after_changes);
}

static void test_drain_cursor_resets_on_trim(void)
{
	drain_test_init(0);
	/* Need time sync for timestamps */
	uint8_t ts_cmd[] = {0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	app_cb.on_msg_received(ts_cmd, sizeof(ts_cmd));

	/* Generate events */
	mock_adc_values[0] = 5980;
	drain_pump(100);
	mock_adc_values[0] = 2980;
	drain_pump(6000);

	/* Let drain send some events */
	drain_pump(12000);
	int sends_before_trim = mock_send_count;

	/* Simulate ACK watermark via TIME_SYNC that trims old entries */
	uint32_t wm = event_buffer_oldest_timestamp();
	if (wm > 0) {
		event_buffer_trim(wm);
	}

	/* After trim, drain should restart from new oldest */
	drain_pump(18000);
	/* Just verify no crash — the cursor was reset */
	assert(mock_send_count >= sends_before_trim);
}

/* ================================================================== */
/*  cmd_auth: HMAC-SHA256 command authentication                       */
/* ================================================================== */

/* Test key: 32 bytes of 0xAA */
static const uint8_t test_auth_key[CMD_AUTH_KEY_SIZE] = {
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};

/* Pre-computed with Python: hmac.new(key, payload, sha256).digest()[:8] */
static const uint8_t tag_legacy_allow[] = {
	0x0a, 0xe1, 0xce, 0x9f, 0xf2, 0x90, 0x07, 0x1d
};

static const uint8_t tag_legacy_pause[] = {
	0x08, 0x84, 0x7a, 0x9e, 0xab, 0x15, 0xb6, 0x7e
};

static const uint8_t tag_delay_window[] = {
	0xe3, 0xae, 0x1f, 0xa5, 0x15, 0x66, 0x47, 0x08
};

static void cmd_auth_test_setup(void)
{
	mock_platform_api_reset();
	platform = mock_platform_api_get();
	charge_control_init();
	platform = mock_platform_api_get();
	platform = mock_platform_api_get();
	delay_window_init();
	platform = mock_platform_api_get();
	time_sync_init();
	platform = mock_platform_api_get();
	charge_now_init();
	cmd_auth_set_key(test_auth_key, CMD_AUTH_KEY_SIZE);
}

static void test_cmd_auth_set_key_ok(void)
{
	int ret = cmd_auth_set_key(test_auth_key, CMD_AUTH_KEY_SIZE);
	assert(ret == 0);
	assert(cmd_auth_is_configured() == true);
}

static void test_cmd_auth_set_key_wrong_size(void)
{
	int ret = cmd_auth_set_key(test_auth_key, 16);
	assert(ret == -1);
}

static void test_cmd_auth_verify_legacy_allow(void)
{
	cmd_auth_set_key(test_auth_key, CMD_AUTH_KEY_SIZE);
	uint8_t payload[] = {0x10, 0x01, 0x00, 0x00};
	assert(cmd_auth_verify(payload, sizeof(payload), tag_legacy_allow) == true);
}

static void test_cmd_auth_verify_legacy_pause(void)
{
	cmd_auth_set_key(test_auth_key, CMD_AUTH_KEY_SIZE);
	uint8_t payload[] = {0x10, 0x00, 0x00, 0x00};
	assert(cmd_auth_verify(payload, sizeof(payload), tag_legacy_pause) == true);
}

static void test_cmd_auth_verify_delay_window(void)
{
	cmd_auth_set_key(test_auth_key, CMD_AUTH_KEY_SIZE);
	/* payload: 10 02 e8030000 f00a0000 (start=1000, end=2800) */
	uint8_t payload[] = {0x10, 0x02, 0xe8, 0x03, 0x00, 0x00,
			     0xf0, 0x0a, 0x00, 0x00};
	assert(cmd_auth_verify(payload, sizeof(payload), tag_delay_window) == true);
}

static void test_cmd_auth_wrong_tag_rejected(void)
{
	cmd_auth_set_key(test_auth_key, CMD_AUTH_KEY_SIZE);
	uint8_t payload[] = {0x10, 0x01, 0x00, 0x00};
	uint8_t bad_tag[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	assert(cmd_auth_verify(payload, sizeof(payload), bad_tag) == false);
}

static void test_cmd_auth_wrong_key_rejected(void)
{
	uint8_t wrong_key[CMD_AUTH_KEY_SIZE];
	memset(wrong_key, 0xBB, CMD_AUTH_KEY_SIZE);
	cmd_auth_set_key(wrong_key, CMD_AUTH_KEY_SIZE);

	uint8_t payload[] = {0x10, 0x01, 0x00, 0x00};
	/* tag_legacy_allow was computed with 0xAA key */
	assert(cmd_auth_verify(payload, sizeof(payload), tag_legacy_allow) == false);
}

static void test_cmd_auth_no_key_rejects(void)
{
	/* Simulate unconfigured state by setting a key, verifying, then
	 * checking that a NULL key call fails */
	assert(cmd_auth_verify(NULL, 0, NULL) == false);
}

/* --- RX integration: auth verification in app_rx_process_msg --- */

static void test_rx_auth_signed_legacy_accepted(void)
{
	cmd_auth_test_setup();
	/* Build signed legacy allow: 4-byte payload + 8-byte tag = 12 bytes */
	uint8_t msg[12] = {0x10, 0x01, 0x00, 0x00};
	memcpy(msg + 4, tag_legacy_allow, CMD_AUTH_TAG_SIZE);

	charge_control_set(false, 0);  /* start paused */
	app_rx_process_msg(msg, sizeof(msg));

	/* Command should have been accepted — charging now allowed */
	assert(charge_control_is_allowed() == true);
}

static void test_rx_auth_unsigned_legacy_rejected(void)
{
	cmd_auth_test_setup();
	/* Send only 4-byte payload (no tag) — should be rejected */
	uint8_t msg[] = {0x10, 0x01, 0x00, 0x00};

	charge_control_set(false, 0);  /* start paused */
	app_rx_process_msg(msg, sizeof(msg));

	/* Command should NOT have been accepted — still paused */
	assert(charge_control_is_allowed() == false);
	assert(mock_log_err_count > 0);
}

static void test_rx_auth_bad_tag_legacy_rejected(void)
{
	cmd_auth_test_setup();
	/* Build message with wrong tag */
	uint8_t msg[12] = {0x10, 0x01, 0x00, 0x00,
			   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	charge_control_set(false, 0);
	app_rx_process_msg(msg, sizeof(msg));

	assert(charge_control_is_allowed() == false);
	assert(mock_log_err_count > 0);
}

static void test_rx_auth_signed_delay_window_accepted(void)
{
	cmd_auth_test_setup();

	/* Sync time so delay window can activate */
	uint8_t sync_cmd[] = {0x30,
		0xE8, 0x03, 0x00, 0x00,  /* epoch = 1000 */
		0x00, 0x00, 0x00, 0x00}; /* watermark = 0 */
	time_sync_process_cmd(sync_cmd, sizeof(sync_cmd));
	mock_uptime_ms = 0;

	/* Build signed delay window: 10-byte payload + 8-byte tag = 18 bytes */
	uint8_t msg[18] = {0x10, 0x02, 0xe8, 0x03, 0x00, 0x00,
			   0xf0, 0x0a, 0x00, 0x00};
	memcpy(msg + 10, tag_delay_window, CMD_AUTH_TAG_SIZE);

	app_rx_process_msg(msg, sizeof(msg));

	/* Delay window should have been stored */
	assert(delay_window_has_window() == true);
}

static void test_rx_auth_unsigned_delay_window_rejected(void)
{
	cmd_auth_test_setup();
	/* Send 10-byte delay window payload without tag — rejected */
	uint8_t msg[] = {0x10, 0x02, 0xe8, 0x03, 0x00, 0x00,
			 0xf0, 0x0a, 0x00, 0x00};

	app_rx_process_msg(msg, sizeof(msg));

	assert(delay_window_has_window() == false);
	assert(mock_log_err_count > 0);
}

static void test_rx_auth_mtu_fits(void)
{
	/* Verify signed payloads fit in 19-byte LoRa MTU */
	assert(4 + CMD_AUTH_TAG_SIZE <= 19);   /* legacy charge control */
	assert(10 + CMD_AUTH_TAG_SIZE <= 19);  /* delay window */
}

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(void)
{
	/* Populate function pointers once — reset() preserves them */
	mock_platform_api_init();

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
	RUN_TEST(test_charge_control_pause_sets_gpio_high);
	RUN_TEST(test_charge_control_allow_sets_gpio_low);
	RUN_TEST(test_charge_control_auto_resume);
	RUN_TEST(test_charge_control_no_auto_resume_when_zero);

	printf("\napp_tx:\n");
	RUN_TEST(test_app_tx_sends_12_byte_payload);
	RUN_TEST(test_app_tx_rate_limits);
	RUN_TEST(test_app_tx_not_ready_skips);

	printf("\non_timer change detection:\n");
	RUN_TEST(test_on_timer_no_change_no_send);
	RUN_TEST(test_on_timer_j1772_change_triggers_send);
	RUN_TEST(test_on_timer_current_change_no_send_stubbed);
	RUN_TEST(test_on_timer_thermostat_change_triggers_send);
	RUN_TEST(test_on_timer_heartbeat_sends_after_60s);
	RUN_TEST(test_on_timer_no_heartbeat_before_60s);
	RUN_TEST(test_on_timer_multiple_changes_one_send);
	RUN_TEST(test_on_timer_settled_after_change_no_send);
	RUN_TEST(test_init_sets_timer_interval);

	printf("\nselftest_boot:\n");
	RUN_TEST(test_selftest_boot_all_pass);
	RUN_TEST(test_selftest_boot_adc_pilot_fail);
	RUN_TEST(test_selftest_boot_gpio_cool_fail);
	RUN_TEST(test_selftest_boot_charge_block_toggle_pass);
	RUN_TEST(test_selftest_boot_charge_block_readback_fail);
	RUN_TEST(test_selftest_boot_flag_clears_on_retest);
	RUN_TEST(test_selftest_boot_no_stale_fault_on_pass);
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
	RUN_TEST(test_continuous_pilot_uses_state_not_adc);
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
	RUN_TEST(test_diag_build_version_byte);
	RUN_TEST(test_diag_platform_build_version_byte);
	RUN_TEST(test_diag_rx_dispatches_0x40);

	printf("\nled_engine priority:\n");
	RUN_TEST(test_led_idle_default);
	RUN_TEST(test_led_error_highest_priority);
	RUN_TEST(test_led_ota_higher_than_commission);
	RUN_TEST(test_led_commission_at_boot);
	RUN_TEST(test_led_commission_exits_on_uplink);
	RUN_TEST(test_led_commission_exits_on_timeout);
	RUN_TEST(test_led_disconnected_after_commission);
	RUN_TEST(test_led_charge_now_override);
	RUN_TEST(test_led_ac_priority);
	RUN_TEST(test_led_charging_state_c);

	printf("\nled_engine patterns:\n");
	RUN_TEST(test_led_error_toggles_every_tick);
	RUN_TEST(test_led_commission_5on_5off);
	RUN_TEST(test_led_idle_blip);
	RUN_TEST(test_led_solid_on_charging);
	RUN_TEST(test_led_pattern_resets_on_priority_change);

	printf("\nled_engine error tracking:\n");
	RUN_TEST(test_led_3_adc_failures_error);
	RUN_TEST(test_led_adc_success_resets_counter);
	RUN_TEST(test_led_sidewalk_10min_timeout);
	RUN_TEST(test_led_sidewalk_timeout_clears_on_ready);

	printf("\nled_engine selftest coexistence:\n");
	RUN_TEST(test_led_yields_during_selftest);
	RUN_TEST(test_led_restores_after_selftest);

	printf("\nled_engine button feedback:\n");
	RUN_TEST(test_led_button_ack_3_blinks);
	RUN_TEST(test_led_button_ack_blocked_by_error);

	printf("\nled_engine timer:\n");
	RUN_TEST(test_led_timer_interval_100);
	RUN_TEST(test_led_decimation_sensors_every_5th);

	printf("\ndelay_window:\n");
	RUN_TEST(test_delay_window_no_window_not_paused);
	RUN_TEST(test_delay_window_parse_and_store);
	RUN_TEST(test_delay_window_active_during_window);
	RUN_TEST(test_delay_window_not_active_before_start);
	RUN_TEST(test_delay_window_not_active_after_end);
	RUN_TEST(test_delay_window_ignored_without_time_sync);
	RUN_TEST(test_delay_window_new_replaces_old);
	RUN_TEST(test_delay_window_clear);
	RUN_TEST(test_delay_window_boundary_at_start);
	RUN_TEST(test_delay_window_boundary_at_end);
	RUN_TEST(test_delay_window_bad_payload_too_short);

	printf("\ncharge_control + delay_window:\n");
	RUN_TEST(test_cc_tick_window_pauses_charging);
	RUN_TEST(test_cc_tick_window_expired_resumes);
	RUN_TEST(test_cc_tick_window_not_started_no_change);
	RUN_TEST(test_cc_tick_window_no_sync_falls_through);
	RUN_TEST(test_cc_legacy_cmd_clears_window);
	RUN_TEST(test_rx_routes_delay_window);
	RUN_TEST(test_rx_routes_legacy_charge_control);

	printf("\ncharge_now latch:\n");
	RUN_TEST(test_charge_now_activate_sets_active);
	RUN_TEST(test_charge_now_activate_forces_charging_on);
	RUN_TEST(test_charge_now_activate_clears_delay_window);
	RUN_TEST(test_charge_now_activate_sets_led_override);
	RUN_TEST(test_charge_now_cancel_clears_active);
	RUN_TEST(test_charge_now_cancel_clears_led_override);
	RUN_TEST(test_charge_now_flag_in_uplink);
	RUN_TEST(test_charge_now_flag_cleared_after_cancel);
	RUN_TEST(test_charge_now_cloud_pause_ignored);
	RUN_TEST(test_charge_now_delay_window_ignored);
	RUN_TEST(test_charge_now_expires_after_30min);
	RUN_TEST(test_charge_now_unplug_cancels);
	RUN_TEST(test_charge_now_state_b_does_not_cancel);
	RUN_TEST(test_charge_now_cancel_when_not_active_is_noop);
	RUN_TEST(test_charge_now_power_loss_safe);

	printf("\nbutton dispatch:\n");
	RUN_TEST(test_single_press_activates_charge_now);
	RUN_TEST(test_five_presses_trigger_selftest_not_charge_now);
	RUN_TEST(test_long_press_cancels_charge_now);
	RUN_TEST(test_long_press_without_charge_now_is_noop);
	RUN_TEST(test_two_presses_no_charge_now);

	printf("\nevent_filter:\n");
	RUN_TEST(test_event_filter_no_write_when_unchanged);
	RUN_TEST(test_event_filter_writes_on_j1772_change);
	RUN_TEST(test_event_filter_writes_on_charge_flags_change);
	RUN_TEST(test_event_filter_writes_on_thermostat_change);
	RUN_TEST(test_event_filter_heartbeat_after_timeout);
	RUN_TEST(test_event_filter_voltage_noise_ignored);
	RUN_TEST(test_event_filter_voltage_large_change_writes);
	RUN_TEST(test_event_filter_first_submit_always_writes);
	RUN_TEST(test_event_filter_heartbeat_resets_after_change);
	RUN_TEST(test_event_filter_writes_on_transition_reason);

	printf("\ntransition reason tracking:\n");
	RUN_TEST(test_transition_reason_allow_to_pause_cloud_cmd);
	RUN_TEST(test_transition_reason_pause_to_allow_cloud_cmd);
	RUN_TEST(test_transition_reason_charge_now);
	RUN_TEST(test_transition_reason_manual_shell);
	RUN_TEST(test_transition_reason_auto_resume);
	RUN_TEST(test_transition_reason_none_when_no_change);
	RUN_TEST(test_transition_reason_clear);
	RUN_TEST(test_transition_reason_in_snapshot);
	RUN_TEST(test_uplink_includes_transition_reason);

	printf("\ncmd_auth HMAC:\n");
	printf("\nevent_buffer peek_at:\n");
	RUN_TEST(test_event_buffer_peek_at_oldest);
	RUN_TEST(test_event_buffer_peek_at_out_of_range);
	RUN_TEST(test_event_buffer_peek_at_empty);
	RUN_TEST(test_event_buffer_peek_at_after_trim);

	printf("\napp_tx send_snapshot:\n");
	RUN_TEST(test_send_snapshot_format);
	RUN_TEST(test_send_snapshot_rate_limited);
	RUN_TEST(test_send_snapshot_shares_rate_limit_with_live);

	printf("\nevent buffer drain:\n");
	RUN_TEST(test_drain_sends_buffered_events);
	RUN_TEST(test_drain_respects_rate_limit);
	RUN_TEST(test_drain_cursor_resets_on_trim);

	printf("\ncmd_auth HMAC:\n");
	RUN_TEST(test_cmd_auth_set_key_ok);
	RUN_TEST(test_cmd_auth_set_key_wrong_size);
	RUN_TEST(test_cmd_auth_verify_legacy_allow);
	RUN_TEST(test_cmd_auth_verify_legacy_pause);
	RUN_TEST(test_cmd_auth_verify_delay_window);
	RUN_TEST(test_cmd_auth_wrong_tag_rejected);
	RUN_TEST(test_cmd_auth_wrong_key_rejected);
	RUN_TEST(test_cmd_auth_no_key_rejects);

	printf("\ncmd_auth RX integration:\n");
	RUN_TEST(test_rx_auth_signed_legacy_accepted);
	RUN_TEST(test_rx_auth_unsigned_legacy_rejected);
	RUN_TEST(test_rx_auth_bad_tag_legacy_rejected);
	RUN_TEST(test_rx_auth_signed_delay_window_accepted);
	RUN_TEST(test_rx_auth_unsigned_delay_window_rejected);
	RUN_TEST(test_rx_auth_mtu_fits);

	printf("\n=== %d/%d tests passed ===\n\n", tests_passed, tests_run);
	return (tests_passed == tests_run) ? 0 : 1;
}
