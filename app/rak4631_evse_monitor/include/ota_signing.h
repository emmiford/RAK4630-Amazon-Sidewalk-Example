/*
 * OTA Firmware Signing — ED25519 signature verification.
 *
 * Verifies that OTA firmware images are signed with the trusted
 * developer key before applying updates. Uses a 32-byte ED25519
 * public key compiled into the platform firmware.
 */

#ifndef OTA_SIGNING_H
#define OTA_SIGNING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_SIG_SIZE 64

/**
 * Verify an ED25519 signature over firmware data.
 *
 * @param data  Firmware data (without signature)
 * @param len   Length of firmware data
 * @param sig   64-byte ED25519 signature
 * @return 0 on success, negative error code on failure
 */
int ota_verify_signature(const uint8_t *data, size_t len, const uint8_t *sig);

#ifdef __cplusplus
}
#endif

#endif /* OTA_SIGNING_H */
