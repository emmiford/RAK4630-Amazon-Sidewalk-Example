/*
 * Unit tests for shell command dispatch in app_entry.c
 *
 * Tests app_on_shell_cmd() by calling through the app_cb.on_shell_cmd
 * function pointer with mock print/error callbacks that capture output.
 */

#include "unity.h"
#include "mock_platform_api.h"
#include "platform_api.h"
#include "evse_sensors.h"
#include "charge_control.h"
#include "thermostat_inputs.h"
#include "evse_payload.h"
#include "app_tx.h"
#include "app_rx.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Access the app callback table (compiled with HOST_TEST) */
extern const struct app_callbacks app_cb;

/* ------------------------------------------------------------------ */
/*  Capture buffers for shell_print / shell_error                      */
/* ------------------------------------------------------------------ */

#define CAPTURE_BUF_SIZE  2048
#define CAPTURE_LINE_MAX  32

static char print_buf[CAPTURE_BUF_SIZE];
static int  print_offset;
static int  print_line_count;
static char print_lines[CAPTURE_LINE_MAX][128];

static char error_buf[CAPTURE_BUF_SIZE];
static int  error_offset;
static int  error_line_count;
static char error_lines[CAPTURE_LINE_MAX][128];

static void capture_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int n = vsnprintf(print_buf + print_offset,
			  CAPTURE_BUF_SIZE - print_offset, fmt, args);
	va_end(args);

	/* Also store as a discrete line */
	if (print_line_count < CAPTURE_LINE_MAX) {
		va_start(args, fmt);
		vsnprintf(print_lines[print_line_count],
			  sizeof(print_lines[0]), fmt, args);
		va_end(args);
		print_line_count++;
	}

	if (n > 0) {
		print_offset += n;
		/* Add newline separator */
		if (print_offset < CAPTURE_BUF_SIZE - 1) {
			print_buf[print_offset++] = '\n';
			print_buf[print_offset] = '\0';
		}
	}
}

static void capture_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int n = vsnprintf(error_buf + error_offset,
			  CAPTURE_BUF_SIZE - error_offset, fmt, args);
	va_end(args);

	/* Also store as a discrete line */
	if (error_line_count < CAPTURE_LINE_MAX) {
		va_start(args, fmt);
		vsnprintf(error_lines[error_line_count],
			  sizeof(error_lines[0]), fmt, args);
		va_end(args);
		error_line_count++;
	}

	if (n > 0) {
		error_offset += n;
		if (error_offset < CAPTURE_BUF_SIZE - 1) {
			error_buf[error_offset++] = '\n';
			error_buf[error_offset] = '\0';
		}
	}
}

static void capture_reset(void)
{
	memset(print_buf, 0, sizeof(print_buf));
	print_offset = 0;
	print_line_count = 0;
	memset(print_lines, 0, sizeof(print_lines));

	memset(error_buf, 0, sizeof(error_buf));
	error_offset = 0;
	error_line_count = 0;
	memset(error_lines, 0, sizeof(error_lines));
}

/* ------------------------------------------------------------------ */
/*  Helper: check if any print line contains a substring               */
/* ------------------------------------------------------------------ */

static bool print_output_contains(const char *substring)
{
	return strstr(print_buf, substring) != NULL;
}

static bool error_output_contains(const char *substring)
{
	return strstr(error_buf, substring) != NULL;
}

/* ------------------------------------------------------------------ */
/*  setUp / tearDown                                                   */
/* ------------------------------------------------------------------ */

static const struct platform_api *api;

void setUp(void)
{
	api = mock_platform_api_init();
	mock_sidewalk_ready = true;

	/* Initialize the app through the callback table, which wires
	 * up all internal modules (evse_sensors, charge_control, etc.) */
	app_cb.init(api);

	/* Cancel any leftover simulation */
	evse_sensors_simulate_state(0, 0);

	capture_reset();
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */
/*  evse status                                                        */
/* ------------------------------------------------------------------ */

void test_evse_status_returns_zero(void)
{
	mock_adc_values[0] = 3000; /* State A */
	mock_adc_values[1] = 0;    /* 0 mA */

	int rc = app_cb.on_shell_cmd("evse", "status", capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_evse_status_prints_j1772_state(void)
{
	mock_adc_values[0] = 3000; /* State A */
	app_cb.on_shell_cmd("evse", "status", capture_print, capture_error);

	TEST_ASSERT_TRUE(print_output_contains("J1772 state"));
	TEST_ASSERT_TRUE(print_output_contains("A (Not connected)"));
}

void test_evse_status_prints_voltage(void)
{
	mock_adc_values[0] = 2200; /* State B */
	app_cb.on_shell_cmd("evse", "status", capture_print, capture_error);

	TEST_ASSERT_TRUE(print_output_contains("Pilot voltage"));
	TEST_ASSERT_TRUE(print_output_contains("2200 mV"));
}

void test_evse_status_prints_current(void)
{
	mock_adc_values[0] = 3000;
	app_cb.on_shell_cmd("evse", "status", capture_print, capture_error);

	TEST_ASSERT_TRUE(print_output_contains("Current"));
	TEST_ASSERT_TRUE(print_output_contains("0 mA"));
}

void test_evse_status_prints_charging_allowed(void)
{
	mock_adc_values[0] = 3000;
	app_cb.on_shell_cmd("evse", "status", capture_print, capture_error);

	TEST_ASSERT_TRUE(print_output_contains("Charging allowed"));
	/* Default after init is allowed */
	TEST_ASSERT_TRUE(print_output_contains("YES"));
}

void test_evse_status_prints_simulation_inactive(void)
{
	mock_adc_values[0] = 3000;
	app_cb.on_shell_cmd("evse", "status", capture_print, capture_error);

	TEST_ASSERT_TRUE(print_output_contains("Simulation active"));
	/* No simulation running */
	TEST_ASSERT_TRUE(print_output_contains("NO"));
}

void test_evse_status_null_args_shows_status(void)
{
	/* NULL args should fall through to "status" (same as no subcommand) */
	mock_adc_values[0] = 3000;
	int rc = app_cb.on_shell_cmd("evse", NULL, capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(0, rc);
	TEST_ASSERT_TRUE(print_output_contains("EVSE Status"));
}

/* ------------------------------------------------------------------ */
/*  evse a / b / c — simulation triggers                               */
/* ------------------------------------------------------------------ */

void test_evse_a_triggers_simulation(void)
{
	mock_uptime_ms = 1000;
	int rc = app_cb.on_shell_cmd("evse", "a", capture_print, capture_error);

	TEST_ASSERT_EQUAL_INT(0, rc);
	TEST_ASSERT_TRUE(print_output_contains("State A"));
	TEST_ASSERT_TRUE(evse_sensors_is_simulating());

	/* Verify actual simulated state is A */
	j1772_state_t state;
	evse_j1772_state_get(&state, NULL);
	TEST_ASSERT_EQUAL(J1772_STATE_A, state);
}

void test_evse_b_triggers_simulation(void)
{
	mock_uptime_ms = 1000;
	int rc = app_cb.on_shell_cmd("evse", "b", capture_print, capture_error);

	TEST_ASSERT_EQUAL_INT(0, rc);
	TEST_ASSERT_TRUE(print_output_contains("State B"));
	TEST_ASSERT_TRUE(evse_sensors_is_simulating());

	j1772_state_t state;
	evse_j1772_state_get(&state, NULL);
	TEST_ASSERT_EQUAL(J1772_STATE_B, state);
}

void test_evse_c_triggers_simulation(void)
{
	mock_uptime_ms = 1000;
	int rc = app_cb.on_shell_cmd("evse", "c", capture_print, capture_error);

	TEST_ASSERT_EQUAL_INT(0, rc);
	TEST_ASSERT_TRUE(print_output_contains("State C"));
	TEST_ASSERT_TRUE(evse_sensors_is_simulating());

	j1772_state_t state;
	evse_j1772_state_get(&state, NULL);
	TEST_ASSERT_EQUAL(J1772_STATE_C, state);
}

void test_evse_simulation_sends_uplink(void)
{
	mock_uptime_ms = 1000;
	mock_send_count = 0;
	app_cb.on_shell_cmd("evse", "b", capture_print, capture_error);

	/* Each simulation command triggers app_tx_send_evse_data */
	TEST_ASSERT_GREATER_OR_EQUAL(1, mock_send_count);
}

/* ------------------------------------------------------------------ */
/*  evse allow / pause                                                 */
/* ------------------------------------------------------------------ */

void test_evse_allow_enables_charging(void)
{
	/* First pause, then allow */
	charge_control_set(false, 0);
	TEST_ASSERT_FALSE(charge_control_is_allowed());

	int rc = app_cb.on_shell_cmd("evse", "allow", capture_print, capture_error);

	TEST_ASSERT_EQUAL_INT(0, rc);
	TEST_ASSERT_TRUE(charge_control_is_allowed());
	TEST_ASSERT_TRUE(print_output_contains("ALLOWED"));
}

void test_evse_pause_disables_charging(void)
{
	TEST_ASSERT_TRUE(charge_control_is_allowed());

	int rc = app_cb.on_shell_cmd("evse", "pause", capture_print, capture_error);

	TEST_ASSERT_EQUAL_INT(0, rc);
	TEST_ASSERT_FALSE(charge_control_is_allowed());
	TEST_ASSERT_TRUE(print_output_contains("PAUSED"));
}

void test_evse_allow_sets_gpio_low(void)
{
	charge_control_set(false, 0);
	app_cb.on_shell_cmd("evse", "allow", capture_print, capture_error);

	TEST_ASSERT_EQUAL_INT(0, mock_gpio_set_last_pin);
	TEST_ASSERT_EQUAL_INT(0, mock_gpio_set_last_val);
}

void test_evse_pause_sets_gpio_high(void)
{
	app_cb.on_shell_cmd("evse", "pause", capture_print, capture_error);

	TEST_ASSERT_EQUAL_INT(0, mock_gpio_set_last_pin);
	TEST_ASSERT_EQUAL_INT(1, mock_gpio_set_last_val);
}

/* ------------------------------------------------------------------ */
/*  hvac status                                                        */
/* ------------------------------------------------------------------ */

void test_hvac_status_returns_zero(void)
{
	int rc = app_cb.on_shell_cmd("hvac", "status", capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_hvac_status_prints_flags(void)
{
	mock_gpio_values[2] = 0; /* cool off */

	app_cb.on_shell_cmd("hvac", "status", capture_print, capture_error);

	TEST_ASSERT_TRUE(print_output_contains("Thermostat flags"));
	TEST_ASSERT_TRUE(print_output_contains("Cool"));
}

void test_hvac_status_cool_on(void)
{
	mock_gpio_values[2] = 1; /* cool on */

	app_cb.on_shell_cmd("hvac", "status", capture_print, capture_error);

	TEST_ASSERT_TRUE(print_output_contains("Cool: ON"));
}

void test_hvac_null_args_shows_status(void)
{
	int rc = app_cb.on_shell_cmd("hvac", NULL, capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(0, rc);
	TEST_ASSERT_TRUE(print_output_contains("Thermostat flags"));
}

void test_hvac_call_alias_shows_status(void)
{
	int rc = app_cb.on_shell_cmd("hvac", "call", capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(0, rc);
	TEST_ASSERT_TRUE(print_output_contains("Thermostat flags"));
}

/* ------------------------------------------------------------------ */
/*  sid send                                                           */
/* ------------------------------------------------------------------ */

void test_sid_send_returns_zero(void)
{
	mock_send_count = 0;
	int rc = app_cb.on_shell_cmd("sid", "send", capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_sid_send_triggers_uplink(void)
{
	mock_send_count = 0;
	app_cb.on_shell_cmd("sid", "send", capture_print, capture_error);

	TEST_ASSERT_GREATER_OR_EQUAL(1, mock_send_count);
	TEST_ASSERT_TRUE(print_output_contains("Send queued"));
}

void test_sid_send_failure_prints_error(void)
{
	mock_sidewalk_ready = false;
	int rc = app_cb.on_shell_cmd("sid", "send", capture_print, capture_error);

	TEST_ASSERT_NOT_EQUAL(0, rc);
	TEST_ASSERT_TRUE(error_output_contains("Send failed"));
}

/* ------------------------------------------------------------------ */
/*  Unknown commands                                                   */
/* ------------------------------------------------------------------ */

void test_unknown_command_returns_error(void)
{
	int rc = app_cb.on_shell_cmd("foobar", "baz", capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(-1, rc);
}

void test_unknown_command_prints_error_message(void)
{
	app_cb.on_shell_cmd("foobar", "baz", capture_print, capture_error);

	TEST_ASSERT_TRUE(error_output_contains("Unknown app command"));
	TEST_ASSERT_TRUE(error_output_contains("foobar"));
}

void test_unknown_evse_subcommand_returns_error(void)
{
	int rc = app_cb.on_shell_cmd("evse", "xyz", capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(-1, rc);
	TEST_ASSERT_TRUE(error_output_contains("Unknown evse subcommand"));
	TEST_ASSERT_TRUE(error_output_contains("xyz"));
}

void test_unknown_hvac_subcommand_returns_error(void)
{
	int rc = app_cb.on_shell_cmd("hvac", "xyz", capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(-1, rc);
	TEST_ASSERT_TRUE(error_output_contains("Unknown hvac subcommand"));
	TEST_ASSERT_TRUE(error_output_contains("xyz"));
}

/* ------------------------------------------------------------------ */
/*  NULL / empty args safety                                           */
/* ------------------------------------------------------------------ */

void test_unknown_cmd_null_args_safe(void)
{
	/* Should not crash with NULL args on an unknown command */
	int rc = app_cb.on_shell_cmd("unknown", NULL, capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(-1, rc);
}

void test_sid_null_args_returns_error(void)
{
	/* "sid" with NULL args does not match "send" so falls through to unknown */
	int rc = app_cb.on_shell_cmd("sid", NULL, capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(-1, rc);
}

void test_evse_empty_string_args_shows_status(void)
{
	/* Empty string ("") is not NULL — strcmp with "status" will fail,
	 * but the code checks !args first, so empty string goes to subcommand
	 * matching. Empty string won't match any, so it's "unknown subcommand". */
	mock_adc_values[0] = 3000;
	int rc = app_cb.on_shell_cmd("evse", "", capture_print, capture_error);
	TEST_ASSERT_EQUAL_INT(-1, rc);
	TEST_ASSERT_TRUE(error_output_contains("Unknown evse subcommand"));
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
	UNITY_BEGIN();

	/* evse status */
	RUN_TEST(test_evse_status_returns_zero);
	RUN_TEST(test_evse_status_prints_j1772_state);
	RUN_TEST(test_evse_status_prints_voltage);
	RUN_TEST(test_evse_status_prints_current);
	RUN_TEST(test_evse_status_prints_charging_allowed);
	RUN_TEST(test_evse_status_prints_simulation_inactive);
	RUN_TEST(test_evse_status_null_args_shows_status);

	/* evse a/b/c simulation */
	RUN_TEST(test_evse_a_triggers_simulation);
	RUN_TEST(test_evse_b_triggers_simulation);
	RUN_TEST(test_evse_c_triggers_simulation);
	RUN_TEST(test_evse_simulation_sends_uplink);

	/* evse allow/pause */
	RUN_TEST(test_evse_allow_enables_charging);
	RUN_TEST(test_evse_pause_disables_charging);
	RUN_TEST(test_evse_allow_sets_gpio_low);
	RUN_TEST(test_evse_pause_sets_gpio_high);

	/* hvac status */
	RUN_TEST(test_hvac_status_returns_zero);
	RUN_TEST(test_hvac_status_prints_flags);
	RUN_TEST(test_hvac_status_cool_on);
	RUN_TEST(test_hvac_null_args_shows_status);
	RUN_TEST(test_hvac_call_alias_shows_status);

	/* sid send */
	RUN_TEST(test_sid_send_returns_zero);
	RUN_TEST(test_sid_send_triggers_uplink);
	RUN_TEST(test_sid_send_failure_prints_error);

	/* Unknown commands */
	RUN_TEST(test_unknown_command_returns_error);
	RUN_TEST(test_unknown_command_prints_error_message);
	RUN_TEST(test_unknown_evse_subcommand_returns_error);
	RUN_TEST(test_unknown_hvac_subcommand_returns_error);

	/* NULL/empty args safety */
	RUN_TEST(test_unknown_cmd_null_args_safe);
	RUN_TEST(test_sid_null_args_returns_error);
	RUN_TEST(test_evse_empty_string_args_shows_status);

	return UNITY_END();
}
