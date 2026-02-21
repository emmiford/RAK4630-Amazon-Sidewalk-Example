/*
 * OTA Flash Abstraction — Low-level flash I/O for OTA updates
 *
 * Handles nRF52840 internal flash: init, erase, read, write (with
 * 4-byte alignment padding), and CRC32 computation over flash regions.
 */

#include <ota_flash.h>

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <string.h>

LOG_MODULE_REGISTER(ota_flash, CONFIG_SIDEWALK_LOG_LEVEL);

/* Flash device — nRF52840 internal flash */
static const struct device *flash_dev;

int ota_flash_init(void)
{
	if (flash_dev) {
		return 0;
	}
	flash_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));
	if (!device_is_ready(flash_dev)) {
		LOG_ERR("Flash device not ready");
		flash_dev = NULL;
		return -ENODEV;
	}
	return 0;
}

int ota_flash_erase_pages(uint32_t addr, size_t size)
{
	int err = ota_flash_init();
	if (err) {
		return err;
	}

	/* Align to page boundaries */
	uint32_t page_start = addr & ~(OTA_FLASH_PAGE_SIZE - 1);
	uint32_t end = addr + size;
	uint32_t erase_size = end - page_start;

	/* Round up to page size */
	erase_size = (erase_size + OTA_FLASH_PAGE_SIZE - 1) & ~(OTA_FLASH_PAGE_SIZE - 1);

	LOG_INF("OTA: erasing 0x%08x, %u bytes", page_start, erase_size);
	return flash_erase(flash_dev, page_start, erase_size);
}

int ota_flash_write(uint32_t addr, const uint8_t *data, size_t len)
{
	int err = ota_flash_init();
	if (err) {
		return err;
	}

	/* nRF52840 NVMC requires 4-byte aligned address AND length.
	 * Pad with 0xFF (erased flash value) on both sides. */
	uint32_t aligned_addr = addr & ~3u;
	uint32_t pre_pad = addr - aligned_addr;
	size_t aligned_len = (pre_pad + len + 3u) & ~3u;

	if (pre_pad == 0 && aligned_len == len) {
		return flash_write(flash_dev, addr, data, len);
	}

	uint8_t buf[24]; /* max: 3 pre-pad + 15 data + 2 post-pad */
	if (aligned_len > sizeof(buf)) {
		if (pre_pad != 0) {
			return -ENOMEM;
		}
		/* Large aligned-address write: split into aligned body + padded tail */
		size_t body = len & ~3u;
		int ret = 0;
		if (body > 0) {
			ret = flash_write(flash_dev, addr, data, body);
		}
		if (ret == 0 && body < aligned_len) {
			uint8_t tail[4];
			memset(tail, 0xFF, 4);
			memcpy(tail, data + body, len - body);
			ret = flash_write(flash_dev, addr + body, tail, 4);
		}
		return ret;
	}
	memset(buf, 0xFF, aligned_len);
	memcpy(buf + pre_pad, data, len);
	return flash_write(flash_dev, aligned_addr, buf, aligned_len);
}

int ota_flash_read(uint32_t addr, uint8_t *buf, size_t len)
{
	int err = ota_flash_init();
	if (err) {
		return err;
	}
	return flash_read(flash_dev, addr, buf, len);
}

uint32_t ota_flash_compute_crc32(uint32_t addr, size_t size)
{
	uint32_t crc = 0;
	uint8_t buf[256];
	size_t remaining = size;
	uint32_t offset = 0;

	while (remaining > 0) {
		size_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
		int err = ota_flash_read(addr + offset, buf, chunk);
		if (err) {
			LOG_ERR("OTA: flash read for CRC failed at 0x%08x: %d",
				addr + offset, err);
			return 0;
		}
		crc = crc32_ieee_update(crc, buf, chunk);
		offset += chunk;
		remaining -= chunk;
	}

	return crc;
}
