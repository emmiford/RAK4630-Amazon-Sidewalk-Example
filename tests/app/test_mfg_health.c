/*
 * Unit tests for mfg_health.c — MFG key health check.
 *
 * Verifies that mfg_key_health_check() correctly detects missing
 * ED25519 and P256R1 private keys by reading from the MFG store
 * and comparing against all-zeros.
 *
 * Mocks: sid_pal_mfg_store_read() is provided here to return
 * configurable key data for each test case.
 */

#include "unity.h"
#include <mfg_health.h>
#include <sid_pal_mfg_store_ifc.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Mock MFG store state                                               */
/* ------------------------------------------------------------------ */

static uint8_t mock_ed25519_key[SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519_SIZE];
static uint8_t mock_p256r1_key[SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1_SIZE];

/* Track calls for verification */
static int mock_mfg_read_ed25519_count;
static int mock_mfg_read_p256r1_count;

void sid_pal_mfg_store_read(int value_id, uint8_t *buf, size_t len)
{
	if (value_id == SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519) {
		size_t copy_len = (len < sizeof(mock_ed25519_key)) ?
				  len : sizeof(mock_ed25519_key);
		memcpy(buf, mock_ed25519_key, copy_len);
		mock_mfg_read_ed25519_count++;
	} else if (value_id == SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1) {
		size_t copy_len = (len < sizeof(mock_p256r1_key)) ?
				  len : sizeof(mock_p256r1_key);
		memcpy(buf, mock_p256r1_key, copy_len);
		mock_mfg_read_p256r1_count++;
	}
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Fill a key buffer with non-zero test data */
static void fill_valid_key(uint8_t *key, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		key[i] = (uint8_t)(0xA0 + (i & 0x0F));
	}
}

/* ------------------------------------------------------------------ */
/*  setUp / tearDown                                                   */
/* ------------------------------------------------------------------ */

void setUp(void)
{
	memset(mock_ed25519_key, 0, sizeof(mock_ed25519_key));
	memset(mock_p256r1_key, 0, sizeof(mock_p256r1_key));
	mock_mfg_read_ed25519_count = 0;
	mock_mfg_read_p256r1_count = 0;
}

void tearDown(void) { }

/* ------------------------------------------------------------------ */
/*  Test: both keys valid                                              */
/* ------------------------------------------------------------------ */

static void test_both_keys_valid(void)
{
	fill_valid_key(mock_ed25519_key, sizeof(mock_ed25519_key));
	fill_valid_key(mock_p256r1_key, sizeof(mock_p256r1_key));

	mfg_health_result_t result = mfg_key_health_check();

	TEST_ASSERT_TRUE(result.ed25519_ok);
	TEST_ASSERT_TRUE(result.p256r1_ok);
	TEST_ASSERT_EQUAL_INT(1, mock_mfg_read_ed25519_count);
	TEST_ASSERT_EQUAL_INT(1, mock_mfg_read_p256r1_count);
}

/* ------------------------------------------------------------------ */
/*  Test: ED25519 missing (all zeros)                                  */
/* ------------------------------------------------------------------ */

static void test_ed25519_missing(void)
{
	/* ED25519 stays all-zero (default from setUp) */
	fill_valid_key(mock_p256r1_key, sizeof(mock_p256r1_key));

	mfg_health_result_t result = mfg_key_health_check();

	TEST_ASSERT_FALSE(result.ed25519_ok);
	TEST_ASSERT_TRUE(result.p256r1_ok);
}

/* ------------------------------------------------------------------ */
/*  Test: P256R1 missing (all zeros)                                   */
/* ------------------------------------------------------------------ */

static void test_p256r1_missing(void)
{
	fill_valid_key(mock_ed25519_key, sizeof(mock_ed25519_key));
	/* P256R1 stays all-zero (default from setUp) */

	mfg_health_result_t result = mfg_key_health_check();

	TEST_ASSERT_TRUE(result.ed25519_ok);
	TEST_ASSERT_FALSE(result.p256r1_ok);
}

/* ------------------------------------------------------------------ */
/*  Test: both keys missing                                            */
/* ------------------------------------------------------------------ */

static void test_both_keys_missing(void)
{
	/* Both stay all-zero (default from setUp) */

	mfg_health_result_t result = mfg_key_health_check();

	TEST_ASSERT_FALSE(result.ed25519_ok);
	TEST_ASSERT_FALSE(result.p256r1_ok);
}

/* ------------------------------------------------------------------ */
/*  Test: single non-zero byte is enough (key is not "missing")        */
/* ------------------------------------------------------------------ */

static void test_ed25519_single_nonzero_byte(void)
{
	/* Only the last byte is non-zero */
	mock_ed25519_key[31] = 0x01;
	fill_valid_key(mock_p256r1_key, sizeof(mock_p256r1_key));

	mfg_health_result_t result = mfg_key_health_check();

	TEST_ASSERT_TRUE(result.ed25519_ok);
	TEST_ASSERT_TRUE(result.p256r1_ok);
}

static void test_p256r1_single_nonzero_byte(void)
{
	fill_valid_key(mock_ed25519_key, sizeof(mock_ed25519_key));
	/* Only the first byte is non-zero */
	mock_p256r1_key[0] = 0xFF;

	mfg_health_result_t result = mfg_key_health_check();

	TEST_ASSERT_TRUE(result.ed25519_ok);
	TEST_ASSERT_TRUE(result.p256r1_ok);
}

/* ------------------------------------------------------------------ */
/*  Test: both reads are always performed (not short-circuited)        */
/* ------------------------------------------------------------------ */

static void test_both_keys_always_read(void)
{
	/* Even when ED25519 is missing, P256R1 should still be read */
	/* Both all-zero */

	mfg_key_health_check();

	TEST_ASSERT_EQUAL_INT(1, mock_mfg_read_ed25519_count);
	TEST_ASSERT_EQUAL_INT(1, mock_mfg_read_p256r1_count);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_both_keys_valid);
	RUN_TEST(test_ed25519_missing);
	RUN_TEST(test_p256r1_missing);
	RUN_TEST(test_both_keys_missing);
	RUN_TEST(test_ed25519_single_nonzero_byte);
	RUN_TEST(test_p256r1_single_nonzero_byte);
	RUN_TEST(test_both_keys_always_read);

	return UNITY_END();
}
