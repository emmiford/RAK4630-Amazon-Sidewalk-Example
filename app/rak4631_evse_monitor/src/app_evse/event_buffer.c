/*
 * Event Buffer — ring buffer of timestamped EVSE state snapshots
 *
 * Fixed-size circular buffer. Head points to the next write position.
 * When full, new entries overwrite the oldest. Trim removes entries
 * with timestamp <= the ACK watermark.
 */

#include <event_buffer.h>
#include <string.h>

static struct event_snapshot buf[EVENT_BUFFER_CAPACITY];
static uint8_t head;   /* next write index */
static uint8_t count;  /* valid entries (0..CAPACITY) */

void event_buffer_init(void)
{
	memset(buf, 0, sizeof(buf));
	head = 0;
	count = 0;
}

void event_buffer_add(const struct event_snapshot *snap)
{
	if (!snap) {
		return;
	}

	buf[head] = *snap;
	head = (head + 1) % EVENT_BUFFER_CAPACITY;

	if (count < EVENT_BUFFER_CAPACITY) {
		count++;
	}
	/* else: wrapped — oldest entry was overwritten */
}

bool event_buffer_get_latest(struct event_snapshot *out)
{
	if (!out || count == 0) {
		return false;
	}

	/* Latest entry is one before head */
	uint8_t idx = (head == 0) ? EVENT_BUFFER_CAPACITY - 1 : head - 1;
	*out = buf[idx];
	return true;
}

/**
 * Get the index of the oldest entry.
 * When count < CAPACITY, tail is 0. When full, tail == head.
 */
static uint8_t tail_index(void)
{
	if (count < EVENT_BUFFER_CAPACITY) {
		return 0;
	}
	return head; /* when full, head == tail (just got overwritten) */
}

void event_buffer_trim(uint32_t ack_watermark)
{
	if (count == 0) {
		return;
	}

	/*
	 * Walk from oldest to newest, removing entries with
	 * timestamp <= ack_watermark. Since timestamps are monotonically
	 * increasing (from time_sync), we can stop at the first entry
	 * that is newer than the watermark.
	 *
	 * We rebuild by counting how many to trim from the tail.
	 */
	uint8_t tail = tail_index();
	uint8_t trimmed = 0;

	for (uint8_t i = 0; i < count; i++) {
		uint8_t idx = (tail + i) % EVENT_BUFFER_CAPACITY;
		if (buf[idx].timestamp <= ack_watermark) {
			trimmed++;
		} else {
			break; /* timestamps are monotonic — done */
		}
	}

	if (trimmed == 0) {
		return;
	}

	if (trimmed >= count) {
		/* All entries trimmed */
		count = 0;
		head = 0;
		return;
	}

	/*
	 * Compact: shift remaining entries to the start of the buffer.
	 * This simplifies tail_index() and avoids fragmented state.
	 * When the source range wraps around the ring, in-place copy
	 * would overwrite unread entries — use a temp buffer in that case.
	 */
	uint8_t new_count = count - trimmed;
	uint8_t src = (tail + trimmed) % EVENT_BUFFER_CAPACITY;

	if (src + new_count <= EVENT_BUFFER_CAPACITY) {
		/* No wrap — safe to memmove */
		memmove(buf, &buf[src], new_count * sizeof(struct event_snapshot));
	} else {
		/* Source wraps — copy via temp to avoid overwrite */
		struct event_snapshot tmp[EVENT_BUFFER_CAPACITY];
		for (uint8_t i = 0; i < new_count; i++) {
			tmp[i] = buf[(src + i) % EVENT_BUFFER_CAPACITY];
		}
		memcpy(buf, tmp, new_count * sizeof(struct event_snapshot));
	}

	count = new_count;
	head = new_count;
}

bool event_buffer_peek_at(uint8_t index, struct event_snapshot *out)
{
	if (!out || index >= count) {
		return false;
	}

	uint8_t tail = tail_index();
	uint8_t idx = (tail + index) % EVENT_BUFFER_CAPACITY;
	*out = buf[idx];
	return true;
}

uint8_t event_buffer_count(void)
{
	return count;
}

uint32_t event_buffer_oldest_timestamp(void)
{
	if (count == 0) {
		return 0;
	}
	return buf[tail_index()].timestamp;
}

uint32_t event_buffer_newest_timestamp(void)
{
	if (count == 0) {
		return 0;
	}
	uint8_t idx = (head == 0) ? EVENT_BUFFER_CAPACITY - 1 : head - 1;
	return buf[idx].timestamp;
}
