/*
 * Demo: OTA Blink
 *
 * Minimal app image that proves OTA works by blinking LED 0 at 1Hz
 * and sending a heartbeat every 60s with version 0xBB ("Blink").
 *
 * This is NOT production code — it's a standalone demo app that can
 * be deployed via OTA to visually confirm the update pipeline works.
 *
 * Build:
 *   nrfutil toolchain-manager launch --ncs-version v2.9.1 -- bash -c \
 *     "rm -rf build_demo && mkdir build_demo && cd build_demo && \
 *      cmake ../rak-sid/demo/ota_blink && make"
 *
 * Deploy via OTA:
 *   python3 rak-sid/aws/ota_deploy.py deploy --binary build_demo/app.bin --version 0xBB
 *
 * Restore production:
 *   python3 rak-sid/aws/ota_deploy.py deploy --build --version 7
 */

#include <platform_api.h>
#include <string.h>

static const struct platform_api *api;
static bool led_on;
static uint32_t tick_count;

/* Heartbeat every 120 ticks * 500ms = 60s */
#define HEARTBEAT_TICKS 120

/* Payload: magic + version + "BLINK\0" + tick_count_low + tick_count_high */
#define BLINK_MAGIC   0xE5
#define BLINK_VERSION 0xBB

static int app_init(const struct platform_api *platform)
{
	api = platform;
	led_on = false;
	tick_count = 0;

	/* 500ms timer = 1Hz blink (on 500ms, off 500ms) */
	api->set_timer_interval(500);

	/* Turn LED off initially */
	api->led_set(0, false);

	api->log_inf("OTA Blink demo initialized (version 0xBB)");
	return 0;
}

static void app_on_ready(bool ready)
{
	if (ready && api) {
		api->log_inf("Sidewalk READY — blink demo active");
	}
}

static void app_on_msg_received(const uint8_t *data, size_t len)
{
	/* Ignore all downlinks in demo mode */
	(void)data;
	(void)len;
}

static void app_on_msg_sent(uint32_t msg_id)
{
	if (api) {
		api->log_inf("Blink heartbeat %u sent", msg_id);
	}
}

static void app_on_send_error(uint32_t msg_id, int error)
{
	if (api) {
		api->log_err("Blink heartbeat %u error: %d", msg_id, error);
	}
}

static void app_on_timer(void)
{
	if (!api) {
		return;
	}

	/* Toggle LED 0 */
	led_on = !led_on;
	api->led_set(0, led_on);

	tick_count++;

	/* Send heartbeat every 60s */
	if (tick_count % HEARTBEAT_TICKS == 0 && api->is_ready()) {
		uint8_t payload[8] = {
			BLINK_MAGIC,
			BLINK_VERSION,
			'B', 'L', 'N', 'K',
			(uint8_t)(tick_count & 0xFF),
			(uint8_t)((tick_count >> 8) & 0xFF),
		};
		api->send_msg(payload, sizeof(payload));
		api->log_inf("Blink heartbeat sent (ticks=%u)", tick_count);
	}
}

static int app_on_shell_cmd(const char *cmd, const char *args,
			    void (*print)(const char *fmt, ...),
			    void (*error)(const char *fmt, ...))
{
	(void)error;

	if (strcmp(cmd, "evse") == 0 || strcmp(cmd, "hvac") == 0) {
		print("OTA Blink Demo v0xBB");
		print("  LED: %s", led_on ? "ON" : "OFF");
		print("  Ticks: %u", tick_count);
		print("  This is a demo app. Deploy production app to restore normal operation.");
		return 0;
	}

	print("OTA Blink Demo — unknown command: %s", cmd);
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
};
