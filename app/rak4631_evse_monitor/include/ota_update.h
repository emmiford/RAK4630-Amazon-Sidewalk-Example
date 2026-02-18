/*
 * OTA Update Module — App-Only OTA over Sidewalk
 *
 * Receives firmware chunks for the EVSE app partition via Sidewalk
 * downlinks, stages them in flash, validates CRC, and applies the
 * update by copying staging → primary app partition.
 *
 * Protocol: cmd type 0x20, subtypes for START/CHUNK/ABORT (downlink)
 * and ACK/COMPLETE/STATUS (uplink).
 */

#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ota_flash.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Protocol constants                                                  */
/* ------------------------------------------------------------------ */

#define OTA_CMD_TYPE            0x20

/* Downlink subtypes (cloud → device) */
#define OTA_SUB_START           0x01
#define OTA_SUB_CHUNK           0x02
#define OTA_SUB_ABORT           0x03

/* Uplink subtypes (device → cloud) */
#define OTA_SUB_ACK             0x80
#define OTA_SUB_COMPLETE        0x81
#define OTA_SUB_STATUS          0x82

/* Status codes */
#define OTA_STATUS_OK           0
#define OTA_STATUS_CRC_ERR      1
#define OTA_STATUS_FLASH_ERR    2
#define OTA_STATUS_NO_SESSION   3
#define OTA_STATUS_SIZE_ERR     4
#define OTA_STATUS_SIG_ERR      5

/* OTA_START flags byte (byte 19, optional) */
#define OTA_START_FLAGS_SIGNED  0x01

/* ED25519 signature size */
#define OTA_SIG_SIZE            64

/* ------------------------------------------------------------------ */
/*  Flash layout                                                        */
/* ------------------------------------------------------------------ */

#define OTA_APP_PRIMARY_ADDR    0x90000   /* App primary (256KB region) */
#define OTA_APP_PRIMARY_SIZE    0x40000   /* 256KB */
#define OTA_METADATA_ADDR       0xCFF00   /* Recovery metadata (256B) */
#define OTA_STAGING_ADDR        0xD0000   /* Staging area for incoming image */
#define OTA_STAGING_SIZE        0x24FFF   /* ~148KB (up to 0xF4FFF) */

/* OTA_FLASH_PAGE_SIZE is defined in ota_flash.h */
#define OTA_VERIFY_BUF_SIZE     16384     /* ED25519 verify buffer — must hold full app image */

/* Recovery metadata magic */
#define OTA_META_MAGIC          0x4F544155  /* "OTAU" */

/* Recovery metadata states */
#define OTA_META_STATE_NONE     0x00
#define OTA_META_STATE_STAGED   0x01  /* Image staged, ready to apply */
#define OTA_META_STATE_APPLYING 0x02  /* Copy in progress */

/* ------------------------------------------------------------------ */
/*  OTA state machine phases                                            */
/* ------------------------------------------------------------------ */

enum ota_phase {
	OTA_PHASE_IDLE = 0,
	OTA_PHASE_RECEIVING,
	OTA_PHASE_VALIDATING,
	OTA_PHASE_APPLYING,
	OTA_PHASE_COMPLETE,
	OTA_PHASE_ERROR,
};

/* ------------------------------------------------------------------ */
/*  Recovery metadata (stored at OTA_METADATA_ADDR)                     */
/* ------------------------------------------------------------------ */

struct ota_metadata {
	uint32_t magic;          /* OTA_META_MAGIC */
	uint8_t  state;          /* OTA_META_STATE_* */
	uint8_t  reserved[3];
	uint32_t image_size;     /* Size of staged image */
	uint32_t image_crc32;    /* Expected CRC32 of staged image */
	uint32_t app_version;    /* Version from OTA_START */
	uint32_t pages_copied;   /* Progress tracking for apply */
	uint32_t total_pages;    /* Total pages to copy */
};

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

/**
 * Initialize OTA module. Call once at boot.
 * @param send_fn  Function to send an uplink message (platform's send_msg)
 */
void ota_init(int (*send_fn)(const uint8_t *data, size_t len));

/**
 * Register a pre-apply hook. Called before erasing the app primary partition
 * during OTA apply. Use this to stop timers and callbacks into app code.
 */
void ota_set_pre_apply_hook(void (*fn)(void));

/**
 * Check for interrupted OTA apply and resume if needed.
 * Call before discover_app_image() at boot.
 * Returns true if a recovery was performed (device will reboot).
 */
bool ota_boot_recovery_check(void);

/**
 * Process an incoming OTA message (cmd type 0x20).
 * @param data  Raw message payload (starts with 0x20 cmd byte)
 * @param len   Length of payload
 */
void ota_process_msg(const uint8_t *data, size_t len);

/**
 * Abort any in-progress OTA session.
 */
void ota_abort(void);

/**
 * Get the current OTA phase.
 */
enum ota_phase ota_get_phase(void);

/**
 * Get the current OTA phase as a string.
 */
const char *ota_phase_str(enum ota_phase phase);

/**
 * Send an OTA_STATUS uplink with current state.
 */
void ota_send_status(void);

/**
 * Delta OTA test helpers — for flash-based testing without LoRa.
 *
 * Usage:
 *   1. Flash "old" app.bin to primary (0x90000)
 *   2. Run: sid ota delta_setup <chunk_size> <delta_count> <new_size> <new_crc>
 *   3. Use pyOCD to write changed chunks to staging (0xD0000 + idx*chunk_size)
 *   4. Run: sid ota delta_mark <abs_idx> for each written chunk
 *   5. Run: sid ota delta_apply <new_size> <new_crc> <new_version>
 */
void ota_test_delta_setup(uint16_t chunk_size, uint16_t total_delta_chunks,
			  uint32_t new_size, uint32_t new_crc32);
void ota_test_delta_mark_chunk(uint16_t abs_chunk_idx);
void ota_test_delta(uint32_t new_size, uint32_t new_crc32, uint32_t new_version);

#ifdef __cplusplus
}
#endif

#endif /* OTA_UPDATE_H */
