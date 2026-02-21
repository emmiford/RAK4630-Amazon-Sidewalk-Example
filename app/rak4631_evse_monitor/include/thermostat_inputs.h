/*
 * Thermostat Digital Input Interface
 */

#ifndef THERMOSTAT_INPUTS_H
#define THERMOSTAT_INPUTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bit 0 reserved (heat call wired but unused in v1.0) */
#define THERMOSTAT_FLAG_COOL    (1 << 1)

int thermostat_inputs_init(void);
bool thermostat_inputs_cool_call_get(void);
uint8_t thermostat_inputs_flags_get(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_INPUTS_H */
