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

struct platform_api;  /* forward declaration */

/* Bit 0 reserved (heat call wired but unused in v1.0) */
#define THERMOSTAT_FLAG_COOL    (1 << 1)

void thermostat_inputs_set_api(const struct platform_api *platform);
int thermostat_inputs_init(void);
bool thermostat_cool_call_get(void);
uint8_t thermostat_flags_get(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMOSTAT_INPUTS_H */
