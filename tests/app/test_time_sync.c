/*
 * Unit tests for time_sync.c — TIME_SYNC downlink parsing and time tracking
 */

#include "unity.h"
#include "mock_platform_api.h"
#include "time_sync.h"

static const struct platform_api *api;

void setUp(void)
{
	api = mock_platform_api_init();
	time_sync_init();
	time_sync_set_api(api);
}

void tearDown(void) {}

/* --- Helper: build a valid 0x30 payload --- */
static void build_time_sync(uint8_t *buf, uint32_t epoch, uint32_t watermark)
{
	buf[0] = TIME_SYNC_CMD_TYPE;
	buf[1] = (uint8_t)(epoch & 0xFF);
	buf[2] = (uint8_t)((epoch >> 8) & 0xFF);
	buf[3] = (uint8_t)((epoch >> 16) & 0xFF);
	buf[4] = (uint8_t)((epoch >> 24) & 0xFF);
	buf[5] = (uint8_t)(watermark & 0xFF);
	buf[6] = (uint8_t)((watermark >> 8) & 0xFF);
	buf[7] = (uint8_t)((watermark >> 16) & 0xFF);
	buf[8] = (uint8_t)((watermark >> 24) & 0xFF);
}

/* --- Not synced before any TIME_SYNC --- */

void test_not_synced_initially(void)
{
	TEST_ASSERT_FALSE(time_sync_is_synced());
	TEST_ASSERT_EQUAL_UINT32(0, time_sync_get_epoch());
	TEST_ASSERT_EQUAL_UINT32(0, time_sync_get_ack_watermark());
	TEST_ASSERT_EQUAL_UINT32(0, time_sync_ms_since_sync());
}

/* --- Parse valid TIME_SYNC --- */

void test_parse_valid_time_sync(void)
{
	uint8_t buf[9];
	uint32_t epoch = 12345678;
	uint32_t wm = 12345600;

	mock_uptime_ms = 10000;
	build_time_sync(buf, epoch, wm);

	int ret = time_sync_process_cmd(buf, sizeof(buf));
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_TRUE(time_sync_is_synced());
	TEST_ASSERT_EQUAL_UINT32(epoch, time_sync_get_epoch());
	TEST_ASSERT_EQUAL_UINT32(wm, time_sync_get_ack_watermark());
}

/* --- Time advances with uptime --- */

void test_epoch_advances_with_uptime(void)
{
	uint8_t buf[9];
	uint32_t epoch = 1000;

	mock_uptime_ms = 5000;
	build_time_sync(buf, epoch, 0);
	time_sync_process_cmd(buf, sizeof(buf));

	/* 10 seconds later */
	mock_uptime_ms = 15000;
	TEST_ASSERT_EQUAL_UINT32(1010, time_sync_get_epoch());

	/* 60 seconds after sync */
	mock_uptime_ms = 65000;
	TEST_ASSERT_EQUAL_UINT32(1060, time_sync_get_epoch());
}

/* --- ms_since_sync tracks elapsed time --- */

void test_ms_since_sync(void)
{
	uint8_t buf[9];
	mock_uptime_ms = 1000;
	build_time_sync(buf, 500, 0);
	time_sync_process_cmd(buf, sizeof(buf));

	mock_uptime_ms = 6000;
	TEST_ASSERT_EQUAL_UINT32(5000, time_sync_ms_since_sync());
}

/* --- Second sync updates state --- */

void test_resync_updates_epoch(void)
{
	uint8_t buf[9];

	/* First sync */
	mock_uptime_ms = 1000;
	build_time_sync(buf, 100, 50);
	time_sync_process_cmd(buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT32(100, time_sync_get_epoch());

	/* 30 seconds later, cloud sends corrected time */
	mock_uptime_ms = 31000;
	build_time_sync(buf, 131, 120);
	time_sync_process_cmd(buf, sizeof(buf));

	/* Should use new sync point */
	TEST_ASSERT_EQUAL_UINT32(131, time_sync_get_epoch());
	TEST_ASSERT_EQUAL_UINT32(120, time_sync_get_ack_watermark());

	/* 5 seconds after resync */
	mock_uptime_ms = 36000;
	TEST_ASSERT_EQUAL_UINT32(136, time_sync_get_epoch());
}

/* --- Reject malformed payloads --- */

void test_reject_too_short(void)
{
	uint8_t buf[5] = {0x30, 1, 2, 3, 4};
	int ret = time_sync_process_cmd(buf, 5);
	TEST_ASSERT_LESS_THAN(0, ret);
	TEST_ASSERT_FALSE(time_sync_is_synced());
}

void test_reject_null_data(void)
{
	int ret = time_sync_process_cmd(NULL, 9);
	TEST_ASSERT_LESS_THAN(0, ret);
}

void test_reject_wrong_cmd_type(void)
{
	uint8_t buf[9] = {0x10, 0, 0, 0, 0, 0, 0, 0, 0};
	int ret = time_sync_process_cmd(buf, sizeof(buf));
	TEST_ASSERT_LESS_THAN(0, ret);
	TEST_ASSERT_FALSE(time_sync_is_synced());
}

/* --- Edge cases --- */

void test_epoch_zero_is_valid(void)
{
	/* Epoch 0 = 2026-01-01T00:00:00Z, valid but unlikely */
	uint8_t buf[9];
	mock_uptime_ms = 0;
	build_time_sync(buf, 0, 0);
	int ret = time_sync_process_cmd(buf, sizeof(buf));
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_TRUE(time_sync_is_synced());
	TEST_ASSERT_EQUAL_UINT32(0, time_sync_get_epoch());
}

void test_large_epoch_value(void)
{
	uint8_t buf[9];
	uint32_t epoch = 0xFFFFFFF0;  /* far future */
	mock_uptime_ms = 0;
	build_time_sync(buf, epoch, 0);
	time_sync_process_cmd(buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT32(epoch, time_sync_get_epoch());
}

void test_watermark_independent_of_epoch(void)
{
	uint8_t buf[9];
	build_time_sync(buf, 100, 99999);
	mock_uptime_ms = 0;
	time_sync_process_cmd(buf, sizeof(buf));
	TEST_ASSERT_EQUAL_UINT32(99999, time_sync_get_ack_watermark());
}

/* --- main --- */

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_not_synced_initially);
	RUN_TEST(test_parse_valid_time_sync);
	RUN_TEST(test_epoch_advances_with_uptime);
	RUN_TEST(test_ms_since_sync);
	RUN_TEST(test_resync_updates_epoch);
	RUN_TEST(test_reject_too_short);
	RUN_TEST(test_reject_null_data);
	RUN_TEST(test_reject_wrong_cmd_type);
	RUN_TEST(test_epoch_zero_is_valid);
	RUN_TEST(test_large_epoch_value);
	RUN_TEST(test_watermark_independent_of_epoch);

	return UNITY_END();
}
