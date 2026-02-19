/*
 * Command Authentication — HMAC-SHA256 verification for charge control downlinks
 *
 * Cloud signs each charge control payload with a truncated HMAC-SHA256 tag
 * (8 bytes). The device verifies the tag before executing any command.
 * This prevents a compromised cloud layer from sending arbitrary commands.
 *
 * Wire format: [payload bytes] [8-byte HMAC tag]
 *   Legacy charge control: 4 + 8 = 12 bytes (fits 19-byte LoRa MTU)
 *   Delay window:         10 + 8 = 18 bytes (fits 19-byte LoRa MTU)
 */

#ifndef CMD_AUTH_H
#define CMD_AUTH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_AUTH_TAG_SIZE    8   /* truncated HMAC-SHA256 output */
#define CMD_AUTH_KEY_SIZE    32  /* HMAC key length (SHA-256 block-aligned) */

/**
 * Set the HMAC key used for command authentication.
 * Must be called before cmd_auth_verify(). Key is copied internally.
 *
 * @param key     Pointer to key bytes
 * @param key_len Key length in bytes (must be CMD_AUTH_KEY_SIZE)
 * @return 0 on success, -1 if key_len != CMD_AUTH_KEY_SIZE
 */
int cmd_auth_set_key(const uint8_t *key, size_t key_len);

/**
 * Verify the HMAC-SHA256 authentication tag on a command payload.
 *
 * @param payload     Command payload bytes (before the tag)
 * @param payload_len Length of payload
 * @param tag         Pointer to CMD_AUTH_TAG_SIZE bytes of HMAC tag
 * @return true if tag is valid, false otherwise
 */
bool cmd_auth_verify(const uint8_t *payload, size_t payload_len,
		     const uint8_t *tag);

/**
 * Check if a key has been set.
 */
bool cmd_auth_is_configured(void);

#ifdef __cplusplus
}
#endif

#endif /* CMD_AUTH_H */
