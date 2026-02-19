/*
 * App RX — message parsing and command dispatch via platform API
 *
 * In the split architecture, the platform calls app->on_msg_received()
 * directly — no thread or msgq in the app. This file just holds the
 * processing logic.
 */

#include <app_rx.h>
#include <cmd_auth.h>
#include <charge_control.h>
#include <charge_now.h>
#include <delay_window.h>
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

	/* Charge control command family (0x10) */
	if (data[0] == CHARGE_CONTROL_CMD_TYPE) {
		/* Charge Now override: ignore all charge control commands */
		if (charge_now_is_active()) {
			api->log_inf("Charge Now active, ignoring cloud charge control");
			return;
		}

		/* Determine payload size by subtype for auth verification */
		size_t payload_len;
		if (len >= 2 && data[1] == DELAY_WINDOW_SUBTYPE) {
			payload_len = DELAY_WINDOW_PAYLOAD_SIZE;
		} else {
			payload_len = sizeof(charge_control_cmd_t);
		}

		/* Verify HMAC authentication tag (appended after payload) */
		if (cmd_auth_is_configured()) {
			if (len < payload_len + CMD_AUTH_TAG_SIZE) {
				api->log_err("Charge ctrl: missing auth tag "
					     "(got %zu, need %zu)", len,
					     payload_len + CMD_AUTH_TAG_SIZE);
				return;
			}
			if (!cmd_auth_verify(data, payload_len,
					     data + payload_len)) {
				api->log_err("Charge ctrl: auth verification "
					     "failed");
				return;
			}
			api->log_inf("Charge ctrl: auth OK");
		}

		/* Delay window subtype (0x02): 10-byte payload */
		if (payload_len == DELAY_WINDOW_PAYLOAD_SIZE &&
		    len >= DELAY_WINDOW_PAYLOAD_SIZE) {
			api->log_inf("Delay window command received");
			int ret = delay_window_process_cmd(data, payload_len);
			if (ret < 0) {
				api->log_err("Delay window processing failed: %d", ret);
			}
			return;
		}

		/* Legacy charge control (subtype 0x00/0x01): 4-byte payload */
		if (len >= sizeof(charge_control_cmd_t)) {
			api->log_inf("Charge control command received");
			int ret = charge_control_process_cmd(data, payload_len);
			if (ret < 0) {
				api->log_err("Charge control processing failed: %d", ret);
			} else {
				api->log_inf("Charge control: %s",
					     charge_control_is_allowed() ? "ALLOW" : "PAUSE");
			}
			return;
		}

		api->log_wrn("Charge control: payload too short (%zu)", len);
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
