/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * EVSE Simulation Shell Commands for testing
 */

#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <evse_sensors.h>
#include <charge_control.h>

LOG_MODULE_REGISTER(evse_shell, CONFIG_SIDEWALK_LOG_LEVEL);

#define SIMULATION_DURATION_MS (10000)  /* 10 seconds */

static int cmd_evse_state_a(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	evse_sensors_simulate_state(J1772_STATE_A, SIMULATION_DURATION_MS);
	shell_print(sh, "Simulating J1772 State A (no vehicle) for 10 seconds");

	return 0;
}

static int cmd_evse_state_b(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	evse_sensors_simulate_state(J1772_STATE_B, SIMULATION_DURATION_MS);
	shell_print(sh, "Simulating J1772 State B (vehicle connected) for 10 seconds");

	return 0;
}

static int cmd_evse_state_c(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	evse_sensors_simulate_state(J1772_STATE_C, SIMULATION_DURATION_MS);
	shell_print(sh, "Simulating J1772 State C (charging) for 10 seconds");

	return 0;
}

static int cmd_evse_allow(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	charge_control_set(true, 0);
	shell_print(sh, "Charging ALLOWED (GPIO high)");

	return 0;
}

static int cmd_evse_pause(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	charge_control_set(false, 0);
	shell_print(sh, "Charging PAUSED (GPIO low)");

	return 0;
}

static int cmd_evse_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	j1772_state_t state;
	uint16_t voltage_mv;
	uint16_t current_ma;

	int err = evse_j1772_state_get(&state, &voltage_mv);
	if (err) {
		shell_error(sh, "Failed to read J1772 state: %d", err);
	}

	err = evse_current_read(&current_ma);
	if (err) {
		shell_error(sh, "Failed to read current: %d", err);
	}

	charge_control_state_t cc_state;
	charge_control_get_state(&cc_state);

	shell_print(sh, "EVSE Status:");
	shell_print(sh, "  J1772 state: %s", j1772_state_to_string(state));
	shell_print(sh, "  Pilot voltage: %d mV", voltage_mv);
	shell_print(sh, "  Current: %d mA", current_ma);
	shell_print(sh, "  Charging allowed: %s", cc_state.charging_allowed ? "YES" : "NO");
	shell_print(sh, "  Simulation active: %s", evse_sensors_is_simulating() ? "YES" : "NO");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(evse_cmds,
	SHELL_CMD(a, NULL, "Simulate state A for 10s", cmd_evse_state_a),
	SHELL_CMD(b, NULL, "Simulate state B for 10s", cmd_evse_state_b),
	SHELL_CMD(c, NULL, "Simulate state C for 10s", cmd_evse_state_c),
	SHELL_CMD(allow, NULL, "Allow charging", cmd_evse_allow),
	SHELL_CMD(pause, NULL, "Pause charging", cmd_evse_pause),
	SHELL_CMD(status, NULL, "Show EVSE status", cmd_evse_status),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(evse, &evse_cmds, "EVSE simulation commands", NULL);
