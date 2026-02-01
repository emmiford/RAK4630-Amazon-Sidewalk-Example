/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * EVSE Sensor Implementation for J1772 Pilot and Current Clamp
 */

#include "evse_sensors.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(evse_sensors, CONFIG_SIDEWALK_LOG_LEVEL);

/*
 * Voltage divider configuration for J1772 pilot signal:
 *
 * J1772 Pilot (±12V) ──┬── R1 (10kΩ) ──┬── To ADC (AIN0)
 *                      │               │
 *                     GND         R2 (3.3kΩ)
 *                                      │
 *                                     GND
 *
 * Divider ratio = R2 / (R1 + R2) = 3.3 / 13.3 ≈ 0.248
 *
 * Expected ADC voltages (at ADC input after divider):
 *   +12V → ~2980mV (State A: Not connected)
 *   +9V  → ~2234mV (State B: Connected)
 *   +6V  → ~1489mV (State C: Charging)
 *   +3V  → ~745mV  (State D: Ventilation)
 *   0V   → 0mV     (State E: Error)
 *   -12V → Clamped by protection diode
 */

/* Voltage divider ratio (R2 / (R1 + R2)) */
#define VOLTAGE_DIVIDER_RATIO_NUM   248
#define VOLTAGE_DIVIDER_RATIO_DEN   1000

/* Voltage thresholds at ADC input (in mV) with hysteresis */
#define J1772_THRESHOLD_A_B_MV      2600    /* Between +12V and +9V */
#define J1772_THRESHOLD_B_C_MV      1850    /* Between +9V and +6V */
#define J1772_THRESHOLD_C_D_MV      1100    /* Between +6V and +3V */
#define J1772_THRESHOLD_D_E_MV      350     /* Between +3V and 0V */

/* ADC configuration */
#define ADC_RESOLUTION      12
#define ADC_GAIN            ADC_GAIN_1_6
#define ADC_REFERENCE       ADC_REF_INTERNAL    /* 0.6V internal reference */
#define ADC_REF_MV          600                 /* Internal reference in mV */

/* With gain 1/6, full scale = 0.6V * 6 = 3.6V */
#define ADC_FULL_SCALE_MV   3600

/* Current clamp calibration */
/* Assumes 0-3.3V output corresponds to 0-30A (adjust based on your clamp) */
#define CURRENT_CLAMP_MAX_MA        30000   /* 30A max */
#define CURRENT_CLAMP_VOLTAGE_MV    3300    /* 3.3V at max current */

/* ADC channel indices */
#define ADC_CHANNEL_PILOT   0
#define ADC_CHANNEL_CURRENT 1

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
    !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No zephyr,user node with io-channels property found in devicetree"
#endif

static const struct adc_dt_spec adc_channels[] = {
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
};

static bool sensors_initialized = false;

int evse_sensors_init(void)
{
    int err;

    for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
        if (!adc_is_ready_dt(&adc_channels[i])) {
            LOG_ERR("ADC channel %d device not ready", i);
            return -ENODEV;
        }

        err = adc_channel_setup_dt(&adc_channels[i]);
        if (err < 0) {
            LOG_ERR("Failed to setup ADC channel %d: %d", i, err);
            return err;
        }
    }

    sensors_initialized = true;
    LOG_INF("EVSE sensors initialized");
    return 0;
}

static int adc_read_mv(uint8_t channel_idx, uint16_t *voltage_mv)
{
    int err;
    int16_t buf;
    int32_t val_mv;

    if (!sensors_initialized) {
        err = evse_sensors_init();
        if (err) {
            return err;
        }
    }

    if (channel_idx >= ARRAY_SIZE(adc_channels)) {
        return -EINVAL;
    }

    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    err = adc_sequence_init_dt(&adc_channels[channel_idx], &sequence);
    if (err < 0) {
        LOG_ERR("Failed to init ADC sequence: %d", err);
        return err;
    }

    err = adc_read_dt(&adc_channels[channel_idx], &sequence);
    if (err < 0) {
        LOG_ERR("ADC read failed: %d", err);
        return err;
    }

    val_mv = buf;
    err = adc_raw_to_millivolts_dt(&adc_channels[channel_idx], &val_mv);
    if (err < 0) {
        /* Fallback to manual calculation if conversion fails */
        val_mv = (buf * ADC_FULL_SCALE_MV) / (1 << ADC_RESOLUTION);
    }

    *voltage_mv = (uint16_t)val_mv;
    return 0;
}

int evse_pilot_voltage_read(uint16_t *voltage_mv)
{
    if (!voltage_mv) {
        return -EINVAL;
    }
    return adc_read_mv(ADC_CHANNEL_PILOT, voltage_mv);
}

int evse_j1772_state_get(j1772_state_t *state, uint16_t *voltage_mv)
{
    int err;
    uint16_t mv;

    if (!state) {
        return -EINVAL;
    }

    err = evse_pilot_voltage_read(&mv);
    if (err) {
        *state = J1772_STATE_UNKNOWN;
        return err;
    }

    if (voltage_mv) {
        *voltage_mv = mv;
    }

    /* Determine state from voltage thresholds */
    if (mv > J1772_THRESHOLD_A_B_MV) {
        *state = J1772_STATE_A;     /* +12V: Not connected */
    } else if (mv > J1772_THRESHOLD_B_C_MV) {
        *state = J1772_STATE_B;     /* +9V: Connected, not ready */
    } else if (mv > J1772_THRESHOLD_C_D_MV) {
        *state = J1772_STATE_C;     /* +6V: Charging */
    } else if (mv > J1772_THRESHOLD_D_E_MV) {
        *state = J1772_STATE_D;     /* +3V: Charging w/ ventilation */
    } else {
        *state = J1772_STATE_E;     /* 0V: Error */
    }

    LOG_DBG("J1772 pilot: %d mV -> State %c", mv, 'A' + *state);
    return 0;
}

int evse_current_read(uint16_t *current_ma)
{
    int err;
    uint16_t mv;

    if (!current_ma) {
        return -EINVAL;
    }

    err = adc_read_mv(ADC_CHANNEL_CURRENT, &mv);
    if (err) {
        return err;
    }

    /* Convert voltage to current based on clamp calibration */
    /* Linear mapping: 0V = 0A, 3.3V = 30A */
    *current_ma = (uint16_t)(((uint32_t)mv * CURRENT_CLAMP_MAX_MA) /
                             CURRENT_CLAMP_VOLTAGE_MV);

    LOG_DBG("Current clamp: %d mV -> %d mA", mv, *current_ma);
    return 0;
}

const char *j1772_state_to_string(j1772_state_t state)
{
    switch (state) {
    case J1772_STATE_A:
        return "A (Not connected)";
    case J1772_STATE_B:
        return "B (Connected)";
    case J1772_STATE_C:
        return "C (Charging)";
    case J1772_STATE_D:
        return "D (Ventilation)";
    case J1772_STATE_E:
        return "E (Error)";
    case J1772_STATE_F:
        return "F (EVSE Error)";
    default:
        return "Unknown";
    }
}
