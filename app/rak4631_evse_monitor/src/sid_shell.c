/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Sidewalk Shell Commands for debugging and testing
 */

#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <app_tx.h>
#include <sidewalk.h>

LOG_MODULE_REGISTER(sid_shell, CONFIG_SIDEWALK_LOG_LEVEL);

static const char *link_type_str(uint32_t link_mask)
{
	if (link_mask & SID_LINK_TYPE_1) return "BLE";
	if (link_mask & SID_LINK_TYPE_2) return "FSK";
	if (link_mask & SID_LINK_TYPE_3) return "LoRa";
	return "None";
}

static int cmd_sid_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	bool ready = app_tx_is_ready();
	uint32_t link_mask = app_tx_get_link_mask();
	sid_init_status_t init = sidewalk_get_init_status();

	shell_print(sh, "Sidewalk Status:");
	shell_print(sh, "  Init state: %s (err=%d)",
		    sidewalk_init_state_str(init.state), init.err_code);
	shell_print(sh, "  Ready: %s", ready ? "YES" : "NO");
	shell_print(sh, "  Link type: %s (0x%x)", link_type_str(link_mask), link_mask);

	/* Explain what the init state means */
	switch (init.state) {
	case SID_INIT_NOT_STARTED:
		shell_warn(sh, "  -> Init never ran. app_start() may have failed early.");
		break;
	case SID_INIT_PLATFORM_INIT_ERR:
		shell_error(sh, "  -> sid_platform_init() failed. Check radio/SPI config.");
		break;
	case SID_INIT_MFG_EMPTY:
		shell_error(sh, "  -> MFG store is empty! Flash mfg.hex with device credentials.");
		shell_error(sh, "     Use: nrfjprog --program mfg.hex --sectorerase");
		break;
	case SID_INIT_RADIO_INIT_ERR:
		shell_error(sh, "  -> Radio init failed. Check SX1262 SPI/GPIO wiring.");
		break;
	case SID_INIT_SID_INIT_ERR:
		shell_error(sh, "  -> sid_init() failed. Config or memory issue.");
		break;
	case SID_INIT_SID_START_ERR:
		shell_error(sh, "  -> sid_start() failed. Link mask or state issue.");
		break;
	case SID_INIT_STARTED_OK:
		if (!ready) {
			shell_warn(sh, "  -> Sidewalk started but not READY. Waiting for gateway.");
			shell_warn(sh, "     Ensure a Sidewalk gateway (Ring/Echo) is in range.");
		} else {
			shell_print(sh, "  -> Sidewalk running and connected.");
		}
		break;
	}

	return 0;
}

static int cmd_sid_reinit(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Re-running Sidewalk init sequence...");
	shell_print(sh, "(Watch log output for errors)");

	/* Queue platform init and autostart on the sidewalk thread */
	int err = sidewalk_event_send(sidewalk_event_platform_init, NULL, NULL);
	if (err) {
		shell_error(sh, "Failed to queue platform_init: %d", err);
		return err;
	}

	err = sidewalk_event_send(sidewalk_event_autostart, NULL, NULL);
	if (err) {
		shell_error(sh, "Failed to queue autostart: %d", err);
		return err;
	}

	shell_print(sh, "Init events queued. Run 'sid status' in a few seconds to check result.");
	return 0;
}

static int cmd_sid_send(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Triggering manual sensor read and transmit...");

	int err = app_tx_send_evse_data();
	if (err) {
		shell_error(sh, "Send failed: %d", err);
		return err;
	}

	shell_print(sh, "Send queued successfully");
	return 0;
}

static int cmd_sid_lora(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint32_t mask = SID_LINK_TYPE_3;

	app_tx_set_link_mask(mask);
	sidewalk_event_send(sidewalk_event_set_link,
			    (void *)(uintptr_t)mask, NULL);

	shell_print(sh, "Switching Sidewalk stack to LoRa (reinit)...");
	return 0;
}

static int cmd_sid_ble(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint32_t mask = SID_LINK_TYPE_1;

	app_tx_set_link_mask(mask);
	sidewalk_event_send(sidewalk_event_set_link,
			    (void *)(uintptr_t)mask, NULL);

	shell_print(sh, "Switching Sidewalk stack to BLE (reinit)...");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sid_cmds,
	SHELL_CMD(status, NULL, "Show Sidewalk init state and status", cmd_sid_status),
	SHELL_CMD(reinit, NULL, "Re-run Sidewalk init (with visible logs)", cmd_sid_reinit),
	SHELL_CMD(send, NULL, "Trigger manual send", cmd_sid_send),
	SHELL_CMD(lora, NULL, "Switch to LoRa mode", cmd_sid_lora),
	SHELL_CMD(ble, NULL, "Switch to BLE mode", cmd_sid_ble),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sid, &sid_cmds, "Sidewalk commands", NULL);
