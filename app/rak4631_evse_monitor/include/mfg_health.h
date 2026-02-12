/*
 * MFG key health check — detects missing Sidewalk credentials at boot.
 *
 * Extracted from sidewalk_events.c so it can be unit-tested independently
 * of the full Sidewalk SDK initialization path.
 */
#ifndef MFG_HEALTH_H
#define MFG_HEALTH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result of the MFG key health check.
 */
typedef struct {
	bool ed25519_ok;   /**< true if ED25519 private key is non-zero */
	bool p256r1_ok;    /**< true if P256R1 private key is non-zero */
} mfg_health_result_t;

/**
 * @brief Check that MFG private keys are present (non-zero).
 *
 * Reads ED25519 and P256R1 private keys from the MFG store and checks
 * whether they are all-zeros (indicating lost/missing credentials).
 * Logs errors for any missing keys.
 *
 * @return Result struct indicating which keys are OK.
 */
mfg_health_result_t mfg_key_health_check(void);

#ifdef __cplusplus
}
#endif

#endif /* MFG_HEALTH_H */
