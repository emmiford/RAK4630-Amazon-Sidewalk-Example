/*
 * MFG key health check implementation.
 *
 * Reads ED25519 and P256R1 private keys from the Sidewalk MFG store
 * and verifies they are non-zero. Missing keys indicate a chip erase
 * without MFG re-flash, which will cause PSA crypto errors (e.g. -149)
 * and BLE handshake failures.
 */

#include <mfg_health.h>
#include <sid_pal_mfg_store_ifc.h>
#include <string.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mfg_health, CONFIG_SIDEWALK_LOG_LEVEL);

mfg_health_result_t mfg_key_health_check(void)
{
	static const uint8_t zeros[32] = {0};
	uint8_t buf[32];
	mfg_health_result_t result = { .ed25519_ok = true, .p256r1_ok = true };

	memset(buf, 0, sizeof(buf));
	sid_pal_mfg_store_read(SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519,
			       buf, SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519_SIZE);
	if (memcmp(buf, zeros, 32) == 0) {
		LOG_ERR("MFG ED25519 private key MISSING — re-provision mfg.hex!");
		result.ed25519_ok = false;
	}

	memset(buf, 0, sizeof(buf));
	sid_pal_mfg_store_read(SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1,
			       buf, SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1_SIZE);
	if (memcmp(buf, zeros, 32) == 0) {
		LOG_ERR("MFG P256R1 private key MISSING — re-provision mfg.hex!");
		result.p256r1_ok = false;
	}

	memset(buf, 0, sizeof(buf));

	if (result.ed25519_ok && result.p256r1_ok) {
		LOG_INF("MFG key health check: OK");
	} else {
		LOG_ERR("MFG keys lost (HUK change after reflash?). BLE handshake will fail.");
		LOG_ERR("Fix: re-flash mfg.hex then app. See 'sid mfg' for details.");
	}

	return result;
}
