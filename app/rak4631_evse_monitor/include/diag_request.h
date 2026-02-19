/*
 * Diagnostics Request Interface — remote status query via 0x40 downlink
 *
 * When the cloud sends a 0x40 command, the device responds immediately
 * with an extended diagnostics uplink (magic 0xE6) containing firmware
 * version, uptime, fault state, and operational flags.
 *
 * See TDD §3.5 and §4.4.
 */

#ifndef DIAG_REQUEST_H
#define DIAG_REQUEST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Command type for diagnostics request downlink */
#define DIAG_REQUEST_CMD_TYPE  0x40

/* Diagnostics response payload constants */
#define DIAG_MAGIC    0xE6
#define DIAG_VERSION  0x01
#define DIAG_PAYLOAD_SIZE  14

/* State flags byte (byte 11) bit definitions */
#define DIAG_FLAG_SIDEWALK_READY  0x01
#define DIAG_FLAG_CHARGE_ALLOWED  0x02
#define DIAG_FLAG_CHARGE_NOW      0x04
#define DIAG_FLAG_INTERLOCK       0x08
#define DIAG_FLAG_SELFTEST_PASS   0x10
#define DIAG_FLAG_OTA_IN_PROGRESS 0x20
#define DIAG_FLAG_TIME_SYNCED     0x40

/* Error codes for last_error_code byte */
#define DIAG_ERR_NONE       0
#define DIAG_ERR_SENSOR     1
#define DIAG_ERR_CLAMP      2
#define DIAG_ERR_INTERLOCK  3
#define DIAG_ERR_SELFTEST   4

/**
 * Process a diagnostics request downlink (cmd type 0x40).
 * Sends a 0xE6 diagnostics response immediately.
 *
 * @param data  Raw payload starting with 0x40 command byte
 * @param len   Payload length (must be >= 1)
 * @return 0 on success, <0 on error
 */
int diag_request_process_cmd(const uint8_t *data, size_t len);

/**
 * Build a diagnostics response payload into the provided buffer.
 * Buffer must be at least DIAG_PAYLOAD_SIZE bytes.
 *
 * @param buf   Output buffer (>= DIAG_PAYLOAD_SIZE bytes)
 * @return Number of bytes written (DIAG_PAYLOAD_SIZE), or <0 on error
 */
int diag_request_build_response(uint8_t *buf);

/**
 * Get the highest-priority active fault as an error code.
 * Uses selftest_get_fault_flags() internally.
 *
 * @return DIAG_ERR_* code (0 = no fault)
 */
uint8_t diag_request_get_error_code(void);

/**
 * Build the state flags byte from current device state.
 *
 * @return State flags byte (see DIAG_FLAG_* defines)
 */
uint8_t diag_request_get_state_flags(void);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_REQUEST_H */
