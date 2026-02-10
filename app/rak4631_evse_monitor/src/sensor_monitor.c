/*
 * Sensor Monitor — event-driven change detection (platform side)
 *
 * Thermostat: GPIO edge interrupts with 50ms debounce.
 * J1772 pilot + current clamp: 500ms ADC poll with state comparison.
 *
 * Calls app_cb->on_sensor_change(source) on state transitions.
 */

#include "sensor_monitor.h"
#include <platform_api.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_monitor, CONFIG_SIDEWALK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/*  External: platform API table (for ADC reads)                       */
/* ------------------------------------------------------------------ */

extern const struct platform_api platform_api_table;

/* ------------------------------------------------------------------ */
/*  State                                                               */
/* ------------------------------------------------------------------ */

static const struct app_callbacks *app_cb;

/* J1772 voltage thresholds (mV) — must match evse_sensors.c mapping */
#define THRESHOLD_A_B_MV  2600
#define THRESHOLD_B_C_MV  1850
#define THRESHOLD_C_D_MV  1100
#define THRESHOLD_D_E_MV  350

/* Current on/off threshold */
#define CURRENT_ON_THRESHOLD_MA  500

/* Debounce */
#define THERMOSTAT_DEBOUNCE_MS  50
#define ADC_POLL_INTERVAL_MS    500
#define J1772_DEBOUNCE_MS       100

/* Tracked state */
static uint8_t last_j1772_state;   /* 0=A, 1=B, 2=C, 3=D, 4=E/F */
static bool last_current_on;
static uint8_t last_thermostat_flags;

/* ------------------------------------------------------------------ */
/*  J1772 voltage-to-state mapping                                     */
/* ------------------------------------------------------------------ */

static uint8_t voltage_to_j1772(int mv)
{
	if (mv >= THRESHOLD_A_B_MV)  return 0; /* State A: +12V */
	if (mv >= THRESHOLD_B_C_MV)  return 1; /* State B: +9V  */
	if (mv >= THRESHOLD_C_D_MV)  return 2; /* State C: +6V  */
	if (mv >= THRESHOLD_D_E_MV)  return 3; /* State D: +3V  */
	return 4;                               /* State E/F: <=0V */
}

/* ------------------------------------------------------------------ */
/*  Notify app                                                          */
/* ------------------------------------------------------------------ */

static void notify_app(uint8_t source)
{
	if (app_cb && app_cb->version >= 2 && app_cb->on_sensor_change) {
		app_cb->on_sensor_change(source);
	}
}

/* ------------------------------------------------------------------ */
/*  Thermostat GPIO interrupts                                          */
/* ------------------------------------------------------------------ */

#define HEAT_CALL_NODE DT_NODELABEL(heat_call)
#define COOL_CALL_NODE DT_NODELABEL(cool_call)

static const struct gpio_dt_spec heat_call_gpio =
	GPIO_DT_SPEC_GET(HEAT_CALL_NODE, gpios);
static const struct gpio_dt_spec cool_call_gpio =
	GPIO_DT_SPEC_GET(COOL_CALL_NODE, gpios);

static struct gpio_callback heat_cb_data, cool_cb_data;
static int64_t last_thermostat_change_ms;

static void thermostat_work_handler(struct k_work *work);
K_WORK_DEFINE(thermostat_work, thermostat_work_handler);

static void thermostat_gpio_isr(const struct device *dev,
				struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	k_work_submit(&thermostat_work);
}

static void thermostat_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	/* Debounce: ignore if last change was <50ms ago */
	int64_t now = k_uptime_get();
	if (last_thermostat_change_ms &&
	    (now - last_thermostat_change_ms) < THERMOSTAT_DEBOUNCE_MS) {
		return;
	}

	uint8_t flags = 0;
	if (gpio_pin_get_dt(&heat_call_gpio)) {
		flags |= 0x01;
	}
	if (gpio_pin_get_dt(&cool_call_gpio)) {
		flags |= 0x02;
	}

	if (flags != last_thermostat_flags) {
		last_thermostat_flags = flags;
		last_thermostat_change_ms = now;
		LOG_INF("Thermostat change: heat=%d cool=%d",
			(flags & 0x01) ? 1 : 0, (flags & 0x02) ? 1 : 0);
		notify_app(SENSOR_SRC_THERMOSTAT);
	}
}

/* ------------------------------------------------------------------ */
/*  ADC poll (J1772 + current) — 500ms timer                           */
/* ------------------------------------------------------------------ */

static void adc_poll_work_handler(struct k_work *work);
K_WORK_DEFINE(adc_poll_work, adc_poll_work_handler);

static void adc_poll_timer_cb(struct k_timer *timer_id);
K_TIMER_DEFINE(adc_poll_timer, adc_poll_timer_cb, NULL);

static void adc_poll_timer_cb(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	k_work_submit(&adc_poll_work);
}

static void adc_poll_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	uint8_t changed = 0;

	/* --- J1772 pilot voltage --- */
	int mv = platform_api_table.adc_read_mv(0);
	if (mv >= 0) {
		uint8_t j1772 = voltage_to_j1772(mv);
		if (j1772 != last_j1772_state) {
			/* Debounce: re-read after short delay */
			k_msleep(J1772_DEBOUNCE_MS);
			int mv2 = platform_api_table.adc_read_mv(0);
			if (mv2 >= 0) {
				uint8_t j1772_confirm = voltage_to_j1772(mv2);
				if (j1772_confirm == j1772) {
					LOG_INF("J1772 state change: %d -> %d (%d mV)",
						last_j1772_state, j1772, mv2);
					last_j1772_state = j1772;
					changed |= SENSOR_SRC_J1772;
				}
			}
		}
	}

	/* --- Current clamp (binary on/off) --- */
	int current_mv = platform_api_table.adc_read_mv(1);
	if (current_mv >= 0) {
		/* Convert: gain 1/6, 3.6V full scale, CT ratio assumed 30A:1V */
		uint16_t current_ma = (uint16_t)(((uint32_t)current_mv * 30000) / 3300);
		bool current_on = (current_ma >= CURRENT_ON_THRESHOLD_MA);
		if (current_on != last_current_on) {
			LOG_INF("Current change: %s (%d mA)",
				current_on ? "ON" : "OFF", current_ma);
			last_current_on = current_on;
			changed |= SENSOR_SRC_CURRENT;
		}
	}

	if (changed) {
		notify_app(changed);
	}
}

/* ------------------------------------------------------------------ */
/*  Init / Stop                                                         */
/* ------------------------------------------------------------------ */

int sensor_monitor_init(const struct app_callbacks *cb)
{
	if (!cb || cb->version < 2 || !cb->on_sensor_change) {
		LOG_WRN("App does not support on_sensor_change (v%u), "
			"sensor monitor disabled",
			cb ? cb->version : 0);
		return 0;
	}

	app_cb = cb;

	/* --- Thermostat GPIO interrupts --- */
	int err;

	/* Read initial thermostat state */
	last_thermostat_flags = 0;
	if (gpio_pin_get_dt(&heat_call_gpio)) {
		last_thermostat_flags |= 0x01;
	}
	if (gpio_pin_get_dt(&cool_call_gpio)) {
		last_thermostat_flags |= 0x02;
	}

	/* Heat call interrupt */
	err = gpio_pin_interrupt_configure_dt(&heat_call_gpio,
					      GPIO_INT_EDGE_BOTH);
	if (err) {
		LOG_ERR("Heat GPIO interrupt config err %d", err);
		return err;
	}
	gpio_init_callback(&heat_cb_data, thermostat_gpio_isr,
			   BIT(heat_call_gpio.pin));
	err = gpio_add_callback_dt(&heat_call_gpio, &heat_cb_data);
	if (err) {
		LOG_ERR("Heat GPIO callback add err %d", err);
		return err;
	}

	/* Cool call interrupt */
	err = gpio_pin_interrupt_configure_dt(&cool_call_gpio,
					      GPIO_INT_EDGE_BOTH);
	if (err) {
		LOG_ERR("Cool GPIO interrupt config err %d", err);
		return err;
	}
	gpio_init_callback(&cool_cb_data, thermostat_gpio_isr,
			   BIT(cool_call_gpio.pin));
	err = gpio_add_callback_dt(&cool_call_gpio, &cool_cb_data);
	if (err) {
		LOG_ERR("Cool GPIO callback add err %d", err);
		return err;
	}

	LOG_INF("Thermostat interrupts enabled (heat=%d cool=%d)",
		(last_thermostat_flags & 0x01) ? 1 : 0,
		(last_thermostat_flags & 0x02) ? 1 : 0);

	/* --- ADC poll: read initial state --- */
	int mv = platform_api_table.adc_read_mv(0);
	if (mv >= 0) {
		last_j1772_state = voltage_to_j1772(mv);
		LOG_INF("Initial J1772 state: %d (%d mV)", last_j1772_state, mv);
	}

	int cmv = platform_api_table.adc_read_mv(1);
	if (cmv >= 0) {
		uint16_t current_ma = (uint16_t)(((uint32_t)cmv * 30000) / 3300);
		last_current_on = (current_ma >= CURRENT_ON_THRESHOLD_MA);
		LOG_INF("Initial current: %s (%d mA)",
			last_current_on ? "ON" : "OFF", current_ma);
	}

	/* Start 500ms poll timer */
	k_timer_start(&adc_poll_timer, K_MSEC(ADC_POLL_INTERVAL_MS),
		      K_MSEC(ADC_POLL_INTERVAL_MS));

	LOG_INF("Sensor monitor started (500ms ADC poll + GPIO interrupts)");
	return 0;
}

void sensor_monitor_stop(void)
{
	k_timer_stop(&adc_poll_timer);

	gpio_pin_interrupt_configure_dt(&heat_call_gpio, GPIO_INT_DISABLE);
	gpio_pin_interrupt_configure_dt(&cool_call_gpio, GPIO_INT_DISABLE);
	gpio_remove_callback_dt(&heat_call_gpio, &heat_cb_data);
	gpio_remove_callback_dt(&cool_call_gpio, &cool_cb_data);

	app_cb = NULL;
	LOG_INF("Sensor monitor stopped");
}
