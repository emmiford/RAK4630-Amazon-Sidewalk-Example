/*
 * EVSE Sensor Implementation for J1772 Pilot and Current Clamp
 *
 * App-side: all hardware access goes through the platform API.
 */

#include "evse_sensors.h"
#include <app_platform.h>
#include <string.h>

/* ADC channel indices (match platform devicetree order) */
#define ADC_CHANNEL_PILOT   0
#define ADC_CHANNEL_CURRENT 1

/* Voltage thresholds at ADC input (in mV) with hysteresis */
#define J1772_THRESHOLD_A_B_MV      2600
#define J1772_THRESHOLD_B_C_MV      1850
#define J1772_THRESHOLD_C_D_MV      1100
#define J1772_THRESHOLD_D_E_MV      350

/* Current clamp calibration: 0-3.3V = 0-30A */
#define CURRENT_CLAMP_MAX_MA        30000
#define CURRENT_CLAMP_VOLTAGE_MV    3300

/* Simulation mode state */
static bool simulation_active;
static j1772_state_t simulated_state;
static uint32_t simulation_end_ms;

int evse_sensors_init(void)
{
	/* No init needed — platform owns the ADC hardware */
	if (platform) {
		platform->log_inf("EVSE sensors ready (platform ADC)");
	}
	return 0;
}

int evse_pilot_voltage_read(uint16_t *voltage_mv)
{
	if (!voltage_mv || !platform) {
		return -1;
	}
	int mv = platform->adc_read_mv(ADC_CHANNEL_PILOT);
	if (mv < 0) {
		return mv;
	}
	*voltage_mv = (uint16_t)mv;
	return 0;
}

int evse_j1772_state_get(j1772_state_t *state, uint16_t *voltage_mv)
{
	if (!state || !platform) {
		return -1;
	}

	/* Check simulation mode */
	if (simulation_active) {
		if (platform->uptime_ms() >= simulation_end_ms) {
			simulation_active = false;
			platform->log_inf("Simulation expired, returning to real sensors");
		} else {
			*state = simulated_state;
			static const uint16_t state_voltages[] = {
				2980, 2234, 1489, 745, 0, 0
			};
			if (voltage_mv && simulated_state < 6) {
				*voltage_mv = state_voltages[simulated_state];
			}
			return 0;
		}
	}

	uint16_t mv;
	int err = evse_pilot_voltage_read(&mv);
	if (err) {
		*state = J1772_STATE_UNKNOWN;
		return err;
	}

	if (voltage_mv) {
		*voltage_mv = mv;
	}

	if (mv > J1772_THRESHOLD_A_B_MV) {
		*state = J1772_STATE_A;
	} else if (mv > J1772_THRESHOLD_B_C_MV) {
		*state = J1772_STATE_B;
	} else if (mv > J1772_THRESHOLD_C_D_MV) {
		*state = J1772_STATE_C;
	} else if (mv > J1772_THRESHOLD_D_E_MV) {
		*state = J1772_STATE_D;
	} else {
		*state = J1772_STATE_E;
	}

	return 0;
}

int evse_current_read(uint16_t *current_ma)
{
	if (!current_ma || !platform) {
		return -1;
	}

	int mv = platform->adc_read_mv(ADC_CHANNEL_CURRENT);
	if (mv < 0) {
		return mv;
	}

	*current_ma = (uint16_t)(((uint32_t)mv * CURRENT_CLAMP_MAX_MA) /
				  CURRENT_CLAMP_VOLTAGE_MV);
	return 0;
}

const char *j1772_state_to_string(j1772_state_t state)
{
	switch (state) {
	case J1772_STATE_A: return "A (Not connected)";
	case J1772_STATE_B: return "B (Connected)";
	case J1772_STATE_C: return "C (Charging)";
	case J1772_STATE_D: return "D (Ventilation)";
	case J1772_STATE_E: return "E (Error)";
	case J1772_STATE_F: return "F (EVSE Error)";
	default:            return "Unknown";
	}
}

void evse_sensors_simulate_state(uint8_t j1772_state, uint32_t duration_ms)
{
	if (!platform) {
		return;
	}

	if (duration_ms == 0) {
		simulation_active = false;
		platform->log_inf("Simulation cancelled");
		return;
	}

	if (j1772_state > J1772_STATE_F) {
		platform->log_err("Invalid J1772 state: %d", j1772_state);
		return;
	}

	simulated_state = (j1772_state_t)j1772_state;
	simulation_active = true;
	simulation_end_ms = platform->uptime_ms() + duration_ms;

	platform->log_inf("Simulating J1772 state %c for %u ms",
		     'A' + j1772_state, duration_ms);
}

bool evse_sensors_is_simulating(void)
{
	return simulation_active;
}
