/*
 * Mock sid_pal_mfg_store_ifc.h for host-side unit tests.
 *
 * Provides the MFG store constants and a mockable sid_pal_mfg_store_read().
 * The mock implementation fills the output buffer from configurable test data.
 */
#ifndef SID_PAL_MFG_STORE_IFC_H_MOCK
#define SID_PAL_MFG_STORE_IFC_H_MOCK

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MFG store value IDs (matching Sidewalk SDK definitions) */
#define SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519       14
#define SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1        18

/* Key sizes */
#define SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519_SIZE  32
#define SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1_SIZE   32

/**
 * @brief Mock MFG store read function.
 *
 * Implementation is in the test file; it copies from mock_ed25519_key[]
 * or mock_p256r1_key[] depending on the value_id.
 */
void sid_pal_mfg_store_read(int value_id, uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SID_PAL_MFG_STORE_IFC_H_MOCK */
