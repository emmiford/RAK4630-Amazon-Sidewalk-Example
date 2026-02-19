/*
 * Delay Window Interface — time-based charge pause/resume
 *
 * Stores one active delay window [start_epoch, end_epoch] in RAM.
 * The cloud sends delay windows via 0x10 downlink with subtype 0x02.
 * When now is within the window, charging is paused. When the window
 * expires, charging resumes automatically — no cloud message needed.
 *
 * Requires TIME_SYNC to be operational (time_sync_get_epoch() != 0).
 * If time is not synced, delay windows are ignored (safe default).
 */

#ifndef DELAY_WINDOW_H
#define DELAY_WINDOW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Subtype byte in charge control downlink (0x10) */
#define DELAY_WINDOW_SUBTYPE        0x02

/* Delay window payload: cmd(1) + subtype(1) + start(4) + end(4) = 10 bytes */
#define DELAY_WINDOW_PAYLOAD_SIZE   10

void delay_window_init(void);

/**
 * Process a delay window downlink (cmd 0x10, subtype 0x02).
 *
 * @param data  Raw payload starting with 0x10
 * @param len   Payload length (must be >= DELAY_WINDOW_PAYLOAD_SIZE)
 * @return 0 on success, <0 on error
 */
int delay_window_process_cmd(const uint8_t *data, size_t len);

/**
 * Check if a delay window is currently pausing charging.
 * Returns true only if: window is set, time is synced, and start <= now <= end.
 */
bool delay_window_is_paused(void);

/**
 * Check if any delay window is stored (regardless of active/expired state).
 */
bool delay_window_has_window(void);

/**
 * Clear the stored delay window. Called by Charge Now or legacy commands.
 */
void delay_window_clear(void);

/**
 * Get the stored window boundaries.
 */
void delay_window_get(uint32_t *start, uint32_t *end);

#ifdef __cplusplus
}
#endif

#endif /* DELAY_WINDOW_H */
