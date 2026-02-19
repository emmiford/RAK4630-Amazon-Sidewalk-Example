/*
 * Event Filter — write to event buffer only on meaningful state change
 *
 * Wraps event_buffer_add() with change detection. A new entry is written
 * only when:
 *   - J1772 state changes
 *   - Charge control flags change
 *   - Thermostat flags change
 *   - Pilot voltage changes by more than VOLTAGE_NOISE_MV
 *   - Heartbeat interval expires with no other writes
 *
 * This replaces the unconditional every-poll-cycle buffer write,
 * extending buffer lifetime from ~25 seconds to hours of steady state.
 */

#ifndef EVENT_FILTER_H
#define EVENT_FILTER_H

#include <event_buffer.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Voltage must change by more than this to trigger a new entry.
 * ±2V = ±2000mV — filters ADC noise without missing real transitions. */
#ifndef EVENT_FILTER_VOLTAGE_NOISE_MV
#define EVENT_FILTER_VOLTAGE_NOISE_MV  2000
#endif

/* Minimum interval between heartbeat entries (ms).
 * Matches the uplink heartbeat so the cloud sees at least one entry
 * per interval even when the charger is idle. */
#ifndef EVENT_FILTER_HEARTBEAT_MS
#define EVENT_FILTER_HEARTBEAT_MS  300000  /* 5 minutes */
#endif

/**
 * Initialize the event filter. Must be called after event_buffer_init().
 * Clears the last-buffered state so the first submit always writes.
 */
void event_filter_init(void);

/**
 * Submit a snapshot for possible buffering.
 *
 * Compares the snapshot against the last-buffered state. Writes to the
 * event buffer only if a meaningful change is detected or the heartbeat
 * interval has elapsed.
 *
 * @param snap       Snapshot to evaluate
 * @param uptime_ms  Current uptime (for heartbeat timing)
 * @return true if the snapshot was written to the buffer
 */
bool event_filter_submit(const struct event_snapshot *snap, uint32_t uptime_ms);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_FILTER_H */
