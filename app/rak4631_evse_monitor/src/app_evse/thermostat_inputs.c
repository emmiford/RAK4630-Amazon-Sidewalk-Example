/*
 * Thermostat Digital Input Implementation
 *
 * App-side: GPIO access goes through platform API.
 */

#include <thermostat_inputs.h>
#include <app_platform.h>

/* GPIO pin indices — must match platform board-level mapping */
#define EVSE_PIN_COOL   2

int thermostat_inputs_init(void)
{
	/* Platform owns GPIO init */
	return 0;
}

bool thermostat_cool_call_get(void)
{
	if (!platform) {
		return false;
	}
	int val = platform->gpio_get(EVSE_PIN_COOL);
	return (val > 0);
}

uint8_t thermostat_flags_get(void)
{
	uint8_t flags = 0;

	if (thermostat_cool_call_get()) {
		flags |= THERMOSTAT_FLAG_COOL;
	}

	return flags;
}
