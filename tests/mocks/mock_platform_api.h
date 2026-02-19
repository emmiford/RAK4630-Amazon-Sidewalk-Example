/*
 * Mock Platform API for host-side unit testing.
 *
 * Provides a struct platform_api populated with stub function pointers.
 * Stubs record arguments and return configurable values.
 */

#ifndef MOCK_PLATFORM_API_H
#define MOCK_PLATFORM_API_H

#include <platform_api.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOCK_MAX_SENDS     16
#define MOCK_SEND_BUF_SIZE 64
#define MOCK_MAX_LED_CALLS 512

/* --- Configurable inputs --- */

extern int  mock_adc_values[4];
extern bool mock_adc_fail[4];           /* adc_read_mv returns -1 */

extern int  mock_gpio_values[4];
extern bool mock_gpio_fail[4];          /* gpio_get returns -1 */
extern bool mock_gpio_readback_fail[4]; /* gpio_get returns !value */

extern uint32_t mock_uptime_ms;
extern bool     mock_sidewalk_ready;

/* --- Observable outputs: sends --- */

struct mock_send_record {
	uint8_t data[MOCK_SEND_BUF_SIZE];
	size_t  len;
};

extern struct mock_send_record mock_sends[MOCK_MAX_SENDS];
extern int    mock_send_count;
extern int    mock_send_return;

/* Legacy aliases (point to sends[0]) */
extern uint8_t *mock_last_send_buf;
extern size_t   mock_last_send_len;

/* --- Observable outputs: GPIO sets --- */

extern int mock_gpio_set_last_pin;
extern int mock_gpio_set_last_val;
extern int mock_gpio_set_call_count;

/* --- Observable outputs: logging --- */

extern int  mock_log_inf_count;
extern int  mock_log_err_count;
extern int  mock_log_wrn_count;
extern char mock_last_log[256];

/* --- Observable outputs: timer --- */

extern uint32_t mock_timer_interval;

/* --- Observable outputs: LEDs --- */

extern int  mock_led_set_count;
extern int  mock_led_last_id;
extern bool mock_led_last_on;

struct mock_led_record {
	int  led_id;
	bool on;
};

extern int mock_led_call_count;
extern struct mock_led_record mock_led_calls[MOCK_MAX_LED_CALLS];

/* Legacy per-LED tracking */
extern bool mock_led_states[4];
extern int  mock_led_on_count[4];

/* Initialize and return the mock platform API table */
const struct platform_api *mock_platform_api_init(void);

/* Get the API pointer without resetting state */
const struct platform_api *mock_platform_api_get(void);

/* Reset all mock state — call in setUp() */
void mock_platform_api_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_PLATFORM_API_H */
