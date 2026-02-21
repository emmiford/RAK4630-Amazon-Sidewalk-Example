/*
 * Time Sync Interface — device wall-clock time via cloud TIME_SYNC downlinks
 *
 * The cloud sends a 0x30 command with a 4-byte device epoch and a
 * 4-byte ACK watermark.  The device stores the sync point and derives
 * current time as: sync_time + (uptime_now - sync_uptime) / 1000.
 *
 * device epoch: seconds since 2026-01-01 00:00:00 UTC.
 */

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Command type for TIME_SYNC downlink */
#define TIME_SYNC_CMD_TYPE  0x30

/* TIME_SYNC payload: cmd(1) + epoch(4) + watermark(4) = 9 bytes */
#define TIME_SYNC_PAYLOAD_SIZE  9

/* device epoch base: 2026-01-01T00:00:00Z as Unix timestamp */
#define EPOCH_OFFSET  1767225600UL

/**
 * Initialize time sync module. Clears all state.
 * Call once during app_init().
 */
void time_sync_init(void);

/**
 * Process a TIME_SYNC downlink (cmd type 0x30).
 *
 * @param data  Raw payload starting with 0x30 command byte
 * @param len   Payload length (must be >= TIME_SYNC_PAYLOAD_SIZE)
 * @return 0 on success, <0 on error
 */
int time_sync_process_cmd(const uint8_t *data, size_t len);

/**
 * Get the current device epoch (seconds since 2026-01-01).
 * Returns 0 if time has not been synced.
 */
uint32_t time_sync_get_epoch(void);

/**
 * Get the most recent ACK watermark from the cloud.
 * Returns 0 if no TIME_SYNC has been received.
 */
uint32_t time_sync_get_ack_watermark(void);

/**
 * Check if time has been synced at least once.
 */
bool time_sync_is_synced(void);

/**
 * Get milliseconds since last sync. Returns 0 if not synced.
 */
uint32_t time_sync_ms_since_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* TIME_SYNC_H */
