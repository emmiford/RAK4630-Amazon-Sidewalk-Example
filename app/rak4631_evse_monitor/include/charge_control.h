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

struct platform_api;  /* forward declaration */

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

void charge_control_set_api(const struct platform_api *platform);
int charge_control_init(void);
int charge_control_process_cmd(const uint8_t *data, size_t len);
void charge_control_set(bool allowed, uint16_t auto_resume_min);
void charge_control_get_state(charge_control_state_t *state);
bool charge_control_is_allowed(void);
void charge_control_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* CHARGE_CONTROL_H */
