/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Thermostat Digital Input Implementation
 */

#include "thermostat_inputs.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(thermostat_inputs, CONFIG_SIDEWALK_LOG_LEVEL);

/* Device tree node references */
#define HEAT_CALL_NODE  DT_NODELABEL(heat_call)
#define COOL_CALL_NODE  DT_NODELABEL(cool_call)

#if !DT_NODE_EXISTS(HEAT_CALL_NODE)
#error "heat_call node not found in devicetree"
#endif

#if !DT_NODE_EXISTS(COOL_CALL_NODE)
#error "cool_call node not found in devicetree"
#endif

static const struct gpio_dt_spec heat_call_gpio =
    GPIO_DT_SPEC_GET(HEAT_CALL_NODE, gpios);

static const struct gpio_dt_spec cool_call_gpio =
    GPIO_DT_SPEC_GET(COOL_CALL_NODE, gpios);

static bool inputs_initialized = false;

int thermostat_inputs_init(void)
{
    int err;

    /* Initialize heat call input */
    if (!gpio_is_ready_dt(&heat_call_gpio)) {
        LOG_ERR("Heat call GPIO device not ready");
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&heat_call_gpio, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Failed to configure heat call GPIO: %d", err);
        return err;
    }

    /* Initialize cool call input */
    if (!gpio_is_ready_dt(&cool_call_gpio)) {
        LOG_ERR("Cool call GPIO device not ready");
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&cool_call_gpio, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Failed to configure cool call GPIO: %d", err);
        return err;
    }

    inputs_initialized = true;
    LOG_INF("Thermostat inputs initialized");
    return 0;
}

bool thermostat_heat_call_get(void)
{
    if (!inputs_initialized) {
        int err = thermostat_inputs_init();
        if (err) {
            return false;
        }
    }

    int val = gpio_pin_get_dt(&heat_call_gpio);
    if (val < 0) {
        LOG_ERR("Failed to read heat call GPIO: %d", val);
        return false;
    }

    return val != 0;
}

bool thermostat_cool_call_get(void)
{
    if (!inputs_initialized) {
        int err = thermostat_inputs_init();
        if (err) {
            return false;
        }
    }

    int val = gpio_pin_get_dt(&cool_call_gpio);
    if (val < 0) {
        LOG_ERR("Failed to read cool call GPIO: %d", val);
        return false;
    }

    return val != 0;
}

uint8_t thermostat_flags_get(void)
{
    uint8_t flags = 0;

    if (thermostat_heat_call_get()) {
        flags |= THERMOSTAT_FLAG_HEAT;
    }

    if (thermostat_cool_call_get()) {
        flags |= THERMOSTAT_FLAG_COOL;
    }

    LOG_DBG("Thermostat flags: 0x%02x (heat=%d, cool=%d)",
            flags,
            (flags & THERMOSTAT_FLAG_HEAT) ? 1 : 0,
            (flags & THERMOSTAT_FLAG_COOL) ? 1 : 0);

    return flags;
}
