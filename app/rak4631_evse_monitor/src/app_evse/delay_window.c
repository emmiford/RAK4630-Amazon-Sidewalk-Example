/*
 * Delay Window Implementation
 *
 * Stores one active delay window [start, end] in device epoch seconds.
 * Queries time_sync_get_epoch() for current device time.
 */

#include <delay_window.h>
#include <time_sync.h>
#include <app_platform.h>

/* One window at a time — new downlink replaces previous */
static struct {
	uint32_t start_epoch;
	uint32_t end_epoch;
	bool     has_window;
} window;

void delay_window_init(void)
{
	window.start_epoch = 0;
	window.end_epoch = 0;
	window.has_window = false;
}

int delay_window_process_cmd(const uint8_t *data, size_t len)
{
	if (!data || len < DELAY_WINDOW_PAYLOAD_SIZE) {
		LOG_WRN("delay_window: payload too short (%u)", (unsigned)len);
		return -1;
	}

	if (data[1] != DELAY_WINDOW_SUBTYPE) {
		LOG_WRN("delay_window: wrong subtype 0x%02x", data[1]);
		return -1;
	}

	/* Parse start (LE uint32, bytes 2-5) */
	uint32_t start = (uint32_t)data[2]
		       | ((uint32_t)data[3] << 8)
		       | ((uint32_t)data[4] << 16)
		       | ((uint32_t)data[5] << 24);

	/* Parse end (LE uint32, bytes 6-9) */
	uint32_t end = (uint32_t)data[6]
		     | ((uint32_t)data[7] << 8)
		     | ((uint32_t)data[8] << 16)
		     | ((uint32_t)data[9] << 24);

	window.start_epoch = start;
	window.end_epoch = end;
	window.has_window = true;

	LOG_INF("Delay window: start=%u end=%u (duration=%us)",
		start, end, end - start);

	return 0;
}

bool delay_window_is_paused(void)
{
	if (!window.has_window) {
		return false;
	}

	uint32_t now = time_sync_get_epoch();
	if (now == 0) {
		/* No TIME_SYNC — ignore window (safe default) */
		return false;
	}

	return (now >= window.start_epoch && now <= window.end_epoch);
}

bool delay_window_has_window(void)
{
	return window.has_window;
}

void delay_window_clear(void)
{
	if (window.has_window) {
		LOG_INF("Delay window cleared");
	}
	window.start_epoch = 0;
	window.end_epoch = 0;
	window.has_window = false;
}

void delay_window_get(uint32_t *start, uint32_t *end)
{
	if (start) *start = window.start_epoch;
	if (end)   *end   = window.end_epoch;
}
