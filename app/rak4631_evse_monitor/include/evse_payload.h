/*
 * EVSE Payload Structure
 *
 * Domain-specific payload format for EVSE sensor data sent over Sidewalk.
 * This belongs to the app layer — the platform sends raw bytes and has
 * no knowledge of this structure.
 */

#ifndef EVSE_PAYLOAD_H
#define EVSE_PAYLOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wire-format constants (must match decode_evse_lambda.py / protocol_constants.py) */
#define EVSE_MAGIC          0xE5
#define DIAG_MAGIC          0xE6

/* Legacy payload type for sid_demo format */
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


evse_payload_t evse_payload_get(void);
int evse_payload_init(void);

#ifdef __cplusplus
}
#endif

#endif /* EVSE_PAYLOAD_H */
