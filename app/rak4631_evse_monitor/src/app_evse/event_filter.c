/*
 * Event Filter — change-detection wrapper for event_buffer_add()
 *
 * Compares each submitted snapshot against the last one written to the
 * buffer.  Only writes when something meaningful changed or the heartbeat
 * timer fires.
 */

#include <event_filter.h>
#include <event_buffer.h>
#include <string.h>
#include <stdlib.h>

static struct event_snapshot last;
static bool has_baseline;
static uint32_t last_write_ms;

void event_filter_init(void)
{
	memset(&last, 0, sizeof(last));
	has_baseline = false;
	last_write_ms = 0;
}

/**
 * Returns the absolute difference between two uint16_t values.
 */
static uint16_t abs_diff_u16(uint16_t a, uint16_t b)
{
	return (a > b) ? (a - b) : (b - a);
}

bool event_filter_submit(const struct event_snapshot *snap, uint32_t uptime_ms)
{
	if (!snap) {
		return false;
	}

	bool changed = false;

	if (!has_baseline) {
		/* First snapshot after init — always write */
		changed = true;
	} else {
		/* J1772 state change — always significant */
		if (snap->j1772_state != last.j1772_state) {
			changed = true;
		}

		/* Charge control flags change (relay on/off) */
		if (snap->charge_flags != last.charge_flags) {
			changed = true;
		}

		/* Thermostat flags change */
		if (snap->thermostat_flags != last.thermostat_flags) {
			changed = true;
		}

		/* Pilot voltage — only if change exceeds noise threshold */
		if (abs_diff_u16(snap->pilot_voltage_mv, last.pilot_voltage_mv)
		    > EVENT_FILTER_VOLTAGE_NOISE_MV) {
			changed = true;
		}
	}

	/* Heartbeat — write at least once per interval */
	bool heartbeat = false;
	if (!changed && has_baseline) {
		if (last_write_ms == 0 ||
		    (uptime_ms - last_write_ms) >= EVENT_FILTER_HEARTBEAT_MS) {
			heartbeat = true;
		}
	}

	if (changed || heartbeat) {
		event_buffer_add(snap);
		last = *snap;
		has_baseline = true;
		last_write_ms = uptime_ms;
		return true;
	}

	return false;
}
