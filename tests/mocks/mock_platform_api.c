/*
 * Mock Platform API Implementation
 *
 * Single mock for all host-side tests. Consolidates the old Grenning
 * mock_platform.c and the CMake/Unity mock into one implementation.
 */

#include "mock_platform_api.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* --- Configurable inputs --- */

int  mock_adc_values[4];
bool mock_adc_fail[4];

int  mock_gpio_values[4];
bool mock_gpio_fail[4];
bool mock_gpio_readback_fail[4];

uint32_t mock_uptime_ms;
bool     mock_sidewalk_ready;

/* --- Observable outputs: sends --- */

struct mock_send_record mock_sends[MOCK_MAX_SENDS];
int    mock_send_count;
int    mock_send_return;

/* Legacy aliases */
uint8_t *mock_last_send_buf = mock_sends[0].data;
size_t   mock_last_send_len;

/* --- Observable outputs: GPIO sets --- */

int mock_gpio_set_last_pin;
int mock_gpio_set_last_val;
int mock_gpio_set_call_count;

/* --- Observable outputs: logging --- */

int  mock_log_inf_count;
int  mock_log_err_count;
int  mock_log_wrn_count;
char mock_last_log[256];

/* --- Observable outputs: timer --- */

uint32_t mock_timer_interval;

/* --- Observable outputs: LEDs --- */

int  mock_led_set_count;
int  mock_led_last_id;
bool mock_led_last_on;

int mock_led_call_count;
struct mock_led_record mock_led_calls[MOCK_MAX_LED_CALLS];

bool mock_led_states[4];
int  mock_led_on_count[4];

/* --- Stub implementations --- */

static int stub_send_msg(const uint8_t *data, size_t len)
{
	if (data && mock_send_count < MOCK_MAX_SENDS) {
		size_t copy = len < MOCK_SEND_BUF_SIZE ? len : MOCK_SEND_BUF_SIZE;
		memcpy(mock_sends[mock_send_count].data, data, copy);
		mock_sends[mock_send_count].len = len;
	}
	/* Also update legacy last-send aliases */
	if (data && len <= sizeof(mock_sends[0].data)) {
		mock_last_send_len = len;
	}
	mock_send_count++;
	return mock_send_return;
}

static bool stub_is_ready(void)
{
	return mock_sidewalk_ready;
}

static int stub_get_link_mask(void)
{
	return 4; /* SID_LINK_TYPE_3 = LoRa */
}

static int stub_set_link_mask(uint32_t mask)
{
	(void)mask;
	return 0;
}

static int stub_factory_reset(void)
{
	return 0;
}

static int stub_adc_read_mv(int channel)
{
	if (channel < 0 || channel >= 4) {
		return -1;
	}
	if (mock_adc_fail[channel]) {
		return -1;
	}
	return mock_adc_values[channel];
}

static int stub_gpio_get(int pin_index)
{
	if (pin_index < 0 || pin_index >= 4) {
		return -1;
	}
	if (mock_gpio_fail[pin_index]) {
		return -1;
	}
	if (mock_gpio_readback_fail[pin_index]) {
		return !mock_gpio_values[pin_index];
	}
	return mock_gpio_values[pin_index];
}

static int stub_gpio_set(int pin_index, int val)
{
	mock_gpio_set_call_count++;
	mock_gpio_set_last_pin = pin_index;
	mock_gpio_set_last_val = val;
	if (pin_index >= 0 && pin_index < 4) {
		mock_gpio_values[pin_index] = val;
	}
	return 0;
}

static uint32_t stub_uptime_ms(void)
{
	return mock_uptime_ms;
}

static void stub_reboot(void)
{
}

static int stub_set_timer_interval(uint32_t interval_ms)
{
	mock_timer_interval = interval_ms;
	return 0;
}

static void stub_log_inf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(mock_last_log, sizeof(mock_last_log), fmt, args);
	va_end(args);
	mock_log_inf_count++;
}

static void stub_log_err(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(mock_last_log, sizeof(mock_last_log), fmt, args);
	va_end(args);
	mock_log_err_count++;
}

static void stub_log_wrn(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(mock_last_log, sizeof(mock_last_log), fmt, args);
	va_end(args);
	mock_log_wrn_count++;
}

static void stub_led_set(int led_id, bool on)
{
	mock_led_set_count++;
	mock_led_last_id = led_id;
	mock_led_last_on = on;

	if (led_id >= 0 && led_id < 4) {
		mock_led_states[led_id] = on;
		if (on) {
			mock_led_on_count[led_id]++;
		}
	}

	if (mock_led_call_count < MOCK_MAX_LED_CALLS) {
		mock_led_calls[mock_led_call_count].led_id = led_id;
		mock_led_calls[mock_led_call_count].on = on;
	}
	mock_led_call_count++;
}

static void stub_shell_print(const char *fmt, ...)
{
	(void)fmt;
}

static void stub_shell_error(const char *fmt, ...)
{
	(void)fmt;
}

static void *stub_malloc(size_t size)
{
	(void)size;
	return NULL;
}

static void stub_free(void *ptr)
{
	(void)ptr;
}

static uint32_t stub_mfg_get_version(void)
{
	return 1;
}

static bool stub_mfg_get_dev_id(uint8_t *id_out)
{
	memset(id_out, 0xAA, 5);
	return true;
}

/* --- Singleton API table --- */

static struct platform_api mock_api;

const struct platform_api *mock_platform_api_init(void)
{
	mock_platform_api_reset();

	mock_api.magic   = PLATFORM_API_MAGIC;
	mock_api.version = PLATFORM_API_VERSION;

	mock_api.send_msg      = stub_send_msg;
	mock_api.is_ready      = stub_is_ready;
	mock_api.get_link_mask = stub_get_link_mask;
	mock_api.set_link_mask = stub_set_link_mask;
	mock_api.factory_reset = stub_factory_reset;

	mock_api.adc_read_mv = stub_adc_read_mv;
	mock_api.gpio_get    = stub_gpio_get;
	mock_api.gpio_set    = stub_gpio_set;
	mock_api.led_set     = stub_led_set;

	mock_api.uptime_ms = stub_uptime_ms;
	mock_api.reboot    = stub_reboot;

	mock_api.set_timer_interval = stub_set_timer_interval;

	mock_api.log_inf = stub_log_inf;
	mock_api.log_err = stub_log_err;
	mock_api.log_wrn = stub_log_wrn;

	mock_api.shell_print = stub_shell_print;
	mock_api.shell_error = stub_shell_error;

	mock_api.malloc = stub_malloc;
	mock_api.free   = stub_free;

	mock_api.mfg_get_version = stub_mfg_get_version;
	mock_api.mfg_get_dev_id  = stub_mfg_get_dev_id;

	return &mock_api;
}

const struct platform_api *mock_platform_api_get(void)
{
	return &mock_api;
}

void mock_platform_api_reset(void)
{
	memset(mock_adc_values, 0, sizeof(mock_adc_values));
	memset(mock_adc_fail, 0, sizeof(mock_adc_fail));
	memset(mock_gpio_values, 0, sizeof(mock_gpio_values));
	memset(mock_gpio_fail, 0, sizeof(mock_gpio_fail));
	memset(mock_gpio_readback_fail, 0, sizeof(mock_gpio_readback_fail));

	mock_gpio_set_last_pin  = -1;
	mock_gpio_set_last_val  = -1;
	mock_gpio_set_call_count = 0;

	mock_uptime_ms      = 0;
	mock_sidewalk_ready = true;  /* default to ready */

	memset(mock_sends, 0, sizeof(mock_sends));
	mock_last_send_buf = mock_sends[0].data;
	mock_last_send_len = 0;
	mock_send_count    = 0;
	mock_send_return   = 0;

	mock_led_set_count = 0;
	mock_led_last_id   = 0;
	mock_led_last_on   = false;
	mock_led_call_count = 0;
	memset(mock_led_calls, 0, sizeof(mock_led_calls));
	memset(mock_led_states, 0, sizeof(mock_led_states));
	memset(mock_led_on_count, 0, sizeof(mock_led_on_count));

	mock_log_inf_count = 0;
	mock_log_err_count = 0;
	mock_log_wrn_count = 0;
	memset(mock_last_log, 0, sizeof(mock_last_log));

	mock_timer_interval = 0;
}
