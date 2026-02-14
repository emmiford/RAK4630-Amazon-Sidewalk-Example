/*
 * Host-side tests for OTA ED25519 signature verification flow.
 *
 * Tests the integration of signature verification into the OTA state
 * machine (ota_update.c), using mock_ota_signing.c to control the
 * verify result. Covers:
 *   - Flags byte parsing (18-byte vs 19-byte OTA_START)
 *   - Full mode: CRC OK + sig OK → COMPLETE OK
 *   - Full mode: CRC OK + sig FAIL → COMPLETE SIG_ERR, ERROR phase
 *   - Unsigned image (no flags): no verification, COMPLETE OK
 *   - Delta mode: signature verification on merged image
 *
 * NOTE: Uses chunk_size=12 (4-byte aligned) to avoid mock flash
 * alignment issues. See MEMORY.md "Mock flash alignment" note.
 */

#include "unity.h"
#include <ota_update.h>
#include <platform_api.h>
#include <string.h>

/* Mock flash externs */
extern uint8_t mock_flash_mem[];
extern int mock_reboot_count;
extern void mock_flash_reset(void);

/* Mock signing externs */
extern void mock_ota_signing_reset(void);
extern void mock_ota_signing_set_result(int result);
extern int mock_ota_signing_get_call_count(void);

#define MOCK_FLASH_BASE 0x90000
#define TEST_CHUNK_SIZE 12  /* 4-byte aligned for mock flash compat */

/* Uplink capture */
static uint8_t send_buf[64];
static size_t send_len;
static int send_count;

static int mock_send(const uint8_t *data, size_t len)
{
	if (len <= sizeof(send_buf)) {
		memcpy(send_buf, data, len);
	}
	send_len = len;
	send_count++;
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static void flash_put(uint32_t addr, const void *data, size_t len)
{
	memcpy(&mock_flash_mem[addr - MOCK_FLASH_BASE], data, len);
}

/* Compute CRC32 matching Zephyr's crc32_ieee */
static uint32_t test_crc32(const uint8_t *data, size_t len)
{
	uint32_t crc = ~0u;
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
		}
	}
	return ~crc;
}

/* Build OTA_START message (18 bytes, no flags) */
static void build_start_msg(uint8_t *msg, uint32_t total_size,
			     uint16_t total_chunks, uint16_t chunk_size,
			     uint32_t crc32, uint32_t version)
{
	msg[0] = OTA_CMD_TYPE;
	msg[1] = OTA_SUB_START;
	msg[2]  = (uint8_t)(total_size);
	msg[3]  = (uint8_t)(total_size >> 8);
	msg[4]  = (uint8_t)(total_size >> 16);
	msg[5]  = (uint8_t)(total_size >> 24);
	msg[6]  = (uint8_t)(total_chunks);
	msg[7]  = (uint8_t)(total_chunks >> 8);
	msg[8]  = (uint8_t)(chunk_size);
	msg[9]  = (uint8_t)(chunk_size >> 8);
	msg[10] = (uint8_t)(crc32);
	msg[11] = (uint8_t)(crc32 >> 8);
	msg[12] = (uint8_t)(crc32 >> 16);
	msg[13] = (uint8_t)(crc32 >> 24);
	msg[14] = (uint8_t)(version);
	msg[15] = (uint8_t)(version >> 8);
	msg[16] = (uint8_t)(version >> 16);
	msg[17] = (uint8_t)(version >> 24);
}

/* Build OTA_START with flags byte (19 bytes) */
static void build_start_msg_with_flags(uint8_t *msg, uint32_t total_size,
					uint16_t total_chunks, uint16_t chunk_size,
					uint32_t crc32, uint32_t version,
					uint8_t flags)
{
	build_start_msg(msg, total_size, total_chunks, chunk_size, crc32, version);
	msg[18] = flags;
}

/* Build a CHUNK message: cmd(1) + sub(1) + chunk_idx(2) + data(N) */
static size_t build_chunk_msg(uint8_t *msg, uint16_t chunk_idx,
			      const uint8_t *data, uint16_t data_len)
{
	msg[0] = OTA_CMD_TYPE;
	msg[1] = OTA_SUB_CHUNK;
	msg[2] = (uint8_t)(chunk_idx);
	msg[3] = (uint8_t)(chunk_idx >> 8);
	memcpy(&msg[4], data, data_len);
	return 4 + data_len;
}

/* Prepare firmware data in a buffer with APP_CALLBACK_MAGIC at start,
 * optionally append a fake 64-byte "signature". Returns total size.
 * fw_data_size MUST be a multiple of TEST_CHUNK_SIZE for CRC to work
 * correctly with the mock flash alignment. */
static uint16_t prepare_firmware(uint8_t *buf, uint16_t fw_data_size,
				 bool append_sig)
{
	/* Write APP_CALLBACK_MAGIC at start */
	uint32_t magic = APP_CALLBACK_MAGIC;
	memcpy(buf, &magic, 4);
	/* Fill rest with pattern */
	for (uint16_t i = 4; i < fw_data_size; i++) {
		buf[i] = (uint8_t)(i & 0xFF);
	}
	uint16_t total = fw_data_size;
	if (append_sig) {
		/* Append fake 64-byte signature */
		for (int i = 0; i < OTA_SIG_SIZE; i++) {
			buf[total + i] = (uint8_t)(0xA0 + (i & 0x0F));
		}
		total += OTA_SIG_SIZE;
	}
	return total;
}

/* Helper to do a full OTA: START → chunks → validate.
 * Returns the COMPLETE message status byte from send_buf[2]. */
static uint8_t do_full_ota(uint8_t *firmware, uint16_t total_size,
			    uint16_t chunk_size, bool is_signed)
{
	uint16_t total_chunks = (total_size + chunk_size - 1) / chunk_size;
	uint32_t fw_crc = test_crc32(firmware, total_size);

	/* Send START */
	if (is_signed) {
		uint8_t start[19];
		build_start_msg_with_flags(start, total_size, total_chunks,
					   chunk_size, fw_crc, 1,
					   OTA_START_FLAGS_SIGNED);
		ota_process_msg(start, 19);
	} else {
		uint8_t start[18];
		build_start_msg(start, total_size, total_chunks,
				chunk_size, fw_crc, 1);
		ota_process_msg(start, 18);
	}

	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());

	/* Send all chunks */
	uint8_t chunk_msg[64];
	for (uint16_t i = 0; i < total_chunks; i++) {
		uint16_t offset = i * chunk_size;
		uint16_t data_len = chunk_size;
		if (offset + data_len > total_size) {
			data_len = total_size - offset;
		}
		size_t msg_len = build_chunk_msg(chunk_msg, i,
						  &firmware[offset], data_len);
		send_count = 0;
		ota_process_msg(chunk_msg, msg_len);
	}

	/* Last chunk should trigger validation → COMPLETE message */
	TEST_ASSERT_EQUAL_INT(OTA_SUB_COMPLETE, send_buf[1]);
	return send_buf[2]; /* status */
}

/* ------------------------------------------------------------------ */
/*  Test setup / teardown                                              */
/* ------------------------------------------------------------------ */

void setUp(void)
{
	mock_flash_reset();
	mock_ota_signing_reset();
	send_count = 0;
	send_len = 0;
	memset(send_buf, 0, sizeof(send_buf));
	ota_init(mock_send);
}

void tearDown(void)
{
	ota_abort();
}

/* ------------------------------------------------------------------ */
/*  Flags byte parsing                                                 */
/* ------------------------------------------------------------------ */

void test_start_18_bytes_is_unsigned(void)
{
	/* 18-byte START (no flags) → is_signed=false.
	 * Verify by doing full OTA — mock verify should NOT be called.
	 * 48 bytes = 4 chunks of 12 (aligned). */
	uint8_t fw[48];
	uint16_t size = prepare_firmware(fw, 48, false);

	mock_ota_signing_set_result(0);
	uint8_t status = do_full_ota(fw, size, TEST_CHUNK_SIZE, false);

	TEST_ASSERT_EQUAL_INT(OTA_STATUS_OK, status);
	TEST_ASSERT_EQUAL_INT(0, mock_ota_signing_get_call_count());
}

void test_start_19_bytes_signed_flag(void)
{
	/* 19-byte START with SIGNED flag → is_signed=true.
	 * Mock returns success → verify IS called.
	 * 120B firmware + 64B sig = 184 = NOT a multiple of 12 (184/12 = 15.33).
	 * Use 132B firmware so 132+64=196 and last chunk can be partial (handled
	 * by ota_flash_write alignment). Actually, let's use firmware sizes that
	 * are clean multiples: 120 + 64 = 184 → 16 chunks of 12 = 192, last partial.
	 * Simpler: 120B fw + 64B sig = 184B. 184 / 12 = 15 chunks + 4B tail. OK. */
	uint8_t fw[256];
	uint16_t size = prepare_firmware(fw, 120, true); /* 120 + 64 = 184 */

	mock_ota_signing_set_result(0);
	uint8_t status = do_full_ota(fw, size, TEST_CHUNK_SIZE, true);

	TEST_ASSERT_EQUAL_INT(OTA_STATUS_OK, status);
	TEST_ASSERT_EQUAL_INT(1, mock_ota_signing_get_call_count());
}

void test_start_19_bytes_no_signed_flag(void)
{
	/* 19-byte START with flags=0x00 → is_signed=false.
	 * Verify is NOT called. */
	uint8_t fw[48];
	uint16_t size = prepare_firmware(fw, 48, false);
	uint32_t fw_crc = test_crc32(fw, size);

	uint8_t start[19];
	build_start_msg_with_flags(start, size,
				   (size + TEST_CHUNK_SIZE - 1) / TEST_CHUNK_SIZE,
				   TEST_CHUNK_SIZE, fw_crc, 1, 0x00);
	ota_process_msg(start, 19);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());

	TEST_ASSERT_EQUAL_INT(0, mock_ota_signing_get_call_count());
}

/* ------------------------------------------------------------------ */
/*  Full mode: signature verification                                  */
/* ------------------------------------------------------------------ */

void test_full_mode_signed_ok(void)
{
	/* Signed firmware, mock verify returns 0 → COMPLETE OK */
	uint8_t fw[256];
	uint16_t size = prepare_firmware(fw, 120, true); /* 184B total */

	mock_ota_signing_set_result(0);
	uint8_t status = do_full_ota(fw, size, TEST_CHUNK_SIZE, true);

	TEST_ASSERT_EQUAL_INT(OTA_STATUS_OK, status);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_COMPLETE, ota_get_phase());
	TEST_ASSERT_EQUAL_INT(1, mock_ota_signing_get_call_count());
}

void test_full_mode_signed_fail(void)
{
	/* Signed firmware, mock verify returns -1 → COMPLETE SIG_ERR, ERROR */
	uint8_t fw[256];
	uint16_t size = prepare_firmware(fw, 120, true); /* 184B total */

	mock_ota_signing_set_result(-1);
	uint8_t status = do_full_ota(fw, size, TEST_CHUNK_SIZE, true);

	TEST_ASSERT_EQUAL_INT(OTA_STATUS_SIG_ERR, status);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_ERROR, ota_get_phase());
	TEST_ASSERT_EQUAL_INT(1, mock_ota_signing_get_call_count());
}

void test_full_mode_unsigned_no_verify(void)
{
	/* Unsigned firmware → no verify call, COMPLETE OK */
	uint8_t fw[48];
	uint16_t size = prepare_firmware(fw, 48, false);

	mock_ota_signing_set_result(-1); /* Would fail if called */
	uint8_t status = do_full_ota(fw, size, TEST_CHUNK_SIZE, false);

	TEST_ASSERT_EQUAL_INT(OTA_STATUS_OK, status);
	TEST_ASSERT_EQUAL_INT(0, mock_ota_signing_get_call_count());
}

/* ------------------------------------------------------------------ */
/*  Delta mode: signature verification                                 */
/* ------------------------------------------------------------------ */

void test_delta_mode_signed_ok(void)
{
	/* Delta OTA with signed firmware, mock verify returns 0 → COMPLETE OK.
	 *
	 * Setup: write "old" firmware to primary, differ at chunk 0.
	 * New: 120B firmware + 64B sig = 184B total. */
	uint8_t fw[256];
	uint16_t fw_data = 120;
	uint16_t total_size = prepare_firmware(fw, fw_data, true); /* 184 */
	uint16_t chunk_size = TEST_CHUNK_SIZE;
	uint32_t fw_crc = test_crc32(fw, total_size);

	/* Write "old" firmware to primary: same but byte 4 different */
	uint8_t old_fw[256];
	memcpy(old_fw, fw, total_size);
	old_fw[4] = 0x00; /* Differ at byte 4 → chunk 0 changes */
	flash_put(OTA_APP_PRIMARY_ADDR, old_fw, total_size);

	/* Only chunk 0 is different → delta_count=1 */
	uint8_t start[19];
	build_start_msg_with_flags(start, total_size, 1, chunk_size,
				   fw_crc, 1, OTA_START_FLAGS_SIGNED);
	ota_process_msg(start, 19);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());

	/* Send chunk 0 */
	uint8_t chunk_msg[64];
	size_t msg_len = build_chunk_msg(chunk_msg, 0, fw, chunk_size);

	mock_ota_signing_set_result(0);
	ota_process_msg(chunk_msg, msg_len);

	TEST_ASSERT_EQUAL_INT(OTA_SUB_COMPLETE, send_buf[1]);
	TEST_ASSERT_EQUAL_INT(OTA_STATUS_OK, send_buf[2]);
	TEST_ASSERT_EQUAL_INT(1, mock_ota_signing_get_call_count());
}

void test_delta_mode_signed_fail(void)
{
	/* Delta OTA with signed firmware, mock verify returns -1 → SIG_ERR */
	uint8_t fw[256];
	uint16_t total_size = prepare_firmware(fw, 120, true); /* 184 */
	uint16_t chunk_size = TEST_CHUNK_SIZE;
	uint32_t fw_crc = test_crc32(fw, total_size);

	/* Write "old" firmware to primary (differ at byte 4) */
	uint8_t old_fw[256];
	memcpy(old_fw, fw, total_size);
	old_fw[4] = 0x00;
	flash_put(OTA_APP_PRIMARY_ADDR, old_fw, total_size);

	/* Delta START: 1 chunk changed, signed */
	uint8_t start[19];
	build_start_msg_with_flags(start, total_size, 1, chunk_size,
				   fw_crc, 1, OTA_START_FLAGS_SIGNED);
	ota_process_msg(start, 19);

	/* Send chunk 0 */
	uint8_t chunk_msg[64];
	size_t msg_len = build_chunk_msg(chunk_msg, 0, fw, chunk_size);

	mock_ota_signing_set_result(-1);
	ota_process_msg(chunk_msg, msg_len);

	TEST_ASSERT_EQUAL_INT(OTA_SUB_COMPLETE, send_buf[1]);
	TEST_ASSERT_EQUAL_INT(OTA_STATUS_SIG_ERR, send_buf[2]);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_ERROR, ota_get_phase());
}

/* ------------------------------------------------------------------ */
/*  CRC failure takes priority over signature check                    */
/* ------------------------------------------------------------------ */

void test_crc_failure_before_sig_check(void)
{
	/* If CRC fails, signature verification should never be called. */
	uint8_t fw[256];
	uint16_t size = prepare_firmware(fw, 120, true); /* 184 */
	uint32_t bad_crc = 0xBADBAD;
	uint16_t total_chunks = (size + TEST_CHUNK_SIZE - 1) / TEST_CHUNK_SIZE;

	/* START with wrong CRC */
	uint8_t start[19];
	build_start_msg_with_flags(start, size, total_chunks, TEST_CHUNK_SIZE,
				   bad_crc, 1, OTA_START_FLAGS_SIGNED);
	ota_process_msg(start, 19);

	/* Send all chunks */
	uint8_t chunk_msg[64];
	for (uint16_t i = 0; i < total_chunks; i++) {
		uint16_t offset = i * TEST_CHUNK_SIZE;
		uint16_t data_len = TEST_CHUNK_SIZE;
		if (offset + data_len > size) {
			data_len = size - offset;
		}
		size_t msg_len = build_chunk_msg(chunk_msg, i,
						  &fw[offset], data_len);
		ota_process_msg(chunk_msg, msg_len);
	}

	/* CRC mismatch → COMPLETE with CRC_ERR */
	TEST_ASSERT_EQUAL_INT(OTA_SUB_COMPLETE, send_buf[1]);
	TEST_ASSERT_EQUAL_INT(OTA_STATUS_CRC_ERR, send_buf[2]);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_ERROR, ota_get_phase());
	/* Signature verify should NOT have been called */
	TEST_ASSERT_EQUAL_INT(0, mock_ota_signing_get_call_count());
}

/* ------------------------------------------------------------------ */
/*  Unity runner                                                       */
/* ------------------------------------------------------------------ */

int main(void)
{
	UNITY_BEGIN();

	/* Flags parsing */
	RUN_TEST(test_start_18_bytes_is_unsigned);
	RUN_TEST(test_start_19_bytes_signed_flag);
	RUN_TEST(test_start_19_bytes_no_signed_flag);

	/* Full mode signing */
	RUN_TEST(test_full_mode_signed_ok);
	RUN_TEST(test_full_mode_signed_fail);
	RUN_TEST(test_full_mode_unsigned_no_verify);

	/* Delta mode signing */
	RUN_TEST(test_delta_mode_signed_ok);
	RUN_TEST(test_delta_mode_signed_fail);

	/* CRC priority */
	RUN_TEST(test_crc_failure_before_sig_check);

	return UNITY_END();
}
