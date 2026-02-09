/*
 * Platform API Implementation
 *
 * Wraps Zephyr / Sidewalk SDK calls into the platform_api function table
 * that lives at a fixed flash address for the app image to call into.
 */

#include <platform_api.h>
#include <sidewalk.h>
#include <app_tx.h>
#include <sid_hal_memory_ifc.h>
#include <sid_pal_mfg_store_ifc.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <stdio.h>
#include <stdarg.h>

LOG_MODULE_REGISTER(platform_api, CONFIG_SIDEWALK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/*  ADC hardware (same devicetree nodes as evse_sensors.c)            */
/* ------------------------------------------------------------------ */

#if DT_NODE_EXISTS(DT_PATH(zephyr_user)) && \
    DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)

#define PLATFORM_HAS_ADC 1

static const struct adc_dt_spec platform_adc_channels[] = {
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
};
#define PLATFORM_ADC_CHANNEL_COUNT ARRAY_SIZE(platform_adc_channels)

static bool adc_initialized;

static int platform_adc_init(void)
{
	if (adc_initialized) {
		return 0;
	}
	for (size_t i = 0; i < PLATFORM_ADC_CHANNEL_COUNT; i++) {
		if (!adc_is_ready_dt(&platform_adc_channels[i])) {
			LOG_ERR("ADC ch %zu not ready", i);
			return -ENODEV;
		}
		int err = adc_channel_setup_dt(&platform_adc_channels[i]);
		if (err < 0) {
			LOG_ERR("ADC ch %zu setup err %d", i, err);
			return err;
		}
	}
	adc_initialized = true;
	return 0;
}
#else
#define PLATFORM_HAS_ADC 0
static int platform_adc_init(void) { return -ENODEV; }
#endif

/* ------------------------------------------------------------------ */
/*  GPIO hardware                                                      */
/* ------------------------------------------------------------------ */

static const struct gpio_dt_spec charge_en_gpio =
	GPIO_DT_SPEC_GET_OR(DT_NODELABEL(charge_enable), gpios, {0});

#define HEAT_CALL_NODE DT_NODELABEL(heat_call)
#define COOL_CALL_NODE DT_NODELABEL(cool_call)

static const struct gpio_dt_spec heat_call_gpio =
	GPIO_DT_SPEC_GET(HEAT_CALL_NODE, gpios);
static const struct gpio_dt_spec cool_call_gpio =
	GPIO_DT_SPEC_GET(COOL_CALL_NODE, gpios);

static bool gpio_initialized;

static int platform_gpio_init(void)
{
	if (gpio_initialized) {
		return 0;
	}

	/* Charge enable output */
	if (gpio_is_ready_dt(&charge_en_gpio)) {
		int err = gpio_pin_configure_dt(&charge_en_gpio, GPIO_OUTPUT_ACTIVE);
		if (err < 0) {
			LOG_ERR("charge_en GPIO config err %d", err);
			return err;
		}
		gpio_pin_set_dt(&charge_en_gpio, 1);
	}

	/* Heat call input */
	if (gpio_is_ready_dt(&heat_call_gpio)) {
		int err = gpio_pin_configure_dt(&heat_call_gpio, GPIO_INPUT);
		if (err < 0) {
			LOG_ERR("heat_call GPIO config err %d", err);
			return err;
		}
	}

	/* Cool call input */
	if (gpio_is_ready_dt(&cool_call_gpio)) {
		int err = gpio_pin_configure_dt(&cool_call_gpio, GPIO_INPUT);
		if (err < 0) {
			LOG_ERR("cool_call GPIO config err %d", err);
			return err;
		}
	}

	gpio_initialized = true;
	return 0;
}

/* ------------------------------------------------------------------ */
/*  API function implementations                                       */
/* ------------------------------------------------------------------ */

/* --- Sidewalk --- */

static void platform_send_msg_free(void *ctx)
{
	sidewalk_msg_t *sid_msg = (sidewalk_msg_t *)ctx;
	if (!sid_msg) {
		return;
	}
	if (sid_msg->msg.data) {
		sid_hal_free(sid_msg->msg.data);
	}
	sid_hal_free(sid_msg);
}

static int platform_send_msg(const uint8_t *data, size_t len)
{
	sidewalk_msg_t *sid_msg = sid_hal_malloc(sizeof(sidewalk_msg_t));
	if (!sid_msg) {
		return -ENOMEM;
	}
	memset(sid_msg, 0, sizeof(*sid_msg));

	sid_msg->msg.size = len;
	sid_msg->msg.data = sid_hal_malloc(len);
	if (!sid_msg->msg.data) {
		sid_hal_free(sid_msg);
		return -ENOMEM;
	}
	memcpy(sid_msg->msg.data, data, len);

	uint32_t link_mask = app_tx_get_link_mask();
	sid_msg->desc.link_type = link_mask;
	sid_msg->desc.type = SID_MSG_TYPE_NOTIFY;
	sid_msg->desc.link_mode = SID_LINK_MODE_CLOUD;
	sid_msg->desc.msg_desc_attr.tx_attr.ttl_in_seconds = 60;
	sid_msg->desc.msg_desc_attr.tx_attr.num_retries = 3;
	sid_msg->desc.msg_desc_attr.tx_attr.request_ack = true;

	if (link_mask & SID_LINK_TYPE_1) {
		sidewalk_event_send(sidewalk_event_connect, NULL, NULL);
	}

	int err = sidewalk_event_send(sidewalk_event_send_msg, sid_msg,
				      platform_send_msg_free);
	if (err) {
		platform_send_msg_free(sid_msg);
		return -EIO;
	}
	return 0;
}

static bool platform_is_ready(void)
{
	return app_tx_is_ready();
}

static int platform_get_link_mask(void)
{
	return (int)app_tx_get_link_mask();
}

static int platform_set_link_mask(uint32_t mask)
{
	app_tx_set_link_mask(mask);
	sidewalk_event_send(sidewalk_event_set_link,
			    (void *)(uintptr_t)mask, NULL);
	return 0;
}

static int platform_factory_reset(void)
{
	return sidewalk_event_send(sidewalk_event_factory_reset, NULL, NULL);
}

/* --- Hardware --- */

static int platform_adc_read_mv(int channel)
{
#if PLATFORM_HAS_ADC
	int err = platform_adc_init();
	if (err) {
		return err;
	}
	if (channel < 0 || channel >= (int)PLATFORM_ADC_CHANNEL_COUNT) {
		return -EINVAL;
	}

	int16_t buf;
	struct adc_sequence seq = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};

	err = adc_sequence_init_dt(&platform_adc_channels[channel], &seq);
	if (err < 0) {
		return err;
	}

	err = adc_read_dt(&platform_adc_channels[channel], &seq);
	if (err < 0) {
		return err;
	}

	int32_t val_mv = buf;
	err = adc_raw_to_millivolts_dt(&platform_adc_channels[channel], &val_mv);
	if (err < 0) {
		/* Fallback: gain 1/6, internal ref 0.6V → 3.6V full scale, 12-bit */
		val_mv = (buf * 3600) / 4096;
	}
	return (int)val_mv;
#else
	return -ENODEV;
#endif
}

static int platform_gpio_get(int pin_index)
{
	int err = platform_gpio_init();
	if (err) {
		return err;
	}

	switch (pin_index) {
	case PIN_CHARGE_EN:
		if (!gpio_is_ready_dt(&charge_en_gpio)) {
			return -ENODEV;
		}
		return gpio_pin_get_dt(&charge_en_gpio);
	case PIN_HEAT:
		if (!gpio_is_ready_dt(&heat_call_gpio)) {
			return -ENODEV;
		}
		return gpio_pin_get_dt(&heat_call_gpio);
	case PIN_COOL:
		if (!gpio_is_ready_dt(&cool_call_gpio)) {
			return -ENODEV;
		}
		return gpio_pin_get_dt(&cool_call_gpio);
	default:
		return -EINVAL;
	}
}

static int platform_gpio_set(int pin_index, int val)
{
	int err = platform_gpio_init();
	if (err) {
		return err;
	}

	switch (pin_index) {
	case PIN_CHARGE_EN:
		if (!gpio_is_ready_dt(&charge_en_gpio)) {
			return -ENODEV;
		}
		return gpio_pin_set_dt(&charge_en_gpio, val);
	default:
		return -EINVAL;  /* heat/cool are inputs, not settable */
	}
}

/* --- System --- */

static uint32_t platform_uptime_ms(void)
{
	return (uint32_t)k_uptime_get();
}

static void platform_reboot(void)
{
	LOG_INF("Rebooting...");
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_WARM);
}

/* --- Logging --- */

static void platform_log_inf(const char *fmt, ...)
{
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	LOG_INF("%s", buf);
}

static void platform_log_err(const char *fmt, ...)
{
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	LOG_ERR("%s", buf);
}

static void platform_log_wrn(const char *fmt, ...)
{
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	LOG_WRN("%s", buf);
}

/* Shell output — set by platform before calling app->on_shell_cmd() */
static void (*current_shell_print)(const char *fmt, ...);
static void (*current_shell_error)(const char *fmt, ...);

static void platform_shell_print(const char *fmt, ...)
{
	if (current_shell_print) {
		char buf[128];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		current_shell_print("%s", buf);
	}
}

static void platform_shell_error(const char *fmt, ...)
{
	if (current_shell_error) {
		char buf[128];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		current_shell_error("%s", buf);
	}
}

void platform_api_set_shell_handlers(
	void (*print_fn)(const char *fmt, ...),
	void (*error_fn)(const char *fmt, ...))
{
	current_shell_print = print_fn;
	current_shell_error = error_fn;
}

/* --- Memory --- */

static void *platform_malloc(size_t size)
{
	return sid_hal_malloc(size);
}

static void platform_free(void *ptr)
{
	sid_hal_free(ptr);
}

/* --- MFG diagnostics --- */

static uint32_t platform_mfg_get_version(void)
{
	return sid_pal_mfg_store_get_version();
}

static bool platform_mfg_get_dev_id(uint8_t *id_out)
{
	return sid_pal_mfg_store_dev_id_get(id_out);
}

/* ------------------------------------------------------------------ */
/*  The API table — placed at a fixed address via linker section       */
/* ------------------------------------------------------------------ */

__attribute__((section(".platform_api"), used))
const struct platform_api platform_api_table = {
	.magic           = PLATFORM_API_MAGIC,
	.version         = PLATFORM_API_VERSION,

	/* Sidewalk */
	.send_msg        = platform_send_msg,
	.is_ready        = platform_is_ready,
	.get_link_mask   = platform_get_link_mask,
	.set_link_mask   = platform_set_link_mask,
	.factory_reset   = platform_factory_reset,

	/* Hardware */
	.adc_read_mv     = platform_adc_read_mv,
	.gpio_get        = platform_gpio_get,
	.gpio_set        = platform_gpio_set,

	/* System */
	.uptime_ms       = platform_uptime_ms,
	.reboot          = platform_reboot,

	/* Logging */
	.log_inf         = platform_log_inf,
	.log_err         = platform_log_err,
	.log_wrn         = platform_log_wrn,

	/* Shell */
	.shell_print     = platform_shell_print,
	.shell_error     = platform_shell_error,

	/* Memory */
	.malloc          = platform_malloc,
	.free            = platform_free,

	/* MFG */
	.mfg_get_version = platform_mfg_get_version,
	.mfg_get_dev_id  = platform_mfg_get_dev_id,
};
