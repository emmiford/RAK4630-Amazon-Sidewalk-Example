/*
 * Charge Now 30-Minute Latch
 *
 * Single button press activates a 30-minute override:
 *   - Charging forced on (relay held closed)
 *   - Cloud pause commands ignored
 *   - FLAG_CHARGE_NOW set in uplinks for the full duration
 *   - Delay window cleared
 *
 * Cancelled early by: unplug (J1772 state A), long-press (3s),
 * or 30-minute expiry.  Power loss = latch lost (RAM-only).
 */

#ifndef CHARGE_NOW_H
#define CHARGE_NOW_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHARGE_NOW_DURATION_MS  (30UL * 60 * 1000)  /* 30 minutes */

void charge_now_init(void);
void charge_now_activate(void);
void charge_now_cancel(void);
void charge_now_tick(uint8_t j1772_state);  /* check expiry and unplug */
bool charge_now_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* CHARGE_NOW_H */
