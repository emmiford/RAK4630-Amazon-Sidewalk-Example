/*
 * Charge Control Interface
 */

#ifndef CHARGE_CONTROL_H
#define CHARGE_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHARGE_CONTROL_CMD_TYPE 0x10

typedef struct __attribute__((packed)) {
	uint8_t cmd_type;
	uint8_t charge_allowed;
	uint16_t duration_min;
} charge_control_cmd_t;

typedef struct {
	bool charging_allowed;
	uint16_t auto_resume_min;
	int64_t pause_timestamp_ms;
} charge_control_state_t;

int charge_control_init(void);
int charge_control_process_cmd(const uint8_t *data, size_t len);
/* Transition reason codes — why did charge_allowed change? */
#define TRANSITION_REASON_NONE         0x00  /* No transition (default) */
#define TRANSITION_REASON_CLOUD_CMD    0x01  /* Cloud charge control command (0x10) */
#define TRANSITION_REASON_DELAY_WINDOW 0x02  /* TOU delay window start/expire */
#define TRANSITION_REASON_CHARGE_NOW   0x03  /* Charge Now button override */
#define TRANSITION_REASON_AUTO_RESUME  0x04  /* Auto-resume timer expired */
#define TRANSITION_REASON_MANUAL       0x05  /* Shell command (app evse allow/pause) */

void charge_control_set(bool allowed, uint16_t auto_resume_min);
void charge_control_set_with_reason(bool allowed, uint16_t auto_resume_min,
				    uint8_t reason);
void charge_control_get_state(charge_control_state_t *state);
bool charge_control_is_allowed(void);
void charge_control_tick(void);

/**
 * Get the reason for the most recent charge_allowed transition.
 * Returns TRANSITION_REASON_NONE if no transition has occurred since last read.
 */
uint8_t charge_control_get_last_reason(void);

/**
 * Clear the last transition reason (after reading it into a snapshot/uplink).
 */
void charge_control_clear_last_reason(void);

#ifdef __cplusplus
}
#endif

#endif /* CHARGE_CONTROL_H */
