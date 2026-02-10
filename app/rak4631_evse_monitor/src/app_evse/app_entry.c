/*
 * App Entry Point — callback table and lifecycle
 *
 * This file is the single entry point from the platform into the app.
 * It contains the app_callbacks struct at a fixed address (.app_header)
 * and dispatches platform callbacks to the app modules.
 */

#include <platform_api.h>
#include <evse_sensors.h>
#include <charge_control.h>
#include <thermostat_inputs.h>
#include <rak_sidewalk.h>
#include <app_tx.h>
#include <app_rx.h>
#include <string.h>

static const struct platform_api *api;

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
	print("  Simulation active: %s", evse_sensors_is_simulating() ? "YES" : "NO");
	return 0;
}

static int shell_hvac_status(void (*print)(const char *, ...), void (*error)(const char *, ...))
{
	(void)error;
	uint8_t flags = thermostat_flags_get();
	print("Thermostat flags: 0x%02x", flags);
	print("  Heat: %s", (flags & 0x01) ? "ON" : "OFF");
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
	rak_sidewalk_set_api(api);
	app_tx_set_api(api);
	app_rx_set_api(api);

	/* Initialize app subsystems */
	evse_sensors_init();
	charge_control_init();
	thermostat_inputs_init();

	api->log_inf("App initialized (EVSE monitor v1)");
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
}

static void app_on_send_error(uint32_t msg_id, int error)
{
	if (api) {
		api->log_err("Message %u send error: %d", msg_id, error);
	}
}

static void app_on_timer(void)
{
	/* Periodic tick — check auto-resume, read sensors, transmit */
	charge_control_tick();
	app_tx_send_evse_data();
}

static void app_on_sensor_change(uint8_t source)
{
	if (api) {
		api->log_inf("Sensor change: 0x%02x", source);
	}
	app_tx_send_evse_data();
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
			charge_control_set(true, 0);
			print("Charging ALLOWED (GPIO high)");
			return 0;
		} else if (strcmp(args, "pause") == 0) {
			charge_control_set(false, 0);
			print("Charging PAUSED (GPIO low)");
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

__attribute__((section(".app_header"), used))
const struct app_callbacks app_cb = {
	.magic           = APP_CALLBACK_MAGIC,
	.version         = APP_CALLBACK_VERSION,
	.init            = app_init,
	.on_ready        = app_on_ready,
	.on_msg_received = app_on_msg_received,
	.on_msg_sent     = app_on_msg_sent,
	.on_send_error   = app_on_send_error,
	.on_timer        = app_on_timer,
	.on_shell_cmd    = app_on_shell_cmd,
	.on_sensor_change = app_on_sensor_change,
};
