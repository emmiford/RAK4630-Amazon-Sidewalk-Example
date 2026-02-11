/*
 * Host-side tests for OTA boot recovery path.
 *
 * Tests ota_boot_recovery_check() and ota_resume_apply() using mock
 * Zephyr flash operating on RAM buffers. Verifies that interrupted
 * OTA apply operations are detected and resumed correctly at boot.
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

/* Helper: write to simulated flash at absolute address */
static void flash_put(uint32_t addr, const void *data, size_t len)
{
	memcpy(&mock_flash_mem[addr - MOCK_FLASH_BASE], data, len);
}

/* Helper: read from simulated flash at absolute address */
static void flash_peek(uint32_t addr, void *buf, size_t len)
{
	memcpy(buf, &mock_flash_mem[addr - MOCK_FLASH_BASE], len);
}

/* Helper: write recovery metadata to mock flash */
static void write_test_metadata(uint8_t state, uint32_t image_size,
				uint32_t image_crc32, uint32_t app_version,
				uint32_t pages_copied, uint32_t total_pages)
{
	struct ota_metadata meta = {
		.magic = OTA_META_MAGIC,
		.state = state,
		.image_size = image_size,
		.image_crc32 = image_crc32,
		.app_version = app_version,
		.pages_copied = pages_copied,
		.total_pages = total_pages,
	};
	flash_put(OTA_METADATA_ADDR, &meta, sizeof(meta));
}

/* Helper: prepare a test image in staging with APP_CALLBACK_MAGIC at start */
static uint32_t prepare_staging_image(uint32_t size)
{
	/* Fill staging with pattern data */
	for (uint32_t i = 0; i < size; i++) {
		mock_flash_mem[OTA_STAGING_ADDR - MOCK_FLASH_BASE + i] = (uint8_t)(i & 0xFF);
	}

	/* Write APP_CALLBACK_MAGIC at start (required for post-apply verification) */
	uint32_t magic = APP_CALLBACK_MAGIC;
	flash_put(OTA_STAGING_ADDR, &magic, sizeof(magic));

	/* Compute CRC32 of the staging image */
	uint32_t crc = 0;
	crc = ~crc;
	for (uint32_t i = 0; i < size; i++) {
		uint8_t b = mock_flash_mem[OTA_STAGING_ADDR - MOCK_FLASH_BASE + i];
		crc ^= b;
		for (int j = 0; j < 8; j++) {
			crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
		}
	}
	crc = ~crc;
	return crc;
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
/*  Normal boot — no recovery needed                                   */
/* ------------------------------------------------------------------ */

static void test_normal_boot_no_metadata(void)
{
	/* Flash is all 0xFF — no valid metadata */
	bool recovered = ota_boot_recovery_check();
	TEST_ASSERT_FALSE(recovered);
	TEST_ASSERT_EQUAL_INT(0, mock_reboot_count);
}

static void test_normal_boot_bad_magic(void)
{
	/* Write metadata with wrong magic */
	struct ota_metadata meta = {
		.magic = 0xDEADBEEF,
		.state = OTA_META_STATE_APPLYING,
	};
	flash_put(OTA_METADATA_ADDR, &meta, sizeof(meta));

	bool recovered = ota_boot_recovery_check();
	TEST_ASSERT_FALSE(recovered);
	TEST_ASSERT_EQUAL_INT(0, mock_reboot_count);
}

static void test_staged_but_not_applying(void)
{
	/* STAGED state should be cleared, not trigger recovery */
	write_test_metadata(OTA_META_STATE_STAGED, 4096, 0x12345678, 5, 0, 1);

	bool recovered = ota_boot_recovery_check();
	TEST_ASSERT_FALSE(recovered);
	TEST_ASSERT_EQUAL_INT(0, mock_reboot_count);

	/* Verify metadata was cleared (erased) */
	struct ota_metadata meta;
	flash_peek(OTA_METADATA_ADDR, &meta, sizeof(meta));
	TEST_ASSERT_NOT_EQUAL(OTA_META_MAGIC, meta.magic);
}

static void test_none_state_normal_boot(void)
{
	write_test_metadata(OTA_META_STATE_NONE, 0, 0, 0, 0, 0);

	bool recovered = ota_boot_recovery_check();
	TEST_ASSERT_FALSE(recovered);
	TEST_ASSERT_EQUAL_INT(0, mock_reboot_count);
}

/* ------------------------------------------------------------------ */
/*  Recovery: interrupted apply detected and resumed                   */
/* ------------------------------------------------------------------ */

static void test_recovery_full_apply_from_scratch(void)
{
	uint32_t image_size = 4096;  /* 1 page */
	uint32_t crc = prepare_staging_image(image_size);

	/* Metadata says APPLYING, 0 pages copied */
	write_test_metadata(OTA_META_STATE_APPLYING, image_size, crc, 10, 0, 1);

	bool recovered = ota_boot_recovery_check();
	TEST_ASSERT_TRUE(recovered);
	TEST_ASSERT_EQUAL_INT(1, mock_reboot_count);

	/* Primary should now contain the staged image */
	uint32_t magic;
	flash_peek(OTA_APP_PRIMARY_ADDR, &magic, sizeof(magic));
	TEST_ASSERT_EQUAL_HEX32(APP_CALLBACK_MAGIC, magic);

	/* Metadata should be cleared */
	struct ota_metadata meta;
	flash_peek(OTA_METADATA_ADDR, &meta, sizeof(meta));
	TEST_ASSERT_NOT_EQUAL(OTA_META_MAGIC, meta.magic);
}

static void test_recovery_resumes_at_page_3_of_5(void)
{
	uint32_t image_size = 5 * OTA_FLASH_PAGE_SIZE;  /* 5 pages */
	uint32_t crc = prepare_staging_image(image_size);

	/* Save expected bytes before recovery (staging gets partially erased by clear_metadata) */
	uint8_t expected_bytes[5];
	for (uint32_t p = 0; p < 5; p++) {
		uint32_t offset = p * OTA_FLASH_PAGE_SIZE + 100;
		flash_peek(OTA_STAGING_ADDR + offset, &expected_bytes[p], 1);
	}

	/* Simulate: pages 0-2 already copied, interrupted at page 3 */
	for (uint32_t p = 0; p < 3; p++) {
		uint32_t src = OTA_STAGING_ADDR + p * OTA_FLASH_PAGE_SIZE;
		uint32_t dst = OTA_APP_PRIMARY_ADDR + p * OTA_FLASH_PAGE_SIZE;
		memcpy(&mock_flash_mem[dst - MOCK_FLASH_BASE],
		       &mock_flash_mem[src - MOCK_FLASH_BASE],
		       OTA_FLASH_PAGE_SIZE);
	}

	write_test_metadata(OTA_META_STATE_APPLYING, image_size, crc, 10, 3, 5);

	bool recovered = ota_boot_recovery_check();
	TEST_ASSERT_TRUE(recovered);
	TEST_ASSERT_EQUAL_INT(1, mock_reboot_count);

	/* Verify all 5 pages were copied to primary (compare to saved expected bytes,
	 * since staging gets partially erased by clear_metadata page alignment) */
	for (uint32_t p = 0; p < 5; p++) {
		uint8_t primary_byte;
		uint32_t offset = p * OTA_FLASH_PAGE_SIZE + 100;
		flash_peek(OTA_APP_PRIMARY_ADDR + offset, &primary_byte, 1);
		TEST_ASSERT_EQUAL_HEX8(expected_bytes[p], primary_byte);
	}
}

static void test_recovery_last_page_partial(void)
{
	/* Image smaller than a full page */
	uint32_t image_size = 2048;  /* Half a page */
	uint32_t crc = prepare_staging_image(image_size);

	/* Save expected byte before recovery (staging gets partially erased) */
	uint8_t expected;
	flash_peek(OTA_STAGING_ADDR + 100, &expected, 1);

	write_test_metadata(OTA_META_STATE_APPLYING, image_size, crc, 7, 0, 1);

	bool recovered = ota_boot_recovery_check();
	TEST_ASSERT_TRUE(recovered);
	TEST_ASSERT_EQUAL_INT(1, mock_reboot_count);

	/* Verify partial page was copied correctly */
	uint8_t actual;
	flash_peek(OTA_APP_PRIMARY_ADDR + 100, &actual, 1);
	TEST_ASSERT_EQUAL_HEX8(expected, actual);
}

static void test_recovery_already_complete(void)
{
	/* All pages already copied — metadata says 5/5 but still APPLYING */
	uint32_t image_size = 5 * OTA_FLASH_PAGE_SIZE;
	uint32_t crc = prepare_staging_image(image_size);

	/* Copy all pages to primary (simulating complete copy) */
	memcpy(&mock_flash_mem[OTA_APP_PRIMARY_ADDR - MOCK_FLASH_BASE],
	       &mock_flash_mem[OTA_STAGING_ADDR - MOCK_FLASH_BASE],
	       image_size);

	write_test_metadata(OTA_META_STATE_APPLYING, image_size, crc, 10, 5, 5);

	bool recovered = ota_boot_recovery_check();
	TEST_ASSERT_TRUE(recovered);
	TEST_ASSERT_EQUAL_INT(1, mock_reboot_count);
}

/* ------------------------------------------------------------------ */
/*  Magic verification after apply                                     */
/* ------------------------------------------------------------------ */

static void test_recovery_fails_bad_magic(void)
{
	/* Staging image without APP_CALLBACK_MAGIC at start */
	uint32_t image_size = 4096;
	for (uint32_t i = 0; i < image_size; i++) {
		mock_flash_mem[OTA_STAGING_ADDR - MOCK_FLASH_BASE + i] = (uint8_t)(i & 0xFF);
	}
	/* First 4 bytes are 0x00010203, not APP_CALLBACK_MAGIC */

	write_test_metadata(OTA_META_STATE_APPLYING, image_size, 0, 10, 0, 1);

	bool recovered = ota_boot_recovery_check();
	/* Recovery was attempted but magic check failed — still returns true
	 * because ota_resume_apply was called (even if it returned error) */
	TEST_ASSERT_TRUE(recovered);
	/* No reboot because magic verification failed */
	TEST_ASSERT_EQUAL_INT(0, mock_reboot_count);
}

/* ------------------------------------------------------------------ */
/*  OTA process message: START, CHUNK, ABORT                           */
/* ------------------------------------------------------------------ */

static void test_process_start_sets_receiving(void)
{
	/* Build a START message: cmd(1) sub(1) size(4) chunks(2) chunk_size(2) crc(4) version(4) */
	uint8_t msg[18];
	msg[0] = OTA_CMD_TYPE;
	msg[1] = OTA_SUB_START;
	/* total_size = 4096 */
	msg[2] = 0x00; msg[3] = 0x10; msg[4] = 0x00; msg[5] = 0x00;
	/* total_chunks = 1 */
	msg[6] = 0x01; msg[7] = 0x00;
	/* chunk_size = 15 (max for LoRa) */
	msg[8] = 0x0F; msg[9] = 0x00;
	/* crc32 = 0x12345678 */
	msg[10] = 0x78; msg[11] = 0x56; msg[12] = 0x34; msg[13] = 0x12;
	/* version = 5 */
	msg[14] = 0x05; msg[15] = 0x00; msg[16] = 0x00; msg[17] = 0x00;

	ota_process_msg(msg, sizeof(msg));

	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());
	/* Should have sent an ACK */
	TEST_ASSERT_EQUAL_INT(1, send_count);
	TEST_ASSERT_EQUAL_HEX8(OTA_CMD_TYPE, send_buf[0]);
	TEST_ASSERT_EQUAL_HEX8(OTA_SUB_ACK, send_buf[1]);
}

static void test_process_start_too_short(void)
{
	uint8_t msg[10] = { OTA_CMD_TYPE, OTA_SUB_START };
	ota_process_msg(msg, sizeof(msg));

	/* Should remain IDLE (start rejected) */
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_IDLE, ota_get_phase());
}

static void test_process_abort_resets_to_idle(void)
{
	/* First, get into RECEIVING state */
	uint8_t start[18] = { OTA_CMD_TYPE, OTA_SUB_START };
	start[2] = 0x00; start[3] = 0x10; /* size=4096 */
	start[6] = 0x01; /* chunks=1 */
	start[8] = 0x0F; /* chunk_size=15 */
	start[10] = 0x78; start[11] = 0x56; start[12] = 0x34; start[13] = 0x12;
	start[14] = 0x05;
	ota_process_msg(start, sizeof(start));
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_RECEIVING, ota_get_phase());

	/* Abort */
	uint8_t abort_msg[2] = { OTA_CMD_TYPE, OTA_SUB_ABORT };
	ota_process_msg(abort_msg, sizeof(abort_msg));
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_IDLE, ota_get_phase());
}

static void test_process_unknown_subtype_ignored(void)
{
	uint8_t msg[2] = { OTA_CMD_TYPE, 0xFF };
	ota_process_msg(msg, sizeof(msg));
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_IDLE, ota_get_phase());
}

static void test_process_msg_too_short(void)
{
	uint8_t msg[1] = { OTA_CMD_TYPE };
	ota_process_msg(msg, 1);
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_IDLE, ota_get_phase());
}

static void test_process_wrong_cmd_type(void)
{
	uint8_t msg[2] = { 0x99, OTA_SUB_START };
	ota_process_msg(msg, sizeof(msg));
	TEST_ASSERT_EQUAL_INT(OTA_PHASE_IDLE, ota_get_phase());
}

/* ------------------------------------------------------------------ */
/*  Phase string helper                                                */
/* ------------------------------------------------------------------ */

static void test_phase_str_idle(void)
{
	TEST_ASSERT_NOT_NULL(ota_phase_str(OTA_PHASE_IDLE));
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
	UNITY_BEGIN();

	/* Normal boot */
	RUN_TEST(test_normal_boot_no_metadata);
	RUN_TEST(test_normal_boot_bad_magic);
	RUN_TEST(test_staged_but_not_applying);
	RUN_TEST(test_none_state_normal_boot);

	/* Recovery path */
	RUN_TEST(test_recovery_full_apply_from_scratch);
	RUN_TEST(test_recovery_resumes_at_page_3_of_5);
	RUN_TEST(test_recovery_last_page_partial);
	RUN_TEST(test_recovery_already_complete);

	/* Magic verification */
	RUN_TEST(test_recovery_fails_bad_magic);

	/* Message processing */
	RUN_TEST(test_process_start_sets_receiving);
	RUN_TEST(test_process_start_too_short);
	RUN_TEST(test_process_abort_resets_to_idle);
	RUN_TEST(test_process_unknown_subtype_ignored);
	RUN_TEST(test_process_msg_too_short);
	RUN_TEST(test_process_wrong_cmd_type);

	/* Helpers */
	RUN_TEST(test_phase_str_idle);

	return UNITY_END();
}
