/*
 * App RX — message parsing and command dispatch via platform API
 *
 * In the split architecture, the platform calls app->on_msg_received()
 * directly — no thread or msgq in the app. This file just holds the
 * processing logic.
 */

#include <app_rx.h>
#include <charge_control.h>
#include <time_sync.h>
#include <diag_request.h>
#include <event_buffer.h>
#include <platform_api.h>
#include <string.h>

static const struct platform_api *api;

void app_rx_set_api(const struct platform_api *platform)
{
	api = platform;
}

void app_rx_process_msg(const uint8_t *data, size_t len)
{
	if (!data || len == 0 || !api) {
		return;
	}

	/* Check for raw charge control command (first byte = 0x10) */
	if (len >= sizeof(charge_control_cmd_t) &&
	    data[0] == CHARGE_CONTROL_CMD_TYPE) {
		api->log_inf("Raw charge control command received");
		int ret = charge_control_process_cmd(data, len);
		if (ret < 0) {
			api->log_err("Charge control processing failed: %d", ret);
		} else {
			api->log_inf("Charge control: %s",
				     charge_control_is_allowed() ? "ALLOW" : "PAUSE");
		}
		return;
	}

	/* TIME_SYNC command (0x30) */
	if (data[0] == TIME_SYNC_CMD_TYPE) {
		int ret = time_sync_process_cmd(data, len);
		if (ret < 0) {
			api->log_err("TIME_SYNC processing failed: %d", ret);
		} else {
			/* Trim event buffer with new ACK watermark */
			event_buffer_trim(time_sync_get_ack_watermark());
		}
		return;
	}

	/* Diagnostics request (0x40) */
	if (data[0] == DIAG_REQUEST_CMD_TYPE) {
		int ret = diag_request_process_cmd(data, len);
		if (ret < 0) {
			api->log_err("Diagnostics request failed: %d", ret);
		}
		return;
	}

	api->log_wrn("Unknown RX message (first byte=0x%02x, len=%zu)", data[0], len);
}
