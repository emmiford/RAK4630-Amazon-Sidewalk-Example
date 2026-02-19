/*
 * LED Blink Priority State Machine
 *
 * Table-driven blink engine for a single green LED.  Eight priority levels
 * from error (highest, 5Hz) through idle heartbeat (lowest, blip every 10s).
 * Ticks at 100ms resolution from app_on_timer().
 *
 * Yields to selftest_trigger blink codes when a button-triggered self-test
 * is running.  A short button-ack overlay (3 blinks) is available for
 * future Charge Now confirmation.
 */

#ifndef LED_ENGINE_H
#define LED_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Priority levels (0 = highest) */
typedef enum {
	LED_PRI_ERROR        = 0,  /* 5Hz fast blink */
	LED_PRI_OTA          = 1,  /* double-blink */
	LED_PRI_COMMISSION   = 2,  /* 1Hz even blink */
	LED_PRI_DISCONNECTED = 3,  /* triple-blink */
	LED_PRI_CHARGE_NOW   = 4,  /* 0.5Hz slow blink */
	LED_PRI_AC_PRIORITY  = 5,  /* heartbeat */
	LED_PRI_CHARGING     = 6,  /* solid on */
	LED_PRI_IDLE         = 7,  /* blip every 10s */
	LED_PRI_COUNT
} led_priority_t;

/* Timeouts */
#define LED_COMMISSION_TIMEOUT_MS   300000   /* 5 minutes */
#define LED_SIDEWALK_TIMEOUT_MS     600000   /* 10 minutes */
#define LED_ERROR_THRESHOLD         3        /* consecutive failures before error */

/* Module lifecycle */
void led_engine_init(void);
void led_engine_tick(void);   /* call every 100ms from app_on_timer */

/* State notifications */
void led_engine_notify_uplink_sent(void);
void led_engine_set_ota_active(bool active);
void led_engine_set_charge_now_override(bool active);

/* Error reporting */
void led_engine_report_adc_result(bool success);
void led_engine_report_gpio_result(bool success);
void led_engine_report_charge_gpio_error(void);

/* Button feedback */
void led_engine_button_ack(void);

/* Query */
led_priority_t led_engine_get_active_priority(void);
bool led_engine_is_commissioning(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_ENGINE_H */
