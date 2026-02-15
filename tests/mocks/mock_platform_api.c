/*
 * Mock Platform API Implementation
 */

#include "mock_platform_api.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Configurable state */
int mock_adc_values[4];
int mock_gpio_values[4];
int mock_gpio_set_last_pin;
int mock_gpio_set_last_val;
int mock_gpio_set_call_count;
uint32_t mock_uptime_ms;
bool mock_sidewalk_ready;
uint8_t mock_last_send_buf[256];
size_t mock_last_send_len;
int mock_send_count;
int mock_send_return;
char mock_last_log[256];
int mock_log_wrn_count;

/* --- Stub implementations --- */

static int stub_send_msg(const uint8_t *data, size_t len)
{
	if (data && len <= sizeof(mock_last_send_buf)) {
		memcpy(mock_last_send_buf, data, len);
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
	return 0;
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
	return mock_adc_values[channel];
}

static int stub_gpio_get(int pin_index)
{
	if (pin_index < 0 || pin_index >= 4) {
		return -1;
	}
	return mock_gpio_values[pin_index];
}

static int stub_gpio_set(int pin_index, int val)
{
	mock_gpio_set_last_pin = pin_index;
	mock_gpio_set_last_val = val;
	mock_gpio_set_call_count++;
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
	(void)interval_ms;
	return 0;
}

static void stub_log_inf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(mock_last_log, sizeof(mock_last_log), fmt, args);
	va_end(args);
}

static void stub_log_err(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(mock_last_log, sizeof(mock_last_log), fmt, args);
	va_end(args);
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
	(void)led_id;
	(void)on;
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
	return 0;
}

static bool stub_mfg_get_dev_id(uint8_t *id_out)
{
	(void)id_out;
	return false;
}

/* --- Singleton API table --- */

static struct platform_api mock_api;

const struct platform_api *mock_platform_api_init(void)
{
	mock_platform_api_reset();

	mock_api.magic = PLATFORM_API_MAGIC;
	mock_api.version = PLATFORM_API_VERSION;

	mock_api.send_msg = stub_send_msg;
	mock_api.is_ready = stub_is_ready;
	mock_api.get_link_mask = stub_get_link_mask;
	mock_api.set_link_mask = stub_set_link_mask;
	mock_api.factory_reset = stub_factory_reset;

	mock_api.adc_read_mv = stub_adc_read_mv;
	mock_api.gpio_get = stub_gpio_get;
	mock_api.gpio_set = stub_gpio_set;
	mock_api.led_set = stub_led_set;

	mock_api.uptime_ms = stub_uptime_ms;
	mock_api.reboot = stub_reboot;

	mock_api.set_timer_interval = stub_set_timer_interval;

	mock_api.log_inf = stub_log_inf;
	mock_api.log_err = stub_log_err;
	mock_api.log_wrn = stub_log_wrn;

	mock_api.shell_print = stub_shell_print;
	mock_api.shell_error = stub_shell_error;

	mock_api.malloc = stub_malloc;
	mock_api.free = stub_free;

	mock_api.mfg_get_version = stub_mfg_get_version;
	mock_api.mfg_get_dev_id = stub_mfg_get_dev_id;

	return &mock_api;
}

void mock_platform_api_reset(void)
{
	memset(mock_adc_values, 0, sizeof(mock_adc_values));
	memset(mock_gpio_values, 0, sizeof(mock_gpio_values));
	mock_gpio_set_last_pin = -1;
	mock_gpio_set_last_val = -1;
	mock_gpio_set_call_count = 0;
	mock_uptime_ms = 0;
	mock_sidewalk_ready = false;
	memset(mock_last_send_buf, 0, sizeof(mock_last_send_buf));
	mock_last_send_len = 0;
	mock_send_count = 0;
	mock_send_return = 0;
	memset(mock_last_log, 0, sizeof(mock_last_log));
	mock_log_wrn_count = 0;
}
