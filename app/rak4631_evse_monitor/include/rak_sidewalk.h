/*
 * RAK Sidewalk EVSE Payload Interface
 */

#ifndef __RAK_SIDEWALK_H
#define __RAK_SIDEWALK_H

#include <stdint.h>
#include "evse_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

struct platform_api;  /* forward declaration */

void rak_sidewalk_set_api(const struct platform_api *platform);
evse_payload_t rak_sidewalk_get_payload(void);
int rak_sidewalk_evse_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __RAK_SIDEWALK_H */
