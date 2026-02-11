/*
 * Thermostat Digital Input Implementation
 *
 * App-side: GPIO access goes through platform API.
 */

#include <thermostat_inputs.h>
#include <platform_api.h>

/* GPIO pin indices — must match platform board-level mapping */
#define EVSE_PIN_HEAT   1
#define EVSE_PIN_COOL   2

static const struct platform_api *api;

void thermostat_inputs_set_api(const struct platform_api *platform)
{
	api = platform;
}

int thermostat_inputs_init(void)
{
	/* Platform owns GPIO init */
	return 0;
}

bool thermostat_heat_call_get(void)
{
	if (!api) {
		return false;
	}
	int val = api->gpio_get(EVSE_PIN_HEAT);
	return (val > 0);
}

bool thermostat_cool_call_get(void)
{
	if (!api) {
		return false;
	}
	int val = api->gpio_get(EVSE_PIN_COOL);
	return (val > 0);
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

	return flags;
}
