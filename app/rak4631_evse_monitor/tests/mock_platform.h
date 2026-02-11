/*
 * Mock Platform API for host-side unit testing.
 *
 * Provides a fake platform_api struct with controllable inputs (ADC, GPIO,
 * uptime) and observable outputs (send calls, log counts, GPIO sets).
 */

#ifndef MOCK_PLATFORM_H
#define MOCK_PLATFORM_H

#include <platform_api.h>

#define MOCK_MAX_SENDS 16
#define MOCK_SEND_BUF_SIZE 64

struct mock_send_record {
	uint8_t data[MOCK_SEND_BUF_SIZE];
	size_t len;
};

struct mock_state {
	/* Controllable inputs */
	int adc_values[4];         /* return value for adc_read_mv(channel) */
	int gpio_values[4];        /* return value for gpio_get(pin) */
	uint32_t uptime;           /* return value for uptime_ms() */
	bool ready;                /* return value for is_ready() */

	/* Observable outputs — sends */
	int send_count;
	struct mock_send_record sends[MOCK_MAX_SENDS];

	/* Observable outputs — GPIO sets */
	int gpio_set_count;
	int gpio_last_pin;
	int gpio_last_val;

	/* Observable outputs — logging */
	int log_inf_count;
	int log_wrn_count;
	int log_err_count;

	/* Observable outputs — timer */
	uint32_t timer_interval;
};

/* Reset all mock state to defaults */
void mock_reset(void);

/* Access mock state for assertions */
struct mock_state *mock_get(void);

/* Get the mock platform_api struct */
const struct platform_api *mock_api(void);

#endif /* MOCK_PLATFORM_H */
