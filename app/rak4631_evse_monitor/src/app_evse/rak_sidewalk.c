/*
 * RAK Sidewalk EVSE Payload Implementation
 *
 * App-side: aggregates sensor data through API.
 */

#include <rak_sidewalk.h>
#include <evse_sensors.h>
#include <thermostat_inputs.h>
#include <platform_api.h>

static const struct platform_api *api;
static bool evse_initialized;

void rak_sidewalk_set_api(const struct platform_api *platform)
{
	api = platform;
}

int rak_sidewalk_evse_init(void)
{
	int err;

	err = evse_sensors_init();
	if (err) {
		if (api) api->log_err("Failed to initialize EVSE sensors: %d", err);
		return err;
	}

	err = thermostat_inputs_init();
	if (err) {
		if (api) api->log_err("Failed to initialize thermostat inputs: %d", err);
		return err;
	}

	evse_initialized = true;
	if (api) api->log_inf("EVSE subsystems initialized");
	return 0;
}

evse_payload_t rak_sidewalk_get_payload(void)
{
	evse_payload_t payload = {0};
	j1772_state_t state;
	uint16_t pilot_mv;
	uint16_t current_ma;
	int err;

	if (!evse_initialized) {
		err = rak_sidewalk_evse_init();
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

	payload.thermostat_flags = thermostat_flags_get();

	if (api) {
		api->log_inf("EVSE: J1772=%d (%dmV) I=%dmA therm=0x%02x",
			     payload.j1772_state, payload.j1772_mv,
			     payload.current_ma, payload.thermostat_flags);
	}

	return payload;
}
