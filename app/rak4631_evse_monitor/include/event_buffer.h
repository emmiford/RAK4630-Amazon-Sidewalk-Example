/*
 * Event Buffer — ring buffer of timestamped EVSE state snapshots
 *
 * Captures sensor state on every poll cycle (500ms). The cloud ACKs
 * received data via the ACK watermark in TIME_SYNC (0x30). The device
 * trims all entries at or before the watermark. If no ACK arrives,
 * the buffer wraps and overwrites the oldest entries.
 *
 * 50 entries × 12 bytes = 600 bytes from the app's 8KB RAM budget.
 */

#ifndef EVENT_BUFFER_H
#define EVENT_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVENT_BUFFER_CAPACITY  50

/* 12-byte snapshot — naturally aligned, no packing needed */
struct event_snapshot {
	uint32_t timestamp;         /* SideCharge epoch (seconds since 2026-01-01) */
	uint16_t pilot_voltage_mv;  /* J1772 pilot voltage */
	uint16_t current_ma;        /* Current clamp reading */
	uint8_t  j1772_state;       /* J1772 state code (0-6) */
	uint8_t  thermostat_flags;  /* Thermostat input bits */
	uint8_t  charge_flags;      /* bit 0: CHARGE_ALLOWED */
	uint8_t  transition_reason; /* TRANSITION_REASON_* (0 = no transition) */
};

/* charge_flags bit definitions */
#define EVENT_FLAG_CHARGE_ALLOWED  0x01

/**
 * Initialize the event buffer. Clears all entries.
 */
void event_buffer_init(void);

/**
 * Add a snapshot to the buffer. If full, overwrites the oldest entry.
 *
 * @param snap  Snapshot to add (copied into buffer)
 */
void event_buffer_add(const struct event_snapshot *snap);

/**
 * Get the most recent snapshot. Returns false if buffer is empty.
 */
bool event_buffer_get_latest(struct event_snapshot *out);

/**
 * Trim all entries with timestamp <= ack_watermark.
 * Called when a TIME_SYNC delivers a new ACK watermark.
 */
void event_buffer_trim(uint32_t ack_watermark);

/**
 * Current number of entries in the buffer.
 */
uint8_t event_buffer_count(void);

/**
 * Peek at a buffered entry by index (0 = oldest, count-1 = newest).
 * Returns false if index >= count or out is NULL.
 */
bool event_buffer_peek_at(uint8_t index, struct event_snapshot *out);

/**
 * Get the oldest entry's timestamp. Returns 0 if empty.
 */
uint32_t event_buffer_oldest_timestamp(void);

/**
 * Get the newest entry's timestamp. Returns 0 if empty.
 */
uint32_t event_buffer_newest_timestamp(void);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_BUFFER_H */
