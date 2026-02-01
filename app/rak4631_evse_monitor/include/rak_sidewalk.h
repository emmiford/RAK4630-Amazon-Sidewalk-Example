/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * RAK Sidewalk EVSE Payload Interface
 */

#ifndef __RAK_SIDEWALK_H
#define __RAK_SIDEWALK_H

#include <stdint.h>
#include "evse_sensors.h"

/* EVSE telemetry payload type identifier */
#define EVSE_PAYLOAD_TYPE   0x02

/**
 * @brief EVSE telemetry payload structure
 *
 * Total size: 7 bytes
 * - payload_type:     1 byte  (0x02 for EVSE telemetry)
 * - j1772_state:      1 byte  (J1772 state enum)
 * - j1772_mv:         2 bytes (Raw pilot voltage in mV)
 * - current_ma:       2 bytes (Current from clamp in mA)
 * - thermostat_flags: 1 byte  (Bit 0: heat, Bit 1: cool)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  payload_type;      /* 0x02 for EVSE telemetry */
    uint8_t  j1772_state;       /* J1772 state enum */
    uint16_t j1772_mv;          /* Raw pilot voltage in mV */
    uint16_t current_ma;        /* Current from clamp in mA */
    uint8_t  thermostat_flags;  /* Bit 0: heat, Bit 1: cool */
} evse_payload_t;               /* 7 bytes total */
#pragma pack(pop)

/* Legacy typedef for compatibility with existing code */
typedef evse_payload_t sidewalk_payload_t;

/**
 * @brief Read all EVSE sensors and build payload
 *
 * @return evse_payload_t Populated EVSE payload structure
 */
evse_payload_t rak_sidewalk_get_payload(void);

/**
 * @brief Initialize all EVSE subsystems
 *
 * @return 0 on success, negative errno on failure
 */
int rak_sidewalk_evse_init(void);

#endif /* __RAK_SIDEWALK_H */
