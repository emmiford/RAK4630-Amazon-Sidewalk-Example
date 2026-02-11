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

/* Configurable ADC return values (per channel) */
extern int mock_adc_values[4];

/* Configurable GPIO input return values (per pin) */
extern int mock_gpio_values[4];

/* Last gpio_set call arguments */
extern int mock_gpio_set_last_pin;
extern int mock_gpio_set_last_val;
extern int mock_gpio_set_call_count;

/* Configurable uptime */
extern uint32_t mock_uptime_ms;

/* Sidewalk ready state */
extern bool mock_sidewalk_ready;

/* Last send_msg payload */
extern uint8_t mock_last_send_buf[256];
extern size_t mock_last_send_len;
extern int mock_send_count;
extern int mock_send_return;

/* Log capture (last message) */
extern char mock_last_log[256];
extern int mock_log_wrn_count;

/* Initialize and return the mock platform API table */
const struct platform_api *mock_platform_api_init(void);

/* Reset all mock state — call in setUp() */
void mock_platform_api_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_PLATFORM_API_H */
