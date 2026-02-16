/*
 * Platform Shell Commands
 *
 * All platform-level shell commands: sid status, mfg, ota, radio
 * switching, factory reset, and app command dispatch.
 *
 * Consolidates the old app.c shell code and sid_shell.c into one file.
 */

#include <app.h>
#include <platform_api.h>
#include <tx_state.h>
#include <sidewalk.h>
#include <ota_update.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

LOG_MODULE_REGISTER(platform_shell, CONFIG_SIDEWALK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/*  External: platform API table (defined in platform_api_impl.c)      */
/* ------------------------------------------------------------------ */

extern const struct platform_api platform_api_table;

/* ------------------------------------------------------------------ */
/*  Shell dispatch to app                                              */
/* ------------------------------------------------------------------ */

static const struct shell *current_shell;

static void shell_print_wrapper(const char *fmt, ...)
{
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (current_shell) {
		shell_print(current_shell, "%s", buf);
	}
}

static void shell_error_wrapper(const char *fmt, ...)
{
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (current_shell) {
		shell_error(current_shell, "%s", buf);
	}
}

static int cmd_app_dispatch(const struct shell *sh, const char *cmd,
			    size_t argc, char **argv)
{
	const struct app_callbacks *cb = app_get_callbacks();
	if (!app_image_valid() || !cb->on_shell_cmd) {
		shell_error(sh, "No app image loaded");
		return -1;
	}

	/* Build args string from remaining argv */
	char args_buf[128] = {0};
	size_t pos = 0;
	for (size_t i = 1; i < argc; i++) {
		if (i > 1 && pos < sizeof(args_buf) - 1) {
			args_buf[pos++] = ' ';
		}
		size_t len = strlen(argv[i]);
		if (pos + len >= sizeof(args_buf)) {
			break;
		}
		memcpy(args_buf + pos, argv[i], len);
		pos += len;
	}
	args_buf[pos] = '\0';

	current_shell = sh;
	int ret = cb->on_shell_cmd(cmd, pos > 0 ? args_buf : NULL,
				   shell_print_wrapper, shell_error_wrapper);
	current_shell = NULL;
	return ret;
}

/* ------------------------------------------------------------------ */
/*  Platform shell commands                                            */
/* ------------------------------------------------------------------ */

static const char *link_type_str(uint32_t link_mask)
{
	if (link_mask & SID_LINK_TYPE_1) return "BLE";
	if (link_mask & SID_LINK_TYPE_2) return "FSK";
	if (link_mask & SID_LINK_TYPE_3) return "LoRa";
	return "None";
}

static int cmd_sid_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);

	bool ready = tx_state_is_ready();
	uint32_t link_mask = tx_state_get_link_mask();
	sid_init_status_t init = sidewalk_get_init_status();

	shell_print(sh, "Sidewalk Status:");
	shell_print(sh, "  Init state: %s (err=%d)",
		    sidewalk_init_state_str(init.state), init.err_code);
	shell_print(sh, "  Ready: %s", ready ? "YES" : "NO");
	shell_print(sh, "  Link type: %s (0x%x)", link_type_str(link_mask), link_mask);
	if (app_image_valid()) {
		shell_print(sh, "  App image: LOADED");
	} else {
		const char *reason = app_get_reject_reason();
		if (reason) {
			shell_error(sh, "  App image: NOT LOADED (%s)", reason);
		} else {
			shell_print(sh, "  App image: NOT FOUND");
		}
	}

	switch (init.state) {
	case SID_INIT_NOT_STARTED:
		shell_warn(sh, "  -> Init never ran.");
		break;
	case SID_INIT_PLATFORM_INIT_ERR:
		shell_error(sh, "  -> sid_platform_init() failed (err=%d).", init.err_code);
		break;
	case SID_INIT_MFG_EMPTY:
		shell_error(sh, "  -> MFG store is empty! Flash mfg.hex.");
		break;
	case SID_INIT_RADIO_INIT_ERR:
		shell_error(sh, "  -> Radio init failed (err=%d).", init.err_code);
		break;
	case SID_INIT_SID_INIT_ERR:
		shell_error(sh, "  -> sid_init() failed (err=%d).", init.err_code);
		break;
	case SID_INIT_SID_START_ERR:
		shell_error(sh, "  -> sid_start() failed (err=%d).", init.err_code);
		break;
	case SID_INIT_STARTED_OK:
		if (!ready) {
			shell_warn(sh, "  -> Started but not READY. Waiting for gateway.");
		} else {
			shell_print(sh, "  -> Running and connected.");
		}
		break;
	}
	return 0;
}

static int cmd_sid_mfg(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);

	uint32_t ver = platform_api_table.mfg_get_version();
	shell_print(sh, "MFG Store:");
	shell_print(sh, "  Version: %u", ver);
	if (ver == 0 || ver == 0xFFFFFFFF) {
		shell_error(sh, "  -> MFG partition is EMPTY or ERASED!");
		return -1;
	}

	uint8_t dev_id[5] = {0};
	bool ok = platform_api_table.mfg_get_dev_id(dev_id);
	shell_print(sh, "  Device ID: %s %02x:%02x:%02x:%02x:%02x",
		    ok ? "" : "(FAIL)",
		    dev_id[0], dev_id[1], dev_id[2], dev_id[3], dev_id[4]);

	return 0;
}

static int cmd_sid_reinit(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);

	shell_print(sh, "Re-running Sidewalk init sequence...");
	sidewalk_event_send(sidewalk_event_platform_init, NULL, NULL);
	sidewalk_event_send(sidewalk_event_autostart, NULL, NULL);
	shell_print(sh, "Init events queued. Run 'sid status' to check.");
	return 0;
}

static int cmd_sid_send_app(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_app_dispatch(sh, "sid", 2, (char *[]){"sid", "send"});
}

static int cmd_sid_lora(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	uint32_t mask = SID_LINK_TYPE_3;
	tx_state_set_link_mask(mask);
	sidewalk_event_send(sidewalk_event_set_link, (void *)(uintptr_t)mask, NULL);
	shell_print(sh, "Switching to LoRa...");
	return 0;
}

static int cmd_sid_ble(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	uint32_t mask = SID_LINK_TYPE_1;
	tx_state_set_link_mask(mask);
	sidewalk_event_send(sidewalk_event_set_link, (void *)(uintptr_t)mask, NULL);
	shell_print(sh, "Switching to BLE...");
	return 0;
}

static int cmd_sid_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	shell_warn(sh, "Factory reset — clears session keys and registration.");
	sidewalk_event_send(sidewalk_event_factory_reset, NULL, NULL);
	shell_print(sh, "Factory reset queued. Device will reboot.");
	return 0;
}

/* ------------------------------------------------------------------ */
/*  OTA shell commands                                                 */
/* ------------------------------------------------------------------ */

static int cmd_sid_ota_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	enum ota_phase phase = ota_get_phase();
	shell_print(sh, "OTA Status: %s", ota_phase_str(phase));
	return 0;
}

static int cmd_sid_ota_abort(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	if (ota_get_phase() == OTA_PHASE_IDLE) {
		shell_print(sh, "OTA: no session active");
	} else {
		ota_abort();
		shell_print(sh, "OTA: session aborted");
	}
	return 0;
}

static int cmd_sid_ota_send_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	ota_send_status();
	shell_print(sh, "OTA: status uplink queued");
	return 0;
}

static int cmd_sid_ota_delta_test(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 5) {
		shell_error(sh, "Usage: sid ota delta_test <chunk_sz> <n_delta> "
			    "<new_size> <new_crc> [idx1 idx2 ...]");
		return -EINVAL;
	}

	uint16_t chunk_size = (uint16_t)strtoul(argv[1], NULL, 0);
	uint16_t delta_count = (uint16_t)strtoul(argv[2], NULL, 0);
	uint32_t new_size = strtoul(argv[3], NULL, 0);
	uint32_t new_crc = strtoul(argv[4], NULL, 0);

	ota_test_delta_setup(chunk_size, delta_count, new_size, new_crc);

	for (int i = 5; i < argc; i++) {
		uint16_t idx = (uint16_t)strtoul(argv[i], NULL, 0);
		ota_test_delta_mark_chunk(idx);
		shell_print(sh, "  marked chunk %u", idx);
	}

	shell_print(sh, "Delta: %u chunks marked, validating+applying...", delta_count);
	ota_test_delta(new_size, new_crc, 99);
	/* If we get here, apply failed (success reboots) */
	shell_error(sh, "Delta apply failed!");
	return -EIO;
}

/* ------------------------------------------------------------------ */
/*  App shell commands                                                 */
/* ------------------------------------------------------------------ */

static int cmd_app(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: app <command> [args...]");
		return -1;
	}
	return cmd_app_dispatch(sh, argv[1], argc - 1, argv + 1);
}

static int cmd_sid_selftest(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	return cmd_app_dispatch(sh, "selftest", 1, (char *[]){"selftest"});
}

/* ------------------------------------------------------------------ */
/*  Shell registration                                                 */
/* ------------------------------------------------------------------ */

SHELL_STATIC_SUBCMD_SET_CREATE(ota_cmds,
	SHELL_CMD(status, NULL, "Show OTA status", cmd_sid_ota_status),
	SHELL_CMD(abort, NULL, "Abort OTA session", cmd_sid_ota_abort),
	SHELL_CMD(report, NULL, "Send OTA status uplink", cmd_sid_ota_send_status),
	SHELL_CMD(delta_test, NULL, "Test delta OTA from flash", cmd_sid_ota_delta_test),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sid_cmds,
	SHELL_CMD(status, NULL, "Show Sidewalk status", cmd_sid_status),
	SHELL_CMD(mfg, NULL, "Check MFG store", cmd_sid_mfg),
	SHELL_CMD(reinit, NULL, "Re-run Sidewalk init", cmd_sid_reinit),
	SHELL_CMD(send, NULL, "Trigger manual send (app)", cmd_sid_send_app),
	SHELL_CMD(selftest, NULL, "Run commissioning self-test", cmd_sid_selftest),
	SHELL_CMD(lora, NULL, "Switch to LoRa", cmd_sid_lora),
	SHELL_CMD(ble, NULL, "Switch to BLE", cmd_sid_ble),
	SHELL_CMD(reset, NULL, "Factory reset", cmd_sid_reset),
	SHELL_CMD(ota, &ota_cmds, "OTA update commands", NULL),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(sid, &sid_cmds, "Sidewalk commands", NULL);

SHELL_CMD_REGISTER(app, NULL, "App commands", cmd_app);
