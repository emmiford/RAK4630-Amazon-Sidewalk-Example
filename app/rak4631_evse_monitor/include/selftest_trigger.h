/*
 * Production Self-Test Trigger
 *
 * 5-press detection on Charge Now button triggers a full boot self-test.
 * Results are reported via LED blink codes:
 *   - Green rapid blinks = passed test count
 *   - Pause
 *   - Red rapid blinks = failed test count (skipped if 0)
 *
 * If any test fails, a special uplink is sent with FAULT_SELFTEST flag.
 * Normal single-press button behavior is not affected.
 */

#ifndef SELFTEST_TRIGGER_H
#define SELFTEST_TRIGGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GPIO pin for the Charge Now button (active-high: 1 = pressed) */
#define PIN_CHARGE_NOW_BUTTON     3

/* LED IDs for blink-code output */
#define LED_GREEN  0
#define LED_RED    2

/* 5 presses within 5 seconds triggers self-test.
 * Window is 5s to accommodate 500ms GPIO polling resolution. */
#define TRIGGER_PRESS_COUNT    5
#define TRIGGER_WINDOW_MS      5000

/* Single press: fires after this timeout with no additional presses */
#define SINGLE_PRESS_TIMEOUT_MS  1500

/* Long press: held continuously for this duration cancels Charge Now */
#define LONG_PRESS_MS            3000

/* Pause between green and red blink sequences (in 500ms ticks) */
#define BLINK_PAUSE_TICKS      2

/* Number of individual checks in the boot self-test */
#define SELFTEST_CHECK_COUNT   3

/* Callback for sending uplink when self-test has failures */
typedef int (*selftest_send_fn)(void);

void selftest_trigger_set_send_fn(selftest_send_fn fn);
void selftest_trigger_init(void);
void selftest_trigger_tick(void);       /* call from app_on_timer every 500ms */
bool selftest_trigger_is_running(void); /* true while test running or blinking */

#ifdef __cplusplus
}
#endif

#endif /* SELFTEST_TRIGGER_H */
