/*
 * App TX — payload formatting and sending via platform API
 */

#include <app_tx.h>
#include <rak_sidewalk.h>
#include <platform_api.h>
#include <string.h>

/* EVSE payload format constants */
#define EVSE_MAGIC   0xE5
#define EVSE_VERSION 0x06
#define EVSE_PAYLOAD_SIZE 8

/* Minimum interval between uplinks to avoid flooding on rapid state changes */
#define MIN_SEND_INTERVAL_MS  5000

static const struct platform_api *api;
static bool sidewalk_ready;
static uint32_t last_link_mask;
static uint32_t last_send_ms;

void app_tx_set_api(const struct platform_api *platform)
{
	api = platform;
	last_send_ms = 0;
}

void app_tx_set_ready(bool ready)
{
	sidewalk_ready = ready;
	if (api) {
		api->log_inf("Sidewalk %s", ready ? "READY" : "NOT READY");
	}
}

void app_tx_set_link_mask(uint32_t link_mask)
{
	if (link_mask) {
		last_link_mask = link_mask;
	}
}

bool app_tx_is_ready(void)
{
	return sidewalk_ready;
}

uint32_t app_tx_get_link_mask(void)
{
	return last_link_mask;
}

int app_tx_send_evse_data(void)
{
	if (!api) {
		return -1;
	}

	if (!api->is_ready()) {
		api->log_wrn("Sidewalk not ready, skipping send");
		return -1;
	}

	/* Rate limit: don't send more often than every 5s */
	uint32_t now = api->uptime_ms();
	if (last_send_ms && (now - last_send_ms) < MIN_SEND_INTERVAL_MS) {
		api->log_inf("TX rate-limited, skipping");
		return 0;
	}

	/* Read current sensor data */
	evse_payload_t data = rak_sidewalk_get_payload();

	/* Build 8-byte raw payload */
	uint8_t payload[EVSE_PAYLOAD_SIZE] = {
		EVSE_MAGIC,
		EVSE_VERSION,
		data.j1772_state,
		data.j1772_mv & 0xFF,
		(data.j1772_mv >> 8) & 0xFF,
		data.current_ma & 0xFF,
		(data.current_ma >> 8) & 0xFF,
		data.thermostat_flags
	};

	api->log_inf("EVSE TX: state=%d, pilot=%dmV, current=%dmA, therm=0x%02x",
		     data.j1772_state, data.j1772_mv, data.current_ma,
		     data.thermostat_flags);

	last_send_ms = now;
	return api->send_msg(payload, sizeof(payload));
}
