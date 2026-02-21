/*
 * EVSE Sensor Interface for J1772 Pilot and Current Clamp
 */

#ifndef EVSE_SENSORS_H
#define EVSE_SENSORS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    J1772_STATE_A = 0,      /* Not connected (+12V) */
    J1772_STATE_B,          /* Connected, not ready (+9V) */
    J1772_STATE_C,          /* Charging (+6V) */
    J1772_STATE_D,          /* Charging w/ ventilation (+3V) */
    J1772_STATE_E,          /* Error (0V) */
    J1772_STATE_F,          /* Error (-12V) */
    J1772_STATE_UNKNOWN
} j1772_state_t;

/* Current clamp threshold: >= this value means "charging current flowing" */
#define CURRENT_ON_THRESHOLD_MA  500

int evse_sensors_init(void);
int evse_pilot_voltage_read(uint16_t *voltage_mv);
int evse_j1772_state_get(j1772_state_t *state, uint16_t *voltage_mv);
int evse_current_read(uint16_t *current_ma);
const char *evse_j1772_state_to_string(j1772_state_t state);
void evse_sensors_simulate_state(uint8_t j1772_state, uint32_t duration_ms);
bool evse_sensors_is_simulating(void);

#ifdef __cplusplus
}
#endif

#endif /* EVSE_SENSORS_H */
