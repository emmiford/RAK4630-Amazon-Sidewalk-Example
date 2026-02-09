/*
 * RAK Sidewalk EVSE Payload Interface
 */

#ifndef __RAK_SIDEWALK_H
#define __RAK_SIDEWALK_H

#include <stdint.h>
#include "evse_sensors.h"

#ifdef __cplusplus
extern "C" {
#endif

struct platform_api;  /* forward declaration */

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

void rak_sidewalk_set_api(const struct platform_api *platform);
evse_payload_t rak_sidewalk_get_payload(void);
int rak_sidewalk_evse_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __RAK_SIDEWALK_H */
