/*
 * EVSE Payload — aggregates sensor data through platform API
 */

#include <evse_payload.h>
#include <evse_sensors.h>
#include <thermostat_inputs.h>
#include <selftest.h>
#include <app_platform.h>

static bool evse_initialized;

int evse_payload_init(void)
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

evse_payload_t evse_payload_get(void)
{
	evse_payload_t payload = {0};
	j1772_state_t state;
	uint16_t pilot_mv;
	uint16_t current_ma;
	int err;

	if (!evse_initialized) {
		err = evse_payload_init();
		if (err) {
			payload.payload_type = EVSE_PAYLOAD_TYPE;
			payload.j1772_state = J1772_STATE_UNKNOWN;
			return payload;
		}
	}

	payload.payload_type = EVSE_PAYLOAD_TYPE;

	err = evse_j1772_state_get(&state, &pilot_mv);
	if (err) {
		payload.j1772_state = J1772_STATE_UNKNOWN;
		payload.j1772_mv = 0;
	} else {
		payload.j1772_state = (uint8_t)state;
		payload.j1772_mv = pilot_mv;
	}

	err = evse_current_read(&current_ma);
	if (err) {
		payload.current_ma = 0;
	} else {
		payload.current_ma = current_ma;
	}

	payload.thermostat_flags = thermostat_inputs_flags_get() | selftest_get_fault_flags();

	LOG_INF("EVSE: J1772=%d (%dmV) I=%dmA therm=0x%02x",
		payload.j1772_state, payload.j1772_mv,
		payload.current_ma, payload.thermostat_flags);

	return payload;
}
