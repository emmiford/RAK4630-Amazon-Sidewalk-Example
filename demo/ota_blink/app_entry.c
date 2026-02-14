/*
 * Demo: OTA Blink
 *
 * Minimal app that blinks LED 0. Two build variants:
 *   Demo A (default): slow blink (1s),  version 0xB1
 *   Demo B (-DFAST):  fast blink (250ms), version 0xB2
 *
 * Flash Demo A directly, then OTA Demo B. LED visibly speeds up.
 *
 * Build Demo A:
 *   cmake ../rak-sid/demo/ota_blink && make
 * Build Demo B:
 *   cmake -DCMAKE_C_FLAGS=-DFAST ../rak-sid/demo/ota_blink && make
 */

#include <platform_api.h>
#include <string.h>

static const struct platform_api *api;
static bool led_on;
static uint32_t tick_count;

/* Config: stored in .rodata so both variants generate identical code,
 * differing only in these constant values. */
#ifdef FAST
static const uint32_t blink_ms = 250;
static const uint8_t  blink_ver = 0xB2;
#else
static const uint32_t blink_ms = 1000;
static const uint8_t  blink_ver = 0xB1;
#endif

#define BLINK_MAGIC 0xE5

static int app_init(const struct platform_api *platform)
{
	api = platform;
	led_on = false;
	tick_count = 0;
	api->set_timer_interval(blink_ms);
	api->led_set(0, false);
	api->log_inf("Blink demo v%d", blink_ver);
	return 0;
}

static void app_on_ready(bool ready)     { (void)ready; }
static void app_on_msg_received(const uint8_t *data, size_t len) { (void)data; (void)len; }
static void app_on_msg_sent(uint32_t msg_id)     { (void)msg_id; }
static void app_on_send_error(uint32_t msg_id, int error) { (void)msg_id; (void)error; }

static void app_on_timer(void)
{
	if (!api) {
		return;
	}
	led_on = !led_on;
	api->led_set(0, led_on);
	tick_count++;

	/* Heartbeat every ~60s (60 ticks at 1s or 240 at 250ms) */
	if ((tick_count & 63) == 0 && api->is_ready()) {
		uint8_t payload[8] = {
			BLINK_MAGIC, blink_ver,
			'B', 'L', 'N', 'K',
			(uint8_t)(tick_count & 0xFF),
			(uint8_t)((tick_count >> 8) & 0xFF),
		};
		api->send_msg(payload, sizeof(payload));
	}
}

static int app_on_shell_cmd(const char *cmd, const char *args,
			    void (*print)(const char *fmt, ...),
			    void (*error)(const char *fmt, ...))
{
	(void)args; (void)error; (void)cmd;
	print("Blink demo v%d", blink_ver);
	return 0;
}

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
