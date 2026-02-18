/*
 * OTA Flash Abstraction — Low-level flash I/O for OTA updates
 *
 * Provides init, erase, read, write (with nRF52840 alignment padding),
 * and CRC32 computation over flash regions. Separated from the OTA
 * protocol state machine so flash changes don't risk breaking protocol
 * logic and vice versa.
 */

#ifndef OTA_FLASH_H
#define OTA_FLASH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* nRF52840 flash page size */
#define OTA_FLASH_PAGE_SIZE     4096

/**
 * Initialize the flash device. Safe to call multiple times (no-op after first).
 * @return 0 on success, negative errno on failure
 */
int ota_flash_init(void);

/**
 * Erase flash pages covering the given address range.
 * Address and size are aligned to page boundaries automatically.
 * @param addr  Start address
 * @param size  Number of bytes to erase (rounded up to page boundary)
 * @return 0 on success, negative errno on failure
 */
int ota_flash_erase_pages(uint32_t addr, size_t size);

/**
 * Write data to flash with nRF52840 NVMC 4-byte alignment handling.
 * Pads with 0xFF on both sides if address or length is not 4-byte aligned.
 * @param addr  Flash address to write to
 * @param data  Data buffer to write
 * @param len   Number of bytes to write
 * @return 0 on success, negative errno on failure
 */
int ota_flash_write(uint32_t addr, const uint8_t *data, size_t len);

/**
 * Read data from flash.
 * @param addr  Flash address to read from
 * @param buf   Buffer to read into
 * @param len   Number of bytes to read
 * @return 0 on success, negative errno on failure
 */
int ota_flash_read(uint32_t addr, uint8_t *buf, size_t len);

/**
 * Compute CRC32 (IEEE 802.3) over a flash region.
 * Reads in 256-byte chunks to limit stack usage.
 * @param addr  Start address in flash
 * @param size  Number of bytes to checksum
 * @return CRC32 value, or 0 on read failure
 */
uint32_t compute_flash_crc32(uint32_t addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* OTA_FLASH_H */
