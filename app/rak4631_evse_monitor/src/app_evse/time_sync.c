/*
 * Time Sync Implementation
 *
 * Receives TIME_SYNC (0x30) downlinks from the cloud and maintains
 * device wall-clock time derived from device epoch + local uptime.
 */

#include <time_sync.h>
#include <app_platform.h>

/* Sync state */
static uint32_t sync_epoch;       /* device epoch at sync point */
static uint32_t sync_uptime_ms;   /* uptime_ms() when sync was received */
static uint32_t ack_watermark;    /* last ACK watermark from cloud */
static bool     synced;           /* true after first successful sync */

void time_sync_init(void)
{
	sync_epoch = 0;
	sync_uptime_ms = 0;
	ack_watermark = 0;
	synced = false;
}

int time_sync_process_cmd(const uint8_t *data, size_t len)
{
	if (!data || len < TIME_SYNC_PAYLOAD_SIZE) {
		LOG_WRN("TIME_SYNC: payload too short (%zu)", len);
		return -1;
	}

	if (data[0] != TIME_SYNC_CMD_TYPE) {
		LOG_WRN("TIME_SYNC: wrong cmd type 0x%02x", data[0]);
		return -1;
	}

	/* Parse 4-byte device epoch (LE) */
	uint32_t epoch = (uint32_t)data[1]
		       | ((uint32_t)data[2] << 8)
		       | ((uint32_t)data[3] << 16)
		       | ((uint32_t)data[4] << 24);

	/* Parse 4-byte ACK watermark (LE) */
	uint32_t wm = (uint32_t)data[5]
		    | ((uint32_t)data[6] << 8)
		    | ((uint32_t)data[7] << 16)
		    | ((uint32_t)data[8] << 24);

	uint32_t prev_epoch = sync_epoch;
	sync_epoch = epoch;
	sync_uptime_ms = platform ? platform->uptime_ms() : 0;
	ack_watermark = wm;
	synced = true;

	if (prev_epoch) {
		uint32_t drift = (epoch > prev_epoch)
			? (epoch - prev_epoch) : (prev_epoch - epoch);
		LOG_INF("TIME_SYNC: epoch=%u wm=%u (drift ~%us from prev)",
			epoch, wm, drift);
	} else {
		LOG_INF("TIME_SYNC: epoch=%u wm=%u (first sync)", epoch, wm);
	}

	return 0;
}

uint32_t time_sync_get_epoch(void)
{
	if (!synced) {
		return 0;
	}

	uint32_t now_ms = platform ? platform->uptime_ms() : 0;
	uint32_t elapsed_s = (now_ms - sync_uptime_ms) / 1000;
	return sync_epoch + elapsed_s;
}

uint32_t time_sync_get_ack_watermark(void)
{
	return ack_watermark;
}

bool time_sync_is_synced(void)
{
	return synced;
}

uint32_t time_sync_ms_since_sync(void)
{
	if (!synced || !platform) {
		return 0;
	}
	return platform->uptime_ms() - sync_uptime_ms;
}
