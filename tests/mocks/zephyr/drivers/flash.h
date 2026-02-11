/* Mock Zephyr flash driver for host-side OTA tests.
 *
 * Implements flash_read/write/erase using RAM buffers that simulate
 * the nRF52840 flash regions relevant to OTA:
 *   - App primary:  0x90000 (256KB)
 *   - OTA metadata: 0xCFF00 (256B)
 *   - OTA staging:  0xD0000 (~148KB)
 */
#ifndef ZEPHYR_FLASH_H_MOCK
#define ZEPHYR_FLASH_H_MOCK

#include <zephyr/device.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Simulated flash: covers 0x90000 to 0xF5000 (400KB) */
#define MOCK_FLASH_BASE    0x90000
#define MOCK_FLASH_SIZE    0x65000  /* 400KB — covers primary + meta + staging */

extern uint8_t mock_flash_mem[MOCK_FLASH_SIZE];
extern int mock_flash_read_count;
extern int mock_flash_write_count;
extern int mock_flash_erase_count;
extern int mock_flash_fail_at_page;  /* -1 = no failure; >= 0 = fail at this page erase */
extern int mock_reboot_count;

/* Initialize mock flash to all 0xFF (erased state) */
void mock_flash_reset(void);

static inline int flash_read(const struct device *dev, uint32_t addr,
			     void *buf, size_t len)
{
	(void)dev;
	mock_flash_read_count++;
	if (addr < MOCK_FLASH_BASE || addr + len > MOCK_FLASH_BASE + MOCK_FLASH_SIZE) {
		return -1;
	}
	memcpy(buf, &mock_flash_mem[addr - MOCK_FLASH_BASE], len);
	return 0;
}

static inline int flash_write(const struct device *dev, uint32_t addr,
			      const void *data, size_t len)
{
	(void)dev;
	mock_flash_write_count++;
	if (addr < MOCK_FLASH_BASE || addr + len > MOCK_FLASH_BASE + MOCK_FLASH_SIZE) {
		return -1;
	}
	memcpy(&mock_flash_mem[addr - MOCK_FLASH_BASE], data, len);
	return 0;
}

static inline int flash_erase(const struct device *dev, uint32_t addr, size_t size)
{
	(void)dev;
	mock_flash_erase_count++;
	if (mock_flash_fail_at_page >= 0 &&
	    mock_flash_erase_count > mock_flash_fail_at_page) {
		return -5;  /* Simulated flash error */
	}
	if (addr < MOCK_FLASH_BASE || addr + size > MOCK_FLASH_BASE + MOCK_FLASH_SIZE) {
		return -1;
	}
	memset(&mock_flash_mem[addr - MOCK_FLASH_BASE], 0xFF, size);
	return 0;
}

#endif /* ZEPHYR_FLASH_H_MOCK */
