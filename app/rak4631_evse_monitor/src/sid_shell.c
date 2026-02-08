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

	shell_print(sh, "Sidewalk Status:");
	shell_print(sh, "  Ready: %s", ready ? "YES" : "NO");
	shell_print(sh, "  Link type: %s (0x%x)", link_type_str(link_mask), link_mask);

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
	SHELL_CMD(status, NULL, "Show Sidewalk status", cmd_sid_status),
	SHELL_CMD(send, NULL, "Trigger manual send", cmd_sid_send),
	SHELL_CMD(lora, NULL, "Switch to LoRa mode", cmd_sid_lora),
	SHELL_CMD(ble, NULL, "Switch to BLE mode", cmd_sid_ble),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sid, &sid_cmds, "Sidewalk commands", NULL);
