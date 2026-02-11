/* Mock Zephyr CRC — provides a real CRC32 implementation for test validation */
#ifndef ZEPHYR_CRC_H_MOCK
#define ZEPHYR_CRC_H_MOCK

#include <stdint.h>
#include <stddef.h>

/* Standard CRC32 (IEEE 802.3) — same algorithm as Zephyr's crc32_ieee_update */
static inline uint32_t crc32_ieee_update(uint32_t crc, const uint8_t *data, size_t len)
{
	crc = ~crc;
	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
		}
	}
	return ~crc;
}

#endif /* ZEPHYR_CRC_H_MOCK */
