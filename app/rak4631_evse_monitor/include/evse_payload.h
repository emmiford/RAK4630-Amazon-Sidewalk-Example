/*
 * EVSE Payload Structure
 *
 * Domain-specific payload format for EVSE sensor data sent over Sidewalk.
 * This belongs to the app layer — the platform sends raw bytes and has
 * no knowledge of this structure.
 */

#ifndef __EVSE_PAYLOAD_H
#define __EVSE_PAYLOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVSE_PAYLOAD_TYPE   0x02

#pragma pack(push, 1)
typedef struct {
    uint8_t  payload_type;
    uint8_t  j1772_state;
    uint16_t j1772_mv;
    uint16_t current_ma;
    uint8_t  thermostat_flags;
} evse_payload_t;
#pragma pack(pop)

typedef evse_payload_t sidewalk_payload_t;

struct platform_api;  /* forward declaration */

void evse_payload_set_api(const struct platform_api *platform);
evse_payload_t evse_payload_get(void);
int evse_payload_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __EVSE_PAYLOAD_H */
