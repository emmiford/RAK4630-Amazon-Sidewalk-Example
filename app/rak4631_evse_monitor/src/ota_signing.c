/*
 * OTA Firmware Signing — ED25519 signature verification.
 *
 * Contains the hardcoded 32-byte ED25519 public key and wraps the
 * ed25519 verify call. For host-side unit tests, mock_ota_signing.c
 * replaces this entire file.
 *
 * TODO: Integrate with Mbed TLS PSA or ed25519-donna for real
 *       verification on the nRF52840. This placeholder always returns
 *       success until the platform build integrates a verify-only
 *       ED25519 implementation.
 */

#include <ota_signing.h>

/* 32-byte ED25519 public key — replace with output of:
 *   python3 aws/ota_deploy.py keygen
 *
 * Placeholder: all zeros (will fail real verification until replaced).
 */
static const uint8_t ota_public_key[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int ota_verify_signature(const uint8_t *data, size_t len, const uint8_t *sig)
{
	(void)data;
	(void)len;
	(void)sig;
	(void)ota_public_key;

	/* TODO: Implement real ED25519 verification using:
	 *   - PSA Crypto (if Mbed TLS supports ED25519), or
	 *   - ed25519-donna verify-only (~3KB code)
	 *
	 * For now, return success. The host-side tests use a mock that
	 * exercises the full verification flow in ota_update.c.
	 */
	return 0;
}
