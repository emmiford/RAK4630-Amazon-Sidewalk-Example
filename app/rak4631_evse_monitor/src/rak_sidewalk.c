/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * RAK Sidewalk EVSE Payload Implementation
 */

#include "rak_sidewalk.h"
#include "evse_sensors.h"
#include "thermostat_inputs.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rak_sidewalk, CONFIG_SIDEWALK_LOG_LEVEL);

static bool evse_initialized = false;

int rak_sidewalk_evse_init(void)
{
    int err;

    err = evse_sensors_init();
    if (err) {
        LOG_ERR("Failed to initialize EVSE sensors: %d", err);
        return err;
    }

    err = thermostat_inputs_init();
    if (err) {
        LOG_ERR("Failed to initialize thermostat inputs: %d", err);
        return err;
    }

    evse_initialized = true;
    LOG_INF("EVSE subsystems initialized");
    return 0;
}

evse_payload_t rak_sidewalk_get_payload(void)
{
    evse_payload_t payload = {0};
    j1772_state_t state;
    uint16_t pilot_mv;
    uint16_t current_ma;
    int err;

    /* Initialize if not already done */
    if (!evse_initialized) {
        err = rak_sidewalk_evse_init();
        if (err) {
            LOG_ERR("EVSE init failed: %d", err);
            payload.payload_type = EVSE_PAYLOAD_TYPE;
            payload.j1772_state = J1772_STATE_UNKNOWN;
            return payload;
        }
    }

    /* Set payload type */
    payload.payload_type = EVSE_PAYLOAD_TYPE;

    /* Read J1772 state and pilot voltage */
    err = evse_j1772_state_get(&state, &pilot_mv);
    if (err) {
        LOG_WRN("J1772 read failed: %d", err);
        payload.j1772_state = J1772_STATE_UNKNOWN;
        payload.j1772_mv = 0;
    } else {
        payload.j1772_state = (uint8_t)state;
        payload.j1772_mv = pilot_mv;
        LOG_INF("J1772: State %s, Pilot %d mV",
                j1772_state_to_string(state), pilot_mv);
    }

    /* Read current from clamp */
    err = evse_current_read(&current_ma);
    if (err) {
        LOG_WRN("Current read failed: %d", err);
        payload.current_ma = 0;
    } else {
        payload.current_ma = current_ma;
        LOG_INF("Current: %d mA", current_ma);
    }

    /* Read thermostat flags */
    payload.thermostat_flags = thermostat_flags_get();
    LOG_INF("Thermostat: Heat=%d, Cool=%d",
            (payload.thermostat_flags & THERMOSTAT_FLAG_HEAT) ? 1 : 0,
            (payload.thermostat_flags & THERMOSTAT_FLAG_COOL) ? 1 : 0);

    return payload;
}
