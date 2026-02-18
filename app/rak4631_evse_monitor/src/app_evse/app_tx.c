/*
 * App TX — payload formatting and sending via platform API
 */

#include <app_tx.h>
#include <evse_payload.h>
#include <charge_control.h>
#include <charge_now.h>
#include <time_sync.h>
#include <platform_api.h>
#include <string.h>

/* EVSE payload format constants */
#define EVSE_MAGIC   0xE5
#define EVSE_VERSION 0x08
#define EVSE_PAYLOAD_SIZE 12

/* Control flag bits in flags byte (byte 7), bits 2-3 */
#define FLAG_CHARGE_ALLOWED  0x04   /* bit 2 */
#define FLAG_CHARGE_NOW      0x08   /* bit 3 */

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
	evse_payload_t data = evse_payload_get();

	/* OR charge control flags into the flags byte (bits 2-3) */
	uint8_t flags = data.thermostat_flags;
	if (charge_control_is_allowed()) {
		flags |= FLAG_CHARGE_ALLOWED;
	}
	if (charge_now_is_active()) {
		flags |= FLAG_CHARGE_NOW;
	}

	/* Get device-side timestamp (0 if not yet synced) */
	uint32_t timestamp = time_sync_get_epoch();

	/* Build 12-byte v0x08 payload */
	uint8_t payload[EVSE_PAYLOAD_SIZE] = {
		EVSE_MAGIC,
		EVSE_VERSION,
		data.j1772_state,
		data.j1772_mv & 0xFF,
		(data.j1772_mv >> 8) & 0xFF,
		data.current_ma & 0xFF,
		(data.current_ma >> 8) & 0xFF,
		flags,
		timestamp & 0xFF,
		(timestamp >> 8) & 0xFF,
		(timestamp >> 16) & 0xFF,
		(timestamp >> 24) & 0xFF,
	};

	api->log_inf("EVSE TX v%02x: state=%d, pilot=%dmV, current=%dmA, flags=0x%02x, ts=%u",
		     EVSE_VERSION, data.j1772_state, data.j1772_mv, data.current_ma,
		     flags, timestamp);

	last_send_ms = now;
	return api->send_msg(payload, sizeof(payload));
}
