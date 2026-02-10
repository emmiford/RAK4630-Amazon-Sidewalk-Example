/*
 * Platform-side App Loader
 *
 * Boots Sidewalk, discovers the app callback table at 0x80000,
 * and dispatches events through it.
 */

#include <sidewalk.h>
#include <app_tx.h>
#include <app_rx.h>
#include <app_leds.h>
#include <app_ble_config.h>
#include <sidewalk_dfu/nordic_dfu.h>
#include <app_subGHz_config.h>
#include <sid_hal_reset_ifc.h>
#include <sid_hal_memory_ifc.h>
#include <platform_api.h>
#include <ota_update.h>
#include "sensor_monitor.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <json_printer/sidTypes2str.h>
#ifdef CONFIG_SIDEWALK_FILE_TRANSFER_DFU
#include <sbdt/dfu_file_transfer.h>
#endif
#include <bt_app_callbacks.h>

LOG_MODULE_REGISTER(app, CONFIG_SIDEWALK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/*  External: platform API table (defined in platform_api_impl.c)     */
/* ------------------------------------------------------------------ */

extern const struct platform_api platform_api_table;

/* ------------------------------------------------------------------ */
/*  App callback table discovery                                       */
/* ------------------------------------------------------------------ */

static const struct app_callbacks *app_cb;

static bool app_image_valid(void)
{
	return (app_cb != NULL);
}

static void discover_app_image(void)
{
	const struct app_callbacks *cb =
		(const struct app_callbacks *)APP_CALLBACKS_ADDR;

	if (cb->magic != APP_CALLBACK_MAGIC) {
		LOG_WRN("No valid app image at 0x%08x (magic=0x%08x, expected=0x%08x)",
			APP_CALLBACKS_ADDR, cb->magic, APP_CALLBACK_MAGIC);
		app_cb = NULL;
		return;
	}

	if (cb->version != APP_CALLBACK_VERSION) {
		LOG_WRN("App API version mismatch: %u vs %u",
			cb->version, APP_CALLBACK_VERSION);
	}

	app_cb = cb;
	LOG_INF("App image found at 0x%08x (version %u)", APP_CALLBACKS_ADDR, cb->version);
}

/* ------------------------------------------------------------------ */
/*  Timer — periodic sensor/TX tick                                    */
/* ------------------------------------------------------------------ */

#define NOTIFY_TIMER_DURATION_MS (10000)

static void notify_timer_cb(struct k_timer *timer_id);
K_TIMER_DEFINE(notify_timer, notify_timer_cb, NULL);

static void timer_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	if (app_image_valid() && app_cb->on_timer) {
		app_cb->on_timer();
	}
}
K_WORK_DEFINE(timer_work, timer_work_handler);

static void notify_timer_cb(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	k_work_submit(&timer_work);
}

/* ------------------------------------------------------------------ */
/*  Sidewalk callbacks                                                 */
/* ------------------------------------------------------------------ */

static sidewalk_ctx_t sid_ctx;

static void on_sidewalk_event(bool in_isr, void *context)
{
	int err = sidewalk_event_send(sidewalk_event_process, NULL, NULL);
	if (err) {
		LOG_ERR("Send event err %d", err);
	}
}

static void on_sidewalk_msg_received(const struct sid_msg_desc *msg_desc,
				     const struct sid_msg *msg, void *context)
{
	LOG_DBG("Received message(type: %d, link_mode: %d, id: %u size %u)",
		(int)msg_desc->type, (int)msg_desc->link_mode, msg_desc->id, msg->size);
	LOG_HEXDUMP_INF((uint8_t *)msg->data, msg->size, "Received message: ");

	if (msg_desc->type == SID_MSG_TYPE_RESPONSE &&
	    msg_desc->msg_desc_attr.rx_attr.is_msg_ack) {
		LOG_DBG("Received Ack for msg id %d", msg_desc->id);
	} else if (msg->size >= 1 && ((const uint8_t *)msg->data)[0] == OTA_CMD_TYPE) {
		/* OTA messages handled by platform, not forwarded to app */
		ota_process_msg((const uint8_t *)msg->data, msg->size);
	} else if (app_image_valid() && app_cb->on_msg_received) {
		app_cb->on_msg_received((const uint8_t *)msg->data, msg->size);
	}
}

static void on_sidewalk_msg_sent(const struct sid_msg_desc *msg_desc, void *context)
{
	LOG_DBG("sent message(type: %d, id: %u)", (int)msg_desc->type, msg_desc->id);
	if (app_image_valid() && app_cb->on_msg_sent) {
		app_cb->on_msg_sent(msg_desc->id);
	}
}

static void on_sidewalk_send_error(sid_error_t error, const struct sid_msg_desc *msg_desc,
				   void *context)
{
	LOG_ERR("Send message err %d (%s)", (int)error, SID_ERROR_T_STR(error));
	if (app_image_valid() && app_cb->on_send_error) {
		app_cb->on_send_error(msg_desc->id, (int)error);
	}
}

static void on_sidewalk_factory_reset(void *context)
{
	ARG_UNUSED(context);
	LOG_INF("Factory reset notification received from sid api");
	if (sid_hal_reset(SID_HAL_RESET_NORMAL)) {
		LOG_WRN("Cannot reboot");
	}
}

static void on_sidewalk_status_changed(const struct sid_status *status, void *context)
{
	struct sid_status *new_status = sid_hal_malloc(sizeof(struct sid_status));
	if (!new_status) {
		LOG_ERR("Failed to allocate memory for new status value");
	} else {
		memcpy(new_status, status, sizeof(struct sid_status));
	}
	sidewalk_event_send(sidewalk_event_new_status, new_status, sid_hal_free);

	/* Update platform TX module */
	app_tx_set_link_mask(status->detail.link_status_mask);

	/* Determine ready state */
	bool ready = false;
	switch (status->state) {
	case SID_STATE_READY:
	case SID_STATE_SECURE_CHANNEL_READY:
		ready = true;
		break;
	default:
		break;
	}

	app_tx_set_ready(ready);

	/* Notify app */
	if (app_image_valid() && app_cb->on_ready) {
		app_cb->on_ready(ready);
	}

	LOG_INF("Device %sregistered, Time Sync %s, Link status: {BLE: %s, FSK: %s, LoRa: %s}",
		(SID_STATUS_REGISTERED == status->detail.registration_status) ? "Is " : "Un",
		(SID_STATUS_TIME_SYNCED == status->detail.time_sync_status) ? "Success" : "Fail",
		(status->detail.link_status_mask & SID_LINK_TYPE_1) ? "Up" : "Down",
		(status->detail.link_status_mask & SID_LINK_TYPE_2) ? "Up" : "Down",
		(status->detail.link_status_mask & SID_LINK_TYPE_3) ? "Up" : "Down");

	for (int i = 0; i < SID_LINK_TYPE_MAX_IDX; i++) {
		enum sid_link_mode mode =
			(enum sid_link_mode)status->detail.supported_link_modes[i];
		if (mode) {
			LOG_INF("Link mode on %s = {Cloud: %s, Mobile: %s}",
				(SID_LINK_TYPE_1_IDX == i) ? "BLE" :
				(SID_LINK_TYPE_2_IDX == i) ? "FSK" :
				(SID_LINK_TYPE_3_IDX == i) ? "LoRa" : "unknow",
				(mode & SID_LINK_MODE_CLOUD) ? "True" : "False",
				(mode & SID_LINK_MODE_MOBILE) ? "True" : "False");
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Shell dispatch to app                                              */
/* ------------------------------------------------------------------ */

/* Wrappers to pass Zephyr shell_print/shell_error as function pointers.
 * The Zephyr shell API takes a `const struct shell *` first arg, so we
 * capture it in a file-scope variable during command dispatch. */

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
	if (!app_image_valid() || !app_cb->on_shell_cmd) {
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
	int ret = app_cb->on_shell_cmd(cmd, pos > 0 ? args_buf : NULL,
				       shell_print_wrapper, shell_error_wrapper);
	current_shell = NULL;
	return ret;
}

static int cmd_evse(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_app_dispatch(sh, "evse", argc, argv);
}

static int cmd_hvac(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_app_dispatch(sh, "hvac", argc, argv);
}

/* "sid send" is handled by app, other sid commands stay in platform */
static int cmd_sid_send_app(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_app_dispatch(sh, "sid", 2, (char *[]){"sid", "send"});
}

/* ------------------------------------------------------------------ */
/*  BLE GATT authorization                                             */
/* ------------------------------------------------------------------ */

static bool gatt_authorize(struct bt_conn *conn, const struct bt_gatt_attr *attr)
{
	struct bt_conn_info cinfo = {};
	int ret = bt_conn_get_info(conn, &cinfo);
	if (ret != 0) {
		LOG_ERR("Failed to get id of connection err %d", ret);
		return false;
	}

	if (cinfo.id == BT_ID_SIDEWALK) {
		if (sid_ble_bt_attr_is_SMP(attr)) {
			return false;
		}
	}

#if defined(CONFIG_SIDEWALK_DFU)
	if (cinfo.id == BT_ID_SMP_DFU) {
		if (sid_ble_bt_attr_is_SIDEWALK(attr)) {
			return false;
		}
	}
#endif
	return true;
}

static const struct bt_gatt_authorization_cb gatt_authorization_callbacks = {
	.read_authorize = gatt_authorize,
	.write_authorize = gatt_authorize,
};

/* ------------------------------------------------------------------ */
/*  Platform-side shell commands (sid status, mfg, etc.)               */
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

	bool ready = app_tx_is_ready();
	uint32_t link_mask = app_tx_get_link_mask();
	sid_init_status_t init = sidewalk_get_init_status();

	shell_print(sh, "Sidewalk Status:");
	shell_print(sh, "  Init state: %s (err=%d)",
		    sidewalk_init_state_str(init.state), init.err_code);
	shell_print(sh, "  Ready: %s", ready ? "YES" : "NO");
	shell_print(sh, "  Link type: %s (0x%x)", link_type_str(link_mask), link_mask);
	shell_print(sh, "  App image: %s", app_image_valid() ? "LOADED" : "NOT FOUND");

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

static int cmd_sid_lora(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	uint32_t mask = SID_LINK_TYPE_3;
	app_tx_set_link_mask(mask);
	sidewalk_event_send(sidewalk_event_set_link, (void *)(uintptr_t)mask, NULL);
	shell_print(sh, "Switching to LoRa...");
	return 0;
}

static int cmd_sid_ble(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc); ARG_UNUSED(argv);
	uint32_t mask = SID_LINK_TYPE_1;
	app_tx_set_link_mask(mask);
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

/* OTA subcommands */
SHELL_STATIC_SUBCMD_SET_CREATE(ota_cmds,
	SHELL_CMD(status, NULL, "Show OTA status", cmd_sid_ota_status),
	SHELL_CMD(abort, NULL, "Abort OTA session", cmd_sid_ota_abort),
	SHELL_CMD(report, NULL, "Send OTA status uplink", cmd_sid_ota_send_status),
	SHELL_SUBCMD_SET_END
);

static int cmd_sid_test_change(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: sid test_change <source_bitmask>");
		shell_print(sh, "  1=J1772, 2=current, 4=thermostat");
		return -1;
	}
	if (!app_image_valid() || !app_cb->on_sensor_change) {
		shell_error(sh, "App does not support on_sensor_change");
		return -1;
	}
	uint8_t source = (uint8_t)strtoul(argv[1], NULL, 0);
	shell_print(sh, "Triggering on_sensor_change(0x%02x)", source);
	app_cb->on_sensor_change(source);
	return 0;
}

/* Shell command registration — platform commands */
SHELL_STATIC_SUBCMD_SET_CREATE(sid_cmds,
	SHELL_CMD(status, NULL, "Show Sidewalk status", cmd_sid_status),
	SHELL_CMD(mfg, NULL, "Check MFG store", cmd_sid_mfg),
	SHELL_CMD(reinit, NULL, "Re-run Sidewalk init", cmd_sid_reinit),
	SHELL_CMD(send, NULL, "Trigger manual send (app)", cmd_sid_send_app),
	SHELL_CMD(lora, NULL, "Switch to LoRa", cmd_sid_lora),
	SHELL_CMD(ble, NULL, "Switch to BLE", cmd_sid_ble),
	SHELL_CMD(reset, NULL, "Factory reset", cmd_sid_reset),
	SHELL_CMD(ota, &ota_cmds, "OTA update commands", NULL),
	SHELL_CMD(test_change, NULL, "Test on_sensor_change callback", cmd_sid_test_change),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(sid, &sid_cmds, "Sidewalk commands", NULL);

/* App-dispatched shell commands */
SHELL_CMD_REGISTER(evse, NULL, "EVSE commands (app)", cmd_evse);
SHELL_CMD_REGISTER(hvac, NULL, "HVAC commands (app)", cmd_hvac);

/* ------------------------------------------------------------------ */
/*  App start                                                          */
/* ------------------------------------------------------------------ */

void app_start(void)
{
	LOG_INF("=== PLATFORM START ===");

	if (app_led_init()) {
		LOG_ERR("Cannot init leds");
	}

	/* Initialize OTA module and check for interrupted apply */
	ota_init(platform_api_table.send_msg);
	if (ota_boot_recovery_check()) {
		/* Recovery in progress — will reboot when done */
		return;
	}

	/* Discover app image */
	discover_app_image();

	/* Initialize app if present */
	if (app_image_valid() && app_cb->init) {
		int err = app_cb->init(&platform_api_table);
		if (err) {
			LOG_ERR("App init failed: %d", err);
			app_cb = NULL;
		} else {
			LOG_INF("App loaded and initialized");
			sensor_monitor_init(app_cb);
		}
	} else {
		LOG_WRN("Running in platform-only mode (no app image)");
	}

	/* Configure Sidewalk */
	static struct sid_event_callbacks event_callbacks = {
		.context = &sid_ctx,
		.on_event = on_sidewalk_event,
		.on_msg_received = on_sidewalk_msg_received,
		.on_msg_sent = on_sidewalk_msg_sent,
		.on_send_error = on_sidewalk_send_error,
		.on_status_changed = on_sidewalk_status_changed,
		.on_factory_reset = on_sidewalk_factory_reset,
	};

	struct sid_end_device_characteristics dev_ch = {
		.type = SID_END_DEVICE_TYPE_STATIC,
		.power_type = SID_END_DEVICE_POWERED_BY_BATTERY_AND_LINE_POWER,
		.qualification_id = 0x0001,
	};

	sid_ctx.config = (struct sid_config){
		.link_mask = SID_LINK_TYPE_1 | SID_LINK_TYPE_3,
		.dev_ch = dev_ch,
		.callbacks = &event_callbacks,
		.link_config = app_get_ble_config(),
		.sub_ghz_link_config = app_get_sub_ghz_config(),
	};

	int err = bt_gatt_authorization_cb_register(&gatt_authorization_callbacks);
	if (err) {
		LOG_ERR("Registering GATT authorization callbacks failed (err %d)", err);
		return;
	}

	/* Start Sidewalk */
	sidewalk_start(&sid_ctx);
	sidewalk_event_send(sidewalk_event_platform_init, NULL, NULL);
	sidewalk_event_send(sidewalk_event_autostart, NULL, NULL);

	/* Start periodic timer (10s initial, 60s repeat) */
	LOG_INF("Starting sensor timer (10s delay, 60s period)");
	k_timer_start(&notify_timer, K_MSEC(NOTIFY_TIMER_DURATION_MS),
		      K_MSEC(CONFIG_SID_END_DEVICE_NOTIFY_DATA_PERIOD_MS));
}
