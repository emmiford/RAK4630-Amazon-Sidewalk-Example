/*
 * App Entry Point — callback table and lifecycle
 *
 * This file is the single entry point from the platform into the app.
 * It contains the app_callbacks struct at a fixed address (.app_header)
 * and dispatches platform callbacks to the app modules.
 *
 * The app owns all EVSE domain knowledge: sensor interpretation, change
 * detection, payload format, command handling.  The platform provides
 * generic services (ADC, GPIO, timer, Sidewalk, shell).
 */

#include <platform_api.h>
#include <evse_sensors.h>
#include <charge_control.h>
#include <charge_now.h>
#include <thermostat_inputs.h>
#include <evse_payload.h>
#include <app_tx.h>
#include <app_rx.h>
#include <cmd_auth.h>
#include <delay_window.h>
#include <diag_request.h>
#include <selftest.h>
#include <selftest_trigger.h>
#include <time_sync.h>
#include <event_buffer.h>
#include <event_filter.h>
#include <led_engine.h>
#include <string.h>

static const struct platform_api *api;

/* ------------------------------------------------------------------ */
/*  Polling and change detection                                       */
/* ------------------------------------------------------------------ */

#define POLL_INTERVAL_MS        100
#define SENSOR_DECIMATION       5
#ifndef HEARTBEAT_INTERVAL_MS
#define HEARTBEAT_INTERVAL_MS   900000   /* 15 min; override with -DHEARTBEAT_INTERVAL_MS=60000 for dev */
#endif
#define CURRENT_ON_THRESHOLD_MA 500

static uint8_t decimation_counter;
static j1772_state_t last_j1772_state;
static bool last_current_on;
static uint8_t last_thermostat_flags;
static uint32_t last_heartbeat_ms;

/* ------------------------------------------------------------------ */
/*  Shell command dispatch table                                       */
/* ------------------------------------------------------------------ */

#define SIMULATION_DURATION_MS 10000

static int shell_evse_status(void (*print)(const char *, ...), void (*error)(const char *, ...))
{
	j1772_state_t state;
	uint16_t voltage_mv = 0;
	uint16_t current_ma = 0;

	int err = evse_j1772_state_get(&state, &voltage_mv);
	if (err) {
		error("Failed to read J1772 state: %d", err);
	}

	err = evse_current_read(&current_ma);
	if (err) {
		error("Failed to read current: %d", err);
	}

	charge_control_state_t cc_state;
	charge_control_get_state(&cc_state);

	print("EVSE Status:");
	print("  J1772 state: %s", j1772_state_to_string(state));
	print("  Pilot voltage: %d mV", voltage_mv);
	print("  Current: %d mA", current_ma);
	print("  Charging allowed: %s", cc_state.charging_allowed ? "YES" : "NO");
	print("  Charge Now active: %s", charge_now_is_active() ? "YES" : "NO");
	print("  Simulation active: %s", evse_sensors_is_simulating() ? "YES" : "NO");
	return 0;
}

static int shell_hvac_status(void (*print)(const char *, ...), void (*error)(const char *, ...))
{
	(void)error;
	uint8_t flags = thermostat_flags_get();
	print("Thermostat flags: 0x%02x", flags);
	print("  Cool: %s", (flags & 0x02) ? "ON" : "OFF");
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Callback implementations                                          */
/* ------------------------------------------------------------------ */

static int app_init(const struct platform_api *platform)
{
	api = platform;

	/* Pass API to all app modules */
	evse_sensors_set_api(api);
	charge_control_set_api(api);
	thermostat_inputs_set_api(api);
	evse_payload_set_api(api);
	app_tx_set_api(api);
	app_rx_set_api(api);
	delay_window_set_api(api);
	diag_request_set_api(api);
	selftest_set_api(api);
	selftest_trigger_set_api(api);
	time_sync_set_api(api);
	charge_now_set_api(api);

	/* Command authentication: call cmd_auth_set_key() with a 32-byte
	 * HMAC key to enable signed downlink verification. When no key is
	 * set, charge control commands are accepted without auth.
	 *
	 * For production, replace the zeros below and uncomment:
	 *   static const uint8_t cmd_auth_key[CMD_AUTH_KEY_SIZE] = { ... };
	 *   cmd_auth_set_key(cmd_auth_key, CMD_AUTH_KEY_SIZE);
	 *
	 * Key must match CMD_AUTH_KEY in the charge scheduler Lambda env.
	 * Generate: python3 -c "import secrets; print(secrets.token_hex(32))"
	 */

	/* Initialize app subsystems */
	evse_sensors_init();
	charge_control_init();
	thermostat_inputs_init();
	time_sync_init();
	delay_window_init();
	event_buffer_init();
	event_filter_init();
	charge_now_init();
	selftest_trigger_set_send_fn(app_tx_send_evse_data);
	selftest_trigger_init();
	led_engine_set_api(api);
	led_engine_init();

	/* Boot self-test (reset first — split-image arch has no C runtime BSS init) */
	selftest_reset();
	selftest_boot_result_t st_result;
	if (selftest_boot(&st_result) != 0) {
		api->log_err("Boot self-test FAILED (flags=0x%02x)",
			     selftest_get_fault_flags());
	}

	/* Request 500ms poll interval from platform */
	api->set_timer_interval(POLL_INTERVAL_MS);

	/* Read initial sensor state */
	uint16_t mv = 0;
	evse_j1772_state_get(&last_j1772_state, &mv);

	uint16_t ma = 0;
	if (evse_current_read(&ma) == 0) {
		last_current_on = (ma >= CURRENT_ON_THRESHOLD_MA);
	}

	last_thermostat_flags = thermostat_flags_get();
	last_heartbeat_ms = api->uptime_ms();

	api->log_inf("App initialized (EVSE monitor v2, poll=%dms)", POLL_INTERVAL_MS);
	return 0;
}

static void app_on_ready(bool ready)
{
	app_tx_set_ready(ready);
}

static void app_on_msg_received(const uint8_t *data, size_t len)
{
	app_rx_process_msg(data, len);
}

static void app_on_msg_sent(uint32_t msg_id)
{
	if (api) {
		api->log_inf("Message %u sent OK", msg_id);
	}
	led_engine_notify_uplink_sent();
}

static void app_on_send_error(uint32_t msg_id, int error)
{
	if (api) {
		api->log_err("Message %u send error: %d", msg_id, error);
	}
}

static void app_on_timer(void)
{
	if (!api) {
		return;
	}

	/* LED engine ticks every 100ms (every call) */
	led_engine_tick();

	/* All other logic runs at the original 500ms rate */
	decimation_counter++;
	if (decimation_counter < SENSOR_DECIMATION) {
		return;
	}
	decimation_counter = 0;

	/* Check auto-resume timer */
	charge_control_tick();

	/* Poll Charge Now button for 5-press self-test trigger */
	selftest_trigger_tick();

	/* --- Poll sensors and detect changes --- */
	bool changed = false;

	/* J1772 pilot state */
	j1772_state_t state;
	uint16_t voltage_mv = 0;
	int adc_ret = evse_j1772_state_get(&state, &voltage_mv);
	led_engine_report_adc_result(adc_ret == 0);
	if (adc_ret == 0) {
		if (state != last_j1772_state) {
			api->log_inf("J1772: %s -> %s (%d mV)",
				     j1772_state_to_string(last_j1772_state),
				     j1772_state_to_string(state), voltage_mv);
			last_j1772_state = state;
			changed = true;
		}
	}

	/* Current clamp (binary on/off) */
	uint16_t current_ma = 0;
	if (evse_current_read(&current_ma) == 0) {
		bool current_on = (current_ma >= CURRENT_ON_THRESHOLD_MA);
		if (current_on != last_current_on) {
			api->log_inf("Current: %s (%d mA)",
				     current_on ? "ON" : "OFF", current_ma);
			last_current_on = current_on;
			changed = true;
		}
	}

	/* Thermostat inputs */
	uint8_t flags = thermostat_flags_get();
	if (flags != last_thermostat_flags) {
		api->log_inf("Thermostat: cool=%d", (flags & 0x02) ? 1 : 0);
		last_thermostat_flags = flags;
		changed = true;
	}

	/* --- Record snapshot in event buffer (only on change or heartbeat) --- */
	{
		struct event_snapshot snap = {
			.timestamp = time_sync_get_epoch(),
			.pilot_voltage_mv = voltage_mv,
			.current_ma = current_ma,
			.j1772_state = (uint8_t)last_j1772_state,
			.thermostat_flags = last_thermostat_flags,
			.charge_flags = charge_control_is_allowed()
					? EVENT_FLAG_CHARGE_ALLOWED : 0,
			.transition_reason = charge_control_get_last_reason(),
		};
		event_filter_submit(&snap, api->uptime_ms());
		charge_control_clear_last_reason();
	}

	/* --- Charge Now latch expiry/cancel check --- */
	charge_now_tick((uint8_t)last_j1772_state);

	/* --- Continuous self-test monitoring --- */
	selftest_continuous_tick((uint8_t)state, voltage_mv, current_ma,
				charge_control_is_allowed(), flags);

	/* --- Send on change or heartbeat --- */
	uint32_t now = api->uptime_ms();
	bool heartbeat_due = !last_heartbeat_ms ||
			     (now - last_heartbeat_ms) >= HEARTBEAT_INTERVAL_MS;

	if (changed || heartbeat_due) {
		app_tx_send_evse_data();
		if (heartbeat_due) {
			last_heartbeat_ms = now;
		}
	}
}

static int app_on_shell_cmd(const char *cmd, const char *args,
			    void (*print)(const char *fmt, ...),
			    void (*error)(const char *fmt, ...))
{
	/* evse commands */
	if (strcmp(cmd, "evse") == 0) {
		if (!args || strcmp(args, "status") == 0) {
			return shell_evse_status(print, error);
		} else if (strcmp(args, "a") == 0) {
			evse_sensors_simulate_state(J1772_STATE_A, SIMULATION_DURATION_MS);
			print("Simulating J1772 State A (no vehicle) for 10 seconds");
			app_tx_send_evse_data();
			return 0;
		} else if (strcmp(args, "b") == 0) {
			evse_sensors_simulate_state(J1772_STATE_B, SIMULATION_DURATION_MS);
			print("Simulating J1772 State B (vehicle connected) for 10 seconds");
			app_tx_send_evse_data();
			return 0;
		} else if (strcmp(args, "c") == 0) {
			evse_sensors_simulate_state(J1772_STATE_C, SIMULATION_DURATION_MS);
			print("Simulating J1772 State C (charging) for 10 seconds");
			app_tx_send_evse_data();
			return 0;
		} else if (strcmp(args, "allow") == 0) {
			charge_control_set_with_reason(true, 0, TRANSITION_REASON_MANUAL);
			print("Charging ALLOWED (GPIO high)");
			return 0;
		} else if (strcmp(args, "pause") == 0) {
			charge_control_set_with_reason(false, 0, TRANSITION_REASON_MANUAL);
			print("Charging PAUSED (GPIO low)");
			return 0;
		} else if (strcmp(args, "buffer") == 0) {
			uint8_t cnt = event_buffer_count();
			print("Event buffer: %d/%d entries", cnt, EVENT_BUFFER_CAPACITY);
			if (cnt > 0) {
				print("  Oldest: %u", event_buffer_oldest_timestamp());
				print("  Newest: %u", event_buffer_newest_timestamp());
			}
			return 0;
		}
		error("Unknown evse subcommand: %s", args);
		return -1;
	}

	/* hvac commands */
	if (strcmp(cmd, "hvac") == 0) {
		if (!args || strcmp(args, "status") == 0 || strcmp(args, "call") == 0) {
			return shell_hvac_status(print, error);
		}
		error("Unknown hvac subcommand: %s", args);
		return -1;
	}

	/* selftest (commissioning) */
	if (strcmp(cmd, "selftest") == 0) {
		return selftest_run_shell(print, error);
	}

	/* sid commands */
	if (strcmp(cmd, "sid") == 0 && args && strcmp(args, "time") == 0) {
		if (!time_sync_is_synced()) {
			print("Time: NOT SYNCED (no TIME_SYNC received)");
			return 0;
		}
		uint32_t epoch = time_sync_get_epoch();
		uint32_t wm = time_sync_get_ack_watermark();
		uint32_t since = time_sync_ms_since_sync();
		print("Time sync status:");
		print("  SideCharge epoch: %u", epoch);
		print("  ACK watermark: %u", wm);
		print("  Since last sync: %u ms", since);
		return 0;
	}

	/* sid send (manual trigger) */
	if (strcmp(cmd, "sid") == 0 && args && strcmp(args, "send") == 0) {
		int err = app_tx_send_evse_data();
		if (err) {
			error("Send failed: %d", err);
			return err;
		}
		print("Send queued successfully");
		return 0;
	}

	error("Unknown app command: %s %s", cmd, args ? args : "");
	return -1;
}

/* ------------------------------------------------------------------ */
/*  App callback table — placed at start of app partition              */
/* ------------------------------------------------------------------ */

#ifdef HOST_TEST
const struct app_callbacks app_cb = {
#else
__attribute__((section(".app_header"), used))
const struct app_callbacks app_cb = {
#endif
	.magic           = APP_CALLBACK_MAGIC,
	.version         = APP_CALLBACK_VERSION,
	.init            = app_init,
	.on_ready        = app_on_ready,
	.on_msg_received = app_on_msg_received,
	.on_msg_sent     = app_on_msg_sent,
	.on_send_error   = app_on_send_error,
	.on_timer        = app_on_timer,
	.on_shell_cmd    = app_on_shell_cmd,
};
