/*
 * Unit tests for event_buffer module (TASK-034)
 *
 * Tests: insert, wrap, get_latest, trim by watermark, edge cases.
 */

#include "unity.h"
#include "event_buffer.h"
#include <string.h>

void setUp(void) { event_buffer_init(); }
void tearDown(void) {}

/* --- Helper --- */

static struct event_snapshot make_snap(uint32_t ts, uint8_t state)
{
	struct event_snapshot s = {0};
	s.timestamp = ts;
	s.j1772_state = state;
	s.pilot_voltage_mv = 2980;
	s.current_ma = 0;
	s.thermostat_flags = 0;
	s.charge_flags = EVENT_FLAG_CHARGE_ALLOWED;
	return s;
}

/* --- Tests --- */

void test_empty_buffer_count_is_zero(void)
{
	TEST_ASSERT_EQUAL_UINT8(0, event_buffer_count());
}

void test_empty_buffer_get_latest_returns_false(void)
{
	struct event_snapshot out;
	TEST_ASSERT_FALSE(event_buffer_get_latest(&out));
}

void test_empty_buffer_timestamps_are_zero(void)
{
	TEST_ASSERT_EQUAL_UINT32(0, event_buffer_oldest_timestamp());
	TEST_ASSERT_EQUAL_UINT32(0, event_buffer_newest_timestamp());
}

void test_add_one_entry(void)
{
	struct event_snapshot snap = make_snap(1000, 1);
	event_buffer_add(&snap);

	TEST_ASSERT_EQUAL_UINT8(1, event_buffer_count());
	TEST_ASSERT_EQUAL_UINT32(1000, event_buffer_oldest_timestamp());
	TEST_ASSERT_EQUAL_UINT32(1000, event_buffer_newest_timestamp());
}

void test_get_latest_returns_last_added(void)
{
	struct event_snapshot s1 = make_snap(100, 1);
	struct event_snapshot s2 = make_snap(200, 3);
	event_buffer_add(&s1);
	event_buffer_add(&s2);

	struct event_snapshot out;
	TEST_ASSERT_TRUE(event_buffer_get_latest(&out));
	TEST_ASSERT_EQUAL_UINT32(200, out.timestamp);
	TEST_ASSERT_EQUAL_UINT8(3, out.j1772_state);
}

void test_add_fills_to_capacity(void)
{
	for (uint32_t i = 0; i < EVENT_BUFFER_CAPACITY; i++) {
		struct event_snapshot s = make_snap(i + 1, 1);
		event_buffer_add(&s);
	}

	TEST_ASSERT_EQUAL_UINT8(EVENT_BUFFER_CAPACITY, event_buffer_count());
	TEST_ASSERT_EQUAL_UINT32(1, event_buffer_oldest_timestamp());
	TEST_ASSERT_EQUAL_UINT32(EVENT_BUFFER_CAPACITY, event_buffer_newest_timestamp());
}

void test_wrap_overwrites_oldest(void)
{
	/* Fill to capacity */
	for (uint32_t i = 0; i < EVENT_BUFFER_CAPACITY; i++) {
		struct event_snapshot s = make_snap(i + 1, 1);
		event_buffer_add(&s);
	}

	/* Add one more — should overwrite entry with timestamp=1 */
	struct event_snapshot extra = make_snap(EVENT_BUFFER_CAPACITY + 1, 2);
	event_buffer_add(&extra);

	TEST_ASSERT_EQUAL_UINT8(EVENT_BUFFER_CAPACITY, event_buffer_count());
	TEST_ASSERT_EQUAL_UINT32(2, event_buffer_oldest_timestamp());
	TEST_ASSERT_EQUAL_UINT32(EVENT_BUFFER_CAPACITY + 1, event_buffer_newest_timestamp());
}

void test_wrap_multiple_overwrites(void)
{
	/* Fill + 10 more overwrites */
	for (uint32_t i = 0; i < EVENT_BUFFER_CAPACITY + 10; i++) {
		struct event_snapshot s = make_snap(i + 1, 1);
		event_buffer_add(&s);
	}

	TEST_ASSERT_EQUAL_UINT8(EVENT_BUFFER_CAPACITY, event_buffer_count());
	TEST_ASSERT_EQUAL_UINT32(11, event_buffer_oldest_timestamp());
	TEST_ASSERT_EQUAL_UINT32(EVENT_BUFFER_CAPACITY + 10, event_buffer_newest_timestamp());
}

void test_trim_removes_old_entries(void)
{
	for (uint32_t i = 0; i < 10; i++) {
		struct event_snapshot s = make_snap((i + 1) * 100, 1);
		event_buffer_add(&s);
	}

	/* Trim everything at or before timestamp 500 */
	event_buffer_trim(500);

	TEST_ASSERT_EQUAL_UINT8(5, event_buffer_count());
	TEST_ASSERT_EQUAL_UINT32(600, event_buffer_oldest_timestamp());
	TEST_ASSERT_EQUAL_UINT32(1000, event_buffer_newest_timestamp());
}

void test_trim_all_entries(void)
{
	for (uint32_t i = 0; i < 5; i++) {
		struct event_snapshot s = make_snap(i + 1, 1);
		event_buffer_add(&s);
	}

	/* Watermark newer than all entries */
	event_buffer_trim(100);

	TEST_ASSERT_EQUAL_UINT8(0, event_buffer_count());
	TEST_ASSERT_EQUAL_UINT32(0, event_buffer_oldest_timestamp());
}

void test_trim_no_entries_when_watermark_older(void)
{
	for (uint32_t i = 0; i < 5; i++) {
		struct event_snapshot s = make_snap(100 + i, 1);
		event_buffer_add(&s);
	}

	/* Watermark older than all entries — nothing trimmed */
	event_buffer_trim(50);

	TEST_ASSERT_EQUAL_UINT8(5, event_buffer_count());
	TEST_ASSERT_EQUAL_UINT32(100, event_buffer_oldest_timestamp());
}

void test_trim_empty_buffer(void)
{
	/* Should not crash */
	event_buffer_trim(1000);
	TEST_ASSERT_EQUAL_UINT8(0, event_buffer_count());
}

void test_trim_exact_watermark(void)
{
	/* Entry exactly at watermark should be trimmed */
	struct event_snapshot s1 = make_snap(100, 1);
	struct event_snapshot s2 = make_snap(200, 2);
	event_buffer_add(&s1);
	event_buffer_add(&s2);

	event_buffer_trim(100);

	TEST_ASSERT_EQUAL_UINT8(1, event_buffer_count());
	TEST_ASSERT_EQUAL_UINT32(200, event_buffer_oldest_timestamp());
}

void test_trim_after_wrap(void)
{
	/* Fill buffer + 5 overwrites, then trim */
	for (uint32_t i = 0; i < EVENT_BUFFER_CAPACITY + 5; i++) {
		struct event_snapshot s = make_snap((i + 1) * 10, 1);
		event_buffer_add(&s);
	}

	/* Oldest should be entry #6 (timestamp 60), newest #55 (timestamp 550) */
	TEST_ASSERT_EQUAL_UINT32(60, event_buffer_oldest_timestamp());

	/* Trim up to 200 — removes entries 60..200 (15 entries: 60,70,...,200) */
	event_buffer_trim(200);

	TEST_ASSERT_EQUAL_UINT32(210, event_buffer_oldest_timestamp());
	TEST_ASSERT_EQUAL_UINT32(550, event_buffer_newest_timestamp());
	TEST_ASSERT_EQUAL_UINT8(35, event_buffer_count());
}

void test_add_after_trim(void)
{
	for (uint32_t i = 0; i < 5; i++) {
		struct event_snapshot s = make_snap(i + 1, 1);
		event_buffer_add(&s);
	}

	event_buffer_trim(3);
	TEST_ASSERT_EQUAL_UINT8(2, event_buffer_count());

	/* Add more after trim */
	struct event_snapshot s = make_snap(10, 2);
	event_buffer_add(&s);

	TEST_ASSERT_EQUAL_UINT8(3, event_buffer_count());
	TEST_ASSERT_EQUAL_UINT32(4, event_buffer_oldest_timestamp());
	TEST_ASSERT_EQUAL_UINT32(10, event_buffer_newest_timestamp());
}

void test_null_snap_ignored(void)
{
	event_buffer_add(NULL);
	TEST_ASSERT_EQUAL_UINT8(0, event_buffer_count());
}

void test_null_out_returns_false(void)
{
	struct event_snapshot s = make_snap(1, 1);
	event_buffer_add(&s);
	TEST_ASSERT_FALSE(event_buffer_get_latest(NULL));
}

void test_snapshot_fields_preserved(void)
{
	struct event_snapshot s = {
		.timestamp = 12345,
		.pilot_voltage_mv = 2234,
		.current_ma = 8500,
		.j1772_state = 3,
		.thermostat_flags = 0x03,
		.charge_flags = EVENT_FLAG_CHARGE_ALLOWED,
		._reserved = 0,
	};
	event_buffer_add(&s);

	struct event_snapshot out;
	TEST_ASSERT_TRUE(event_buffer_get_latest(&out));
	TEST_ASSERT_EQUAL_UINT32(12345, out.timestamp);
	TEST_ASSERT_EQUAL_UINT16(2234, out.pilot_voltage_mv);
	TEST_ASSERT_EQUAL_UINT16(8500, out.current_ma);
	TEST_ASSERT_EQUAL_UINT8(3, out.j1772_state);
	TEST_ASSERT_EQUAL_UINT8(0x03, out.thermostat_flags);
	TEST_ASSERT_EQUAL_UINT8(EVENT_FLAG_CHARGE_ALLOWED, out.charge_flags);
}

void test_reinit_clears_buffer(void)
{
	struct event_snapshot s = make_snap(100, 1);
	event_buffer_add(&s);
	TEST_ASSERT_EQUAL_UINT8(1, event_buffer_count());

	event_buffer_init();
	TEST_ASSERT_EQUAL_UINT8(0, event_buffer_count());
}

/* --- Runner --- */

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_empty_buffer_count_is_zero);
	RUN_TEST(test_empty_buffer_get_latest_returns_false);
	RUN_TEST(test_empty_buffer_timestamps_are_zero);
	RUN_TEST(test_add_one_entry);
	RUN_TEST(test_get_latest_returns_last_added);
	RUN_TEST(test_add_fills_to_capacity);
	RUN_TEST(test_wrap_overwrites_oldest);
	RUN_TEST(test_wrap_multiple_overwrites);
	RUN_TEST(test_trim_removes_old_entries);
	RUN_TEST(test_trim_all_entries);
	RUN_TEST(test_trim_no_entries_when_watermark_older);
	RUN_TEST(test_trim_empty_buffer);
	RUN_TEST(test_trim_exact_watermark);
	RUN_TEST(test_trim_after_wrap);
	RUN_TEST(test_add_after_trim);
	RUN_TEST(test_null_snap_ignored);
	RUN_TEST(test_null_out_returns_false);
	RUN_TEST(test_snapshot_fields_preserved);
	RUN_TEST(test_reinit_clears_buffer);
	return UNITY_END();
}
