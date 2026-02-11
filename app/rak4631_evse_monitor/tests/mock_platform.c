/*
 * Mock Platform API implementation for host-side unit testing.
 */

#include "mock_platform.h"
#include <string.h>
#include <stdio.h>

static struct mock_state state;

void mock_reset(void)
{
	memset(&state, 0, sizeof(state));
	state.ready = true;  /* default to ready */
}

struct mock_state *mock_get(void)
{
	return &state;
}

/* --- Mock implementations --- */

static int mock_send_msg(const uint8_t *data, size_t len)
{
	if (state.send_count < MOCK_MAX_SENDS) {
		size_t copy = len < MOCK_SEND_BUF_SIZE ? len : MOCK_SEND_BUF_SIZE;
		memcpy(state.sends[state.send_count].data, data, copy);
		state.sends[state.send_count].len = len;
	}
	state.send_count++;
	return 0;
}

static bool mock_is_ready(void)
{
	return state.ready;
}

static int mock_get_link_mask(void)
{
	return 4; /* SID_LINK_TYPE_3 = LoRa */
}

static int mock_set_link_mask(uint32_t mask)
{
	(void)mask;
	return 0;
}

static int mock_factory_reset(void)
{
	return 0;
}

static int mock_adc_read_mv(int channel)
{
	if (channel < 0 || channel >= 4) {
		return -1;
	}
	return state.adc_values[channel];
}

static int mock_gpio_get(int pin_index)
{
	if (pin_index < 0 || pin_index >= 4) {
		return -1;
	}
	return state.gpio_values[pin_index];
}

static int mock_gpio_set(int pin_index, int val)
{
	state.gpio_set_count++;
	state.gpio_last_pin = pin_index;
	state.gpio_last_val = val;
	if (pin_index >= 0 && pin_index < 4) {
		state.gpio_values[pin_index] = val;
	}
	return 0;
}

static uint32_t mock_uptime_ms(void)
{
	return state.uptime;
}

static void mock_reboot(void)
{
	/* no-op in test */
}

static int mock_set_timer_interval(uint32_t interval_ms)
{
	state.timer_interval = interval_ms;
	return 0;
}

static void mock_log_inf(const char *fmt, ...)
{
	(void)fmt;
	state.log_inf_count++;
}

static void mock_log_err(const char *fmt, ...)
{
	(void)fmt;
	state.log_err_count++;
}

static void mock_log_wrn(const char *fmt, ...)
{
	(void)fmt;
	state.log_wrn_count++;
}

static void mock_shell_print(const char *fmt, ...)
{
	(void)fmt;
}

static void mock_shell_error(const char *fmt, ...)
{
	(void)fmt;
}

static void *mock_malloc(size_t size)
{
	(void)size;
	return NULL;
}

static void mock_free(void *ptr)
{
	(void)ptr;
}

static uint32_t mock_mfg_get_version(void)
{
	return 1;
}

static bool mock_mfg_get_dev_id(uint8_t *id_out)
{
	memset(id_out, 0xAA, 5);
	return true;
}

/* --- The mock API table --- */

static const struct platform_api mock_api_table = {
	.magic           = PLATFORM_API_MAGIC,
	.version         = PLATFORM_API_VERSION,

	.send_msg        = mock_send_msg,
	.is_ready        = mock_is_ready,
	.get_link_mask   = mock_get_link_mask,
	.set_link_mask   = mock_set_link_mask,
	.factory_reset   = mock_factory_reset,

	.adc_read_mv     = mock_adc_read_mv,
	.gpio_get        = mock_gpio_get,
	.gpio_set        = mock_gpio_set,

	.uptime_ms       = mock_uptime_ms,
	.reboot          = mock_reboot,

	.set_timer_interval = mock_set_timer_interval,

	.log_inf         = mock_log_inf,
	.log_err         = mock_log_err,
	.log_wrn         = mock_log_wrn,

	.shell_print     = mock_shell_print,
	.shell_error     = mock_shell_error,

	.malloc          = mock_malloc,
	.free            = mock_free,

	.mfg_get_version = mock_mfg_get_version,
	.mfg_get_dev_id  = mock_mfg_get_dev_id,
};

const struct platform_api *mock_api(void)
{
	return &mock_api_table;
}
