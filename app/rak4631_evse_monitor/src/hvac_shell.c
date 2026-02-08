/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * HVAC Simulation Shell Commands for testing
 */

#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <thermostat_inputs.h>

LOG_MODULE_REGISTER(hvac_shell, CONFIG_SIDEWALK_LOG_LEVEL);

static int cmd_hvac_call(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	/* Read and display current thermostat state */
	uint8_t flags = thermostat_flags_get();
	bool heat = (flags & THERMOSTAT_FLAG_HEAT) != 0;
	bool cool = (flags & THERMOSTAT_FLAG_COOL) != 0;

	shell_print(sh, "HVAC Thermostat Status:");
	shell_print(sh, "  Heat call: %s", heat ? "ACTIVE" : "inactive");
	shell_print(sh, "  Cool call: %s", cool ? "ACTIVE" : "inactive");
	shell_print(sh, "  Flags: 0x%02x", flags);

	return 0;
}

static int cmd_hvac_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint8_t flags = thermostat_flags_get();

	shell_print(sh, "Thermostat flags: 0x%02x", flags);
	shell_print(sh, "  Heat: %s", (flags & THERMOSTAT_FLAG_HEAT) ? "ON" : "OFF");
	shell_print(sh, "  Cool: %s", (flags & THERMOSTAT_FLAG_COOL) ? "ON" : "OFF");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(hvac_cmds,
	SHELL_CMD(call, NULL, "Show thermostat call status", cmd_hvac_call),
	SHELL_CMD(status, NULL, "Show thermostat status", cmd_hvac_status),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(hvac, &hvac_cmds, "HVAC thermostat commands", NULL);
