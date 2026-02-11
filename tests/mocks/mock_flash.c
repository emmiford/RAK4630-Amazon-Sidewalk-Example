/*
 * Mock flash memory globals for OTA host-side tests.
 */

#include <stdint.h>
#include <string.h>

/* These must match the externs in zephyr/drivers/flash.h and zephyr/device.h */
#include <zephyr/device.h>

struct device mock_flash_device = { .name = "mock_flash" };

#define MOCK_FLASH_SIZE 0x65000

uint8_t mock_flash_mem[MOCK_FLASH_SIZE];
int mock_flash_read_count;
int mock_flash_write_count;
int mock_flash_erase_count;
int mock_flash_fail_at_page = -1;
int mock_reboot_count;

void mock_flash_reset(void)
{
	memset(mock_flash_mem, 0xFF, MOCK_FLASH_SIZE);
	mock_flash_read_count = 0;
	mock_flash_write_count = 0;
	mock_flash_erase_count = 0;
	mock_flash_fail_at_page = -1;
	mock_reboot_count = 0;
}
