/*
 * Host-side tests for OTA chunk receive and delta bitmap paths.
 *
 * Tests handle_ota_chunk() — data accumulation into staging flash,
 * duplicate detection, phase/bounds checks — and the delta bitmap
 * set/get logic used in delta OTA mode. Uses the same mock flash
 * infrastructure as test_ota_recovery.c.
 */

#include "unity.h"
#include <ota_update.h>
#include <platform_api.h>
#include <string.h>

/* Mock flash externs from mock Zephyr headers */
extern uint8_t mock_flash_mem[];
extern int mock_flash_read_count;
extern int mock_flash_write_count;
extern int mock_flash_erase_count;
extern int mock_flash_fail_at_page;
extern int mock_reboot_count;
extern void mock_flash_reset(void);

#define MOCK_FLASH_BASE 0x90000

/* Helper: read from simulated flash at absolute address */
static void flash_peek(uint32_t addr, void *buf, size_t len)
{
	memcpy(buf, &mock_flash_mem[addr - MOCK_FLASH_BASE], len);
}

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
/*  Helpers to build OTA messages                                      */
/* ------------------------------------------------------------------ */

/* Build a START message for full (non-delta) mode.
 *
 * total_chunks == full_image_chunks ensures legacy (non-delta) mode.
 * Sets chunk_size to 15 (matching LoRa MTU - 4B header). */
static void build_start_msg(uint8_t *msg, uint32_t total_size,
			     uint16_t total_chunks, uint16_t chunk_size,
			     uint32_t crc32, uint32_t version)
{
	msg[0] = OTA_CMD_TYPE;
	msg[1] = OTA_SUB_START;
	/* total_size LE */
	msg[2]  = (uint8_t)(total_size);
	msg[3]  = (uint8_t)(total_size >> 8);
	msg[4]  = (uint8_t)(total_size >> 16);
	msg[5]  = (uint8_t)(total_size >> 24);
	/* total_chunks LE */
	msg[6]  = (uint8_t)(total_chunks);
	msg[7]  = (uint8_t)(total_chunks >> 8);
	/* chunk_size LE */
	msg[8]  = (uint8_t)(chunk_size);
	msg[9]  = (uint8_t)(chunk_size >> 8);
	/* crc32 LE */
	msg[10] = (uint8_t)(crc32);
	msg[11] = (uint8_t)(crc32 >> 8);
	msg[12] = (uint8_t)(crc32 >> 16);
	msg[13] = (uint8_t)(crc32 >> 24);
	/* version LE */
	msg[14] = (uint8_t)(version);
	msg[15] = (uint8_t)(version >> 8);
	msg[16] = (uint8_t)(version >> 16);
	msg[17] = (uint8_t)(version >> 24);
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

/* Enter RECEIVING state via a START message (full mode) */
static void enter_receiving_full(uint32_t total_size, uint16_t chunk_size)
{
	uint16_t total_chunks = (total_size + chunk_size - 1) / chunk_size;
	uint8_t start[18];
	build_start_msg(start, total_size, total_chunks, chunk_size,
			0x12345678, /* dummy CRC — tests won't reach validation */
			1);         /* version */
	ota_process_msg(start, sizeof(start));
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());
}

/* Enter RECEIVING state via a START message (delta mode).
 * delta_count < full_chunks triggers delta mode. */
static void enter_receiving_delta(uint32_t total_size, uint16_t chunk_size,
				  uint16_t delta_count)
{
	uint8_t start[18];
	build_start_msg(start, total_size, delta_count, chunk_size,
			0x12345678, 1);
	ota_process_msg(start, sizeof(start));
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());
}

/* ------------------------------------------------------------------ */
/*  setUp / tearDown                                                    */
/* ------------------------------------------------------------------ */

void setUp(void)
{
	mock_flash_reset();
	send_count = 0;
	send_len = 0;
	memset(send_buf, 0, sizeof(send_buf));
	ota_init(mock_send);
}

void tearDown(void) { }

/* ------------------------------------------------------------------ */
/*  Test 1: CHUNK writes correct data to staging flash at right offset */
/* ------------------------------------------------------------------ */

static void test_chunk_writes_correct_data(void)
{
	uint16_t chunk_size = 15;
	uint32_t total_size = 45; /* 3 chunks of 15 */
	enter_receiving_full(total_size, chunk_size);
	send_count = 0; /* reset after START's ACK */

	/* Send chunk 0 with pattern data */
	uint8_t pattern[15];
	for (int i = 0; i < 15; i++) {
		pattern[i] = (uint8_t)(0xA0 + i);
	}

	uint8_t msg[19];
	size_t msg_len = build_chunk_msg(msg, 0, pattern, 15);
	ota_process_msg(msg, msg_len);

	/* Verify data was written to staging at offset 0 */
	uint8_t readback[15];
	flash_peek(OTA_STAGING_ADDR, readback, 15);
	TEST_ASSERT_EQUAL_MEMORY(pattern, readback, 15);

	/* Verify ACK was sent */
	TEST_ASSERT_EQUAL_INT(1, send_count);
	TEST_ASSERT_EQUAL_HEX8(OTA_CMD_TYPE, send_buf[0]);
	TEST_ASSERT_EQUAL_HEX8(OTA_SUB_ACK, send_buf[1]);
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);
}

static void test_chunk_1_writes_at_correct_offset(void)
{
	uint16_t chunk_size = 15;
	uint32_t total_size = 45; /* 3 chunks */
	enter_receiving_full(total_size, chunk_size);
	send_count = 0;

	/* Send chunk 0 first (sequential mode requires it) */
	uint8_t data0[15];
	memset(data0, 0xAA, 15);
	uint8_t msg0[19];
	size_t len0 = build_chunk_msg(msg0, 0, data0, 15);
	ota_process_msg(msg0, len0);

	/* Send chunk 1 */
	uint8_t data1[15];
	memset(data1, 0xBB, 15);
	uint8_t msg1[19];
	size_t len1 = build_chunk_msg(msg1, 1, data1, 15);
	ota_process_msg(msg1, len1);

	/* Verify chunk 1 data at staging + 15 */
	uint8_t readback[15];
	flash_peek(OTA_STAGING_ADDR + 15, readback, 15);
	TEST_ASSERT_EQUAL_MEMORY(data1, readback, 15);
}

/* ------------------------------------------------------------------ */
/*  Test 2: CHUNK with wrong phase is rejected                         */
/* ------------------------------------------------------------------ */

static void test_chunk_rejected_when_idle(void)
{
	/* Don't send START — phase is IDLE */
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_IDLE, ota_get_phase());

	uint8_t data[1] = { 0xFF };
	uint8_t msg[5];
	size_t msg_len = build_chunk_msg(msg, 0, data, 1);
	ota_process_msg(msg, msg_len);

	/* Should send NO_SESSION error */
	TEST_ASSERT_EQUAL_INT(1, send_count);
	TEST_ASSERT_EQUAL_HEX8(OTA_SUB_ACK, send_buf[1]);
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_NO_SESSION, send_buf[2]);

	/* Phase should remain IDLE */
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_IDLE, ota_get_phase());
}

/* ------------------------------------------------------------------ */
/*  Test 3: Duplicate chunk is handled gracefully                      */
/* ------------------------------------------------------------------ */

static void test_duplicate_chunk_acked_ok(void)
{
	uint16_t chunk_size = 15;
	uint32_t total_size = 45; /* 3 chunks */
	enter_receiving_full(total_size, chunk_size);
	send_count = 0;

	/* Send chunk 0 */
	uint8_t data[15];
	memset(data, 0xCC, 15);
	uint8_t msg[19];
	size_t msg_len = build_chunk_msg(msg, 0, data, 15);
	ota_process_msg(msg, msg_len);
	TEST_ASSERT_EQUAL_INT(1, send_count);

	/* Send chunk 0 again (duplicate) */
	ota_process_msg(msg, msg_len);
	TEST_ASSERT_EQUAL_INT(2, send_count);

	/* Duplicate should still get OK status */
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);

	/* Phase should still be RECEIVING (not advanced) */
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());
}

/* ------------------------------------------------------------------ */
/*  Test 4: Delta bitmap set/get works for various indices             */
/* ------------------------------------------------------------------ */

static void test_delta_bitmap_chunk_0(void)
{
	/* Use delta mode: total_size=150, chunk_size=15, 10 full chunks.
	 * Send delta_count=3 (< 10) to trigger delta mode. */
	enter_receiving_delta(150, 15, 3);
	send_count = 0;

	/* Send delta chunk at absolute index 0 */
	uint8_t data[15];
	memset(data, 0xDD, 15);
	uint8_t msg[19];
	size_t msg_len = build_chunk_msg(msg, 0, data, 15);
	ota_process_msg(msg, msg_len);

	/* Should ACK successfully */
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);

	/* Verify the data was written to staging at chunk 0 offset */
	uint8_t readback[15];
	flash_peek(OTA_STAGING_ADDR, readback, 15);
	TEST_ASSERT_EQUAL_MEMORY(data, readback, 15);
}

static void test_delta_bitmap_chunk_127(void)
{
	/* total_size large enough for 128+ chunks: 128 * 15 = 1920 bytes.
	 * Use delta_count=2 so that receiving 1 chunk does NOT trigger
	 * validation (which would overwrite send_buf with a COMPLETE msg). */
	uint32_t total_size = 1920;
	uint16_t chunk_size = 15;
	/* delta_count=2, full_image_chunks=128 */
	enter_receiving_delta(total_size, chunk_size, 2);
	send_count = 0;

	/* Send delta chunk at absolute index 127 (last valid 0-based) */
	uint8_t data[15];
	memset(data, 0xEE, 15);
	uint8_t msg[19];
	size_t msg_len = build_chunk_msg(msg, 127, data, 15);
	ota_process_msg(msg, msg_len);

	/* Should ACK successfully */
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);

	/* Verify data at staging + 127*15 */
	uint8_t readback[15];
	flash_peek(OTA_STAGING_ADDR + (uint32_t)127 * 15, readback, 15);
	TEST_ASSERT_EQUAL_MEMORY(data, readback, 15);
}

static void test_delta_bitmap_edge_indices(void)
{
	/* Test indices at byte boundaries: 7 (last bit of byte 0) and
	 * 8 (first bit of byte 1) */
	uint32_t total_size = 300; /* 20 chunks at 15 bytes */
	uint16_t chunk_size = 15;
	enter_receiving_delta(total_size, chunk_size, 3);
	send_count = 0;

	uint8_t data7[15], data8[15];
	memset(data7, 0x77, 15);
	memset(data8, 0x88, 15);

	/* Send chunk 7 */
	uint8_t msg7[19];
	size_t len7 = build_chunk_msg(msg7, 7, data7, 15);
	ota_process_msg(msg7, len7);
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);

	/* Send chunk 8 */
	uint8_t msg8[19];
	size_t len8 = build_chunk_msg(msg8, 8, data8, 15);
	ota_process_msg(msg8, len8);
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);

	/* Verify both chunks in flash */
	uint8_t rb7[15], rb8[15];
	flash_peek(OTA_STAGING_ADDR + 7 * 15, rb7, 15);
	flash_peek(OTA_STAGING_ADDR + 8 * 15, rb8, 15);
	TEST_ASSERT_EQUAL_MEMORY(data7, rb7, 15);
	TEST_ASSERT_EQUAL_MEMORY(data8, rb8, 15);
}

static void test_delta_duplicate_chunk_handled(void)
{
	enter_receiving_delta(150, 15, 3);
	send_count = 0;

	uint8_t data[15];
	memset(data, 0xAB, 15);
	uint8_t msg[19];
	size_t msg_len = build_chunk_msg(msg, 5, data, 15);

	/* Send chunk 5 first time */
	ota_process_msg(msg, msg_len);
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);
	TEST_ASSERT_EQUAL_INT(1, send_count);

	/* Send chunk 5 again (duplicate) */
	ota_process_msg(msg, msg_len);
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);
	TEST_ASSERT_EQUAL_INT(2, send_count);
}

/* ------------------------------------------------------------------ */
/*  Test 5: All chunks received triggers VALIDATING state              */
/* ------------------------------------------------------------------ */

static void test_all_chunks_transitions_to_validating(void)
{
	/* Use chunk_size=12 (multiple of 4) to avoid mock flash alignment
	 * issues. ota_flash_write pads unaligned writes with 0xFF which
	 * overwrites adjacent data in the RAM-backed mock (real NOR flash
	 * ignores 0xFF writes). 2 chunks of 12 = 24 bytes total. */
	uint16_t chunk_size = 12;
	uint32_t total_size = 24;

	uint8_t chunk0[12], chunk1[12];
	memset(chunk0, 0x11, 12);
	memset(chunk1, 0x22, 12);

	/* Compute expected CRC32 for the combined image */
	uint8_t full_image[24];
	memcpy(full_image, chunk0, 12);
	memcpy(full_image + 12, chunk1, 12);

	/* Manual CRC32 (IEEE 802.3) */
	uint32_t crc = 0;
	crc = ~crc;
	for (uint32_t i = 0; i < 24; i++) {
		crc ^= full_image[i];
		for (int j = 0; j < 8; j++) {
			crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
		}
	}
	crc = ~crc;

	/* Send START with correct CRC */
	uint8_t start[18];
	build_start_msg(start, total_size, 2, chunk_size, crc, 1);
	ota_process_msg(start, sizeof(start));
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());
	send_count = 0;

	/* Send chunk 0 */
	uint8_t msg0[16]; /* 4 header + 12 data */
	size_t len0 = build_chunk_msg(msg0, 0, chunk0, 12);
	ota_process_msg(msg0, len0);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());

	/* Send chunk 1 (last) — should trigger validation */
	uint8_t msg1[16];
	size_t len1 = build_chunk_msg(msg1, 1, chunk1, 12);
	ota_process_msg(msg1, len1);

	/* After successful CRC, phase transitions to COMPLETE (via deferred apply) */
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_COMPLETE, ota_get_phase());

	/* COMPLETE message should have been sent */
	TEST_ASSERT_EQUAL_HEX8(OTA_SUB_COMPLETE, send_buf[1]);
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);
}

static void test_all_delta_chunks_transitions_to_validating(void)
{
	/* Delta mode: total_size=150 (10 full chunks), only 1 delta chunk.
	 * We need the merged CRC to match. For simplicity, send 1 delta
	 * chunk and let the rest come from primary (currently 0xFF). */
	uint16_t chunk_size = 15;
	uint32_t total_size = 150;
	uint16_t delta_count = 1;

	/* The merged image = primary baseline (0xFF) for chunks 0-8,
	 * staging data for chunk 9 (whatever we send).
	 * We need to compute the merged CRC. */
	uint8_t merged[150];

	/* Primary is 0xFF after flash reset */
	memset(merged, 0xFF, 150);

	/* We will send chunk index 9 with pattern data */
	uint8_t delta_data[15];
	memset(delta_data, 0x42, 15);
	memcpy(merged + 9 * 15, delta_data, 15);

	/* Compute expected CRC32 of merged image */
	uint32_t crc = 0;
	crc = ~crc;
	for (uint32_t i = 0; i < 150; i++) {
		crc ^= merged[i];
		for (int j = 0; j < 8; j++) {
			crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
		}
	}
	crc = ~crc;

	/* Also need primary to contain 0xFF (it does from flash_reset) */

	/* Send START with delta_count < full_chunks to enter delta mode */
	uint8_t start[18];
	build_start_msg(start, total_size, delta_count, chunk_size, crc, 1);
	ota_process_msg(start, sizeof(start));
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());
	send_count = 0;

	/* Send the single delta chunk at index 9 */
	uint8_t msg[19];
	size_t msg_len = build_chunk_msg(msg, 9, delta_data, 15);
	ota_process_msg(msg, msg_len);

	/* 1 of 1 delta chunks received → should validate + transition to COMPLETE */
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_COMPLETE, ota_get_phase());
	TEST_ASSERT_EQUAL_HEX8(OTA_SUB_COMPLETE, send_buf[1]);
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_OK, send_buf[2]);
}

/* ------------------------------------------------------------------ */
/*  Test 6: Chunk index out of bounds is rejected                      */
/* ------------------------------------------------------------------ */

static void test_chunk_out_of_order_rejected(void)
{
	/* In legacy mode, chunks must be sequential.
	 * Sending chunk 5 when 0 chunks received should be rejected. */
	enter_receiving_full(150, 15);
	send_count = 0;

	uint8_t data[15];
	memset(data, 0xFF, 15);
	uint8_t msg[19];
	size_t msg_len = build_chunk_msg(msg, 5, data, 15);
	ota_process_msg(msg, msg_len);

	/* Should get CRC_ERR status (out-of-order rejection) */
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_CRC_ERR, send_buf[2]);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());
}

static void test_delta_chunk_beyond_image_rejected(void)
{
	/* Delta mode: total_size=150, chunk_size=15 → 10 full_image_chunks.
	 * Chunk index 10 is out of bounds. */
	enter_receiving_delta(150, 15, 3);
	send_count = 0;

	uint8_t data[15];
	memset(data, 0xFF, 15);
	uint8_t msg[19];
	size_t msg_len = build_chunk_msg(msg, 10, data, 15);
	ota_process_msg(msg, msg_len);

	/* Should get SIZE_ERR */
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_SIZE_ERR, send_buf[2]);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());
}

static void test_chunk_payload_too_short_rejected(void)
{
	enter_receiving_full(150, 15);
	send_count = 0;

	/* Message with only 4 bytes (header, no data) → len < 5 */
	uint8_t msg[4] = { OTA_CMD_TYPE, OTA_SUB_CHUNK, 0x00, 0x00 };
	ota_process_msg(msg, 4);

	/* Should get SIZE_ERR */
	TEST_ASSERT_EQUAL_HEX8(OTA_STATUS_SIZE_ERR, send_buf[2]);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
	UNITY_BEGIN();

	/* Chunk write correctness */
	RUN_TEST(test_chunk_writes_correct_data);
	RUN_TEST(test_chunk_1_writes_at_correct_offset);

	/* Wrong phase rejection */
	RUN_TEST(test_chunk_rejected_when_idle);

	/* Duplicate handling */
	RUN_TEST(test_duplicate_chunk_acked_ok);

	/* Delta bitmap */
	RUN_TEST(test_delta_bitmap_chunk_0);
	RUN_TEST(test_delta_bitmap_chunk_127);
	RUN_TEST(test_delta_bitmap_edge_indices);
	RUN_TEST(test_delta_duplicate_chunk_handled);

	/* All chunks → state transition */
	RUN_TEST(test_all_chunks_transitions_to_validating);
	RUN_TEST(test_all_delta_chunks_transitions_to_validating);

	/* Out-of-bounds rejection */
	RUN_TEST(test_chunk_out_of_order_rejected);
	RUN_TEST(test_delta_chunk_beyond_image_rejected);
	RUN_TEST(test_chunk_payload_too_short_rejected);

	return UNITY_END();
}
