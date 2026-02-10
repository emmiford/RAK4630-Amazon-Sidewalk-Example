/*
 * OTA Update Module — App-Only OTA over Sidewalk
 *
 * State machine: IDLE → RECEIVING → VALIDATING → APPLYING → reboot
 *
 * Receives firmware chunks via Sidewalk downlinks, writes them to a
 * staging area in flash, validates the full CRC32, then copies to the
 * app primary partition. Recovery metadata survives power loss during
 * the apply phase.
 */

#include <ota_update.h>
#include <platform_api.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>
#include <string.h>

LOG_MODULE_REGISTER(ota_update, CONFIG_SIDEWALK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/*  Internal state                                                      */
/* ------------------------------------------------------------------ */

static struct {
	enum ota_phase phase;
	uint32_t total_size;
	uint16_t total_chunks;
	uint16_t chunk_size;
	uint32_t expected_crc32;
	uint32_t app_version;
	uint16_t chunks_received;
	uint32_t bytes_written;
	/* Windowed blast mode (window_size > 0 enables it) */
	uint8_t  window_size;          /* 0 = legacy per-chunk ACK mode */
	uint16_t window_start;         /* first chunk idx of current window */
	uint8_t  window_received[33];  /* bitfield: up to 260 chunks per window */
	/* Delta mode: total_chunks < full image chunks */
	bool     delta_mode;
	uint16_t full_image_chunks;    /* chunks in the full image */
	uint8_t  delta_received[128];  /* bitfield: up to 1024 chunks (~15KB) */
} ota_state;

static int (*ota_send_msg)(const uint8_t *data, size_t len);
static void (*ota_pre_apply_hook)(void);

/* Flash device — nRF52840 internal flash */
static const struct device *flash_dev;

/* ------------------------------------------------------------------ */
/*  Flash helpers                                                       */
/* ------------------------------------------------------------------ */

static int ota_flash_init(void)
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

static int ota_flash_erase_pages(uint32_t addr, size_t size)
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

static int ota_flash_write(uint32_t addr, const uint8_t *data, size_t len)
{
	int err = ota_flash_init();
	if (err) {
		return err;
	}

	/* nRF52840 NVMC requires 4-byte aligned writes.
	 * Pad short writes with 0xFF (erased flash value). */
	size_t aligned_len = (len + 3u) & ~3u;
	if (aligned_len == len) {
		return flash_write(flash_dev, addr, data, len);
	}

	uint8_t buf[20]; /* max chunk is 15B → padded to 16B */
	if (aligned_len > sizeof(buf)) {
		return -ENOMEM;
	}
	memcpy(buf, data, len);
	memset(buf + len, 0xFF, aligned_len - len);
	return flash_write(flash_dev, addr, buf, aligned_len);
}

static int ota_flash_read(uint32_t addr, uint8_t *buf, size_t len)
{
	int err = ota_flash_init();
	if (err) {
		return err;
	}
	return flash_read(flash_dev, addr, buf, len);
}

/* ------------------------------------------------------------------ */
/*  Uplink message builders                                             */
/* ------------------------------------------------------------------ */

static void send_ack(uint8_t status, uint16_t next_chunk, uint16_t chunks_received)
{
	if (!ota_send_msg) {
		return;
	}

	uint8_t buf[7];
	buf[0] = OTA_CMD_TYPE;
	buf[1] = OTA_SUB_ACK;
	buf[2] = status;
	buf[3] = (uint8_t)(next_chunk & 0xFF);
	buf[4] = (uint8_t)(next_chunk >> 8);
	buf[5] = (uint8_t)(chunks_received & 0xFF);
	buf[6] = (uint8_t)(chunks_received >> 8);

	ota_send_msg(buf, sizeof(buf));
}

static void send_complete(uint8_t result, uint32_t crc32_calc)
{
	if (!ota_send_msg) {
		return;
	}

	uint8_t buf[7];
	buf[0] = OTA_CMD_TYPE;
	buf[1] = OTA_SUB_COMPLETE;
	buf[2] = result;
	buf[3] = (uint8_t)(crc32_calc & 0xFF);
	buf[4] = (uint8_t)((crc32_calc >> 8) & 0xFF);
	buf[5] = (uint8_t)((crc32_calc >> 16) & 0xFF);
	buf[6] = (uint8_t)((crc32_calc >> 24) & 0xFF);

	ota_send_msg(buf, sizeof(buf));
}

/* ------------------------------------------------------------------ */
/*  Window bitfield helpers                                             */
/* ------------------------------------------------------------------ */

static void window_bitfield_clear(void)
{
	memset(ota_state.window_received, 0, sizeof(ota_state.window_received));
}

static void window_bitfield_set(uint16_t offset)
{
	ota_state.window_received[offset / 8] |= (1u << (offset % 8));
}

static bool window_bitfield_get(uint16_t offset)
{
	return (ota_state.window_received[offset / 8] >> (offset % 8)) & 1u;
}

static void send_gap_report(uint8_t window_idx)
{
	if (!ota_send_msg) {
		return;
	}

	uint16_t win_chunks = ota_state.window_size;
	uint16_t remaining = ota_state.total_chunks - ota_state.window_start;
	if (remaining < win_chunks) {
		win_chunks = remaining;
	}

	/* Collect missing chunk offsets within the window */
	uint8_t gaps[255];
	uint8_t gap_count = 0;
	for (uint16_t i = 0; i < win_chunks && gap_count < sizeof(gaps); i++) {
		if (!window_bitfield_get(i)) {
			gaps[gap_count++] = (uint8_t)i;
		}
	}

	if (gap_count == 0) {
		return; /* No gaps — caller should send WINDOW_ACK instead */
	}

	/* Send GAP_REPORT messages, up to 15 gap indices each (19B limit) */
	uint8_t sent = 0;
	while (sent < gap_count) {
		uint8_t batch = gap_count - sent;
		if (batch > 15) {
			batch = 15;
		}

		uint8_t buf[19]; /* cmd(1) + sub(1) + window_idx(1) + count(1) + gaps(15) */
		buf[0] = OTA_CMD_TYPE;
		buf[1] = OTA_SUB_GAP_REPORT;
		buf[2] = window_idx;
		buf[3] = batch;
		memcpy(&buf[4], &gaps[sent], batch);

		ota_send_msg(buf, 4 + batch);
		LOG_INF("OTA: GAP_REPORT win=%u gaps=%u (batch of %u)", window_idx, gap_count, batch);
		sent += batch;
	}
}

static void send_window_ack(uint8_t window_idx)
{
	if (!ota_send_msg) {
		return;
	}

	uint8_t buf[3];
	buf[0] = OTA_CMD_TYPE;
	buf[1] = OTA_SUB_WINDOW_ACK;
	buf[2] = window_idx;

	ota_send_msg(buf, sizeof(buf));
}

/* ------------------------------------------------------------------ */
/*  Recovery metadata                                                   */
/* ------------------------------------------------------------------ */

static int write_metadata(uint8_t state, uint32_t image_size, uint32_t image_crc32,
			  uint32_t app_version, uint32_t pages_copied, uint32_t total_pages)
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

	int err = ota_flash_erase_pages(OTA_METADATA_ADDR, sizeof(meta));
	if (err) {
		LOG_ERR("OTA: metadata erase failed: %d", err);
		return err;
	}

	err = ota_flash_write(OTA_METADATA_ADDR, (const uint8_t *)&meta, sizeof(meta));
	if (err) {
		LOG_ERR("OTA: metadata write failed: %d", err);
	}
	return err;
}

static int read_metadata(struct ota_metadata *meta)
{
	int err = ota_flash_read(OTA_METADATA_ADDR, (uint8_t *)meta, sizeof(*meta));
	if (err) {
		return err;
	}
	if (meta->magic != OTA_META_MAGIC) {
		return -ENOENT;
	}
	return 0;
}

static int clear_metadata(void)
{
	return ota_flash_erase_pages(OTA_METADATA_ADDR, OTA_FLASH_PAGE_SIZE);
}

/* ------------------------------------------------------------------ */
/*  CRC32 computation over flash region                                 */
/* ------------------------------------------------------------------ */

static uint32_t compute_flash_crc32(uint32_t addr, size_t size)
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

/* ------------------------------------------------------------------ */
/*  Apply: copy staging → primary                                       */
/* ------------------------------------------------------------------ */

/* Static buffer for page-at-a-time copy — avoids 4KB stack allocation
 * which overflows the shell thread stack during delta_test. */
static uint8_t ota_page_buf[OTA_FLASH_PAGE_SIZE];

static int ota_apply(void)
{
	LOG_INF("OTA: applying update (size=%u, crc=0x%08x)",
		ota_state.total_size, ota_state.expected_crc32);

	/* Stop all app callbacks before erasing primary partition */
	if (ota_pre_apply_hook) {
		LOG_INF("OTA: stopping app callbacks before apply");
		ota_pre_apply_hook();
	}

	uint32_t total_pages = (ota_state.total_size + OTA_FLASH_PAGE_SIZE - 1) /
			       OTA_FLASH_PAGE_SIZE;

	/* Write recovery metadata — APPLYING state */
	int err = write_metadata(OTA_META_STATE_APPLYING, ota_state.total_size,
				 ota_state.expected_crc32, ota_state.app_version,
				 0, total_pages);
	if (err) {
		return err;
	}

	/* Copy page by page: erase primary page, read staging, write primary */
	uint8_t *page_buf = ota_page_buf;

	for (uint32_t page = 0; page < total_pages; page++) {
		uint32_t src = OTA_STAGING_ADDR + (page * OTA_FLASH_PAGE_SIZE);
		uint32_t dst = OTA_APP_PRIMARY_ADDR + (page * OTA_FLASH_PAGE_SIZE);
		uint32_t copy_size = OTA_FLASH_PAGE_SIZE;

		/* Last page may be partial */
		if ((page + 1) * OTA_FLASH_PAGE_SIZE > ota_state.total_size) {
			copy_size = ota_state.total_size - (page * OTA_FLASH_PAGE_SIZE);
		}

		/* Read from staging */
		err = ota_flash_read(src, page_buf, copy_size);
		if (err) {
			LOG_ERR("OTA: staging read failed page %u: %d", page, err);
			return err;
		}

		/* Erase primary page */
		err = ota_flash_erase_pages(dst, OTA_FLASH_PAGE_SIZE);
		if (err) {
			LOG_ERR("OTA: primary erase failed page %u: %d", page, err);
			return err;
		}

		/* Write to primary */
		err = ota_flash_write(dst, page_buf, copy_size);
		if (err) {
			LOG_ERR("OTA: primary write failed page %u: %d", page, err);
			return err;
		}

		/* Update progress in metadata */
		write_metadata(OTA_META_STATE_APPLYING, ota_state.total_size,
			       ota_state.expected_crc32, ota_state.app_version,
			       page + 1, total_pages);

		LOG_DBG("OTA: copied page %u/%u", page + 1, total_pages);
	}

	/* Verify magic at primary address */
	uint32_t magic;
	ota_flash_read(OTA_APP_PRIMARY_ADDR, (uint8_t *)&magic, sizeof(magic));
	if (magic != APP_CALLBACK_MAGIC) {
		LOG_ERR("OTA: magic check failed after apply (got 0x%08x)", magic);
		return -EINVAL;
	}

	LOG_INF("OTA: apply complete, clearing metadata and rebooting");
	clear_metadata();

	/* Reboot to load new app */
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_WARM);

	/* Never reached */
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Boot recovery — resume interrupted apply                            */
/* ------------------------------------------------------------------ */

static int ota_resume_apply(const struct ota_metadata *meta)
{
	LOG_WRN("OTA: resuming interrupted apply (page %u/%u)",
		meta->pages_copied, meta->total_pages);

	uint8_t *page_buf = ota_page_buf;

	for (uint32_t page = meta->pages_copied; page < meta->total_pages; page++) {
		uint32_t src = OTA_STAGING_ADDR + (page * OTA_FLASH_PAGE_SIZE);
		uint32_t dst = OTA_APP_PRIMARY_ADDR + (page * OTA_FLASH_PAGE_SIZE);
		uint32_t copy_size = OTA_FLASH_PAGE_SIZE;

		if ((page + 1) * OTA_FLASH_PAGE_SIZE > meta->image_size) {
			copy_size = meta->image_size - (page * OTA_FLASH_PAGE_SIZE);
		}

		int err = ota_flash_read(src, page_buf, copy_size);
		if (err) {
			LOG_ERR("OTA recovery: staging read failed page %u: %d", page, err);
			return err;
		}

		err = ota_flash_erase_pages(dst, OTA_FLASH_PAGE_SIZE);
		if (err) {
			LOG_ERR("OTA recovery: primary erase failed page %u: %d", page, err);
			return err;
		}

		err = ota_flash_write(dst, page_buf, copy_size);
		if (err) {
			LOG_ERR("OTA recovery: primary write failed page %u: %d", page, err);
			return err;
		}

		/* Update progress */
		write_metadata(OTA_META_STATE_APPLYING, meta->image_size,
			       meta->image_crc32, meta->app_version,
			       page + 1, meta->total_pages);
	}

	/* Verify magic */
	uint32_t magic;
	ota_flash_read(OTA_APP_PRIMARY_ADDR, (uint8_t *)&magic, sizeof(magic));
	if (magic != APP_CALLBACK_MAGIC) {
		LOG_ERR("OTA recovery: magic check failed (got 0x%08x)", magic);
		return -EINVAL;
	}

	LOG_INF("OTA recovery: complete, clearing metadata and rebooting");
	clear_metadata();
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_WARM);
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Message handlers                                                    */
/* ------------------------------------------------------------------ */

static void handle_ota_start(const uint8_t *data, size_t len)
{
	/* Payload: total_size(4) total_chunks(2) chunk_size(2) crc32(4) version(4) = 16B
	 * After cmd(1) + sub(1) = offset 2, so total msg len >= 18 */
	if (len < 18) {
		LOG_ERR("OTA START: payload too short (%u)", len);
		send_ack(OTA_STATUS_SIZE_ERR, 0, 0);
		return;
	}

	if (ota_state.phase == OTA_PHASE_RECEIVING) {
		LOG_WRN("OTA START: aborting previous session");
	}

	const uint8_t *p = data + 2;
	uint32_t total_size   = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	uint16_t total_chunks = p[4] | (p[5] << 8);
	uint16_t chunk_size   = p[6] | (p[7] << 8);
	uint32_t crc32        = p[8] | (p[9] << 8) | (p[10] << 16) | (p[11] << 24);
	uint32_t version      = p[12] | (p[13] << 8) | (p[14] << 16) | (p[15] << 24);

	/* Parse optional window_size (byte 18) for blast mode */
	uint8_t window_size = 0;
	if (len >= 19) {
		window_size = data[18];
	}

	/* Detect delta mode: fewer chunks than the full image requires */
	uint16_t full_image_chunks = (total_size + chunk_size - 1) / chunk_size;
	bool is_delta = (total_chunks < full_image_chunks);

	LOG_INF("OTA START: size=%u chunks=%u/%u chunk_size=%u crc=0x%08x ver=%u win=%u%s",
		total_size, total_chunks, full_image_chunks, chunk_size, crc32,
		version, window_size, is_delta ? " DELTA" : "");

	/* Validate size fits staging area */
	if (total_size > OTA_STAGING_SIZE || total_size == 0) {
		LOG_ERR("OTA START: invalid size %u (max %u)", total_size, OTA_STAGING_SIZE);
		send_ack(OTA_STATUS_SIZE_ERR, 0, 0);
		return;
	}

	/* Delta mode: validate bitfield capacity */
	if (is_delta && full_image_chunks > sizeof(ota_state.delta_received) * 8) {
		LOG_ERR("OTA START: image too large for delta (%u chunks, max %u)",
			full_image_chunks, sizeof(ota_state.delta_received) * 8);
		send_ack(OTA_STATUS_SIZE_ERR, 0, 0);
		return;
	}

	/* Erase staging area */
	uint32_t erase_size = (total_size + OTA_FLASH_PAGE_SIZE - 1) &
			      ~(OTA_FLASH_PAGE_SIZE - 1);
	int err = ota_flash_erase_pages(OTA_STAGING_ADDR, erase_size);
	if (err) {
		LOG_ERR("OTA START: staging erase failed: %d", err);
		send_ack(OTA_STATUS_FLASH_ERR, 0, 0);
		return;
	}

	/* Initialize session state */
	ota_state.phase = OTA_PHASE_RECEIVING;
	ota_state.total_size = total_size;
	ota_state.total_chunks = total_chunks;
	ota_state.chunk_size = chunk_size;
	ota_state.expected_crc32 = crc32;
	ota_state.app_version = version;
	ota_state.chunks_received = 0;
	ota_state.bytes_written = 0;
	ota_state.window_size = window_size;
	ota_state.window_start = 0;
	ota_state.delta_mode = is_delta;
	ota_state.full_image_chunks = full_image_chunks;
	window_bitfield_clear();
	if (is_delta) {
		memset(ota_state.delta_received, 0, sizeof(ota_state.delta_received));
	}

	LOG_INF("OTA: staging erased, ready for chunks%s%s",
		window_size ? " (blast mode)" : "",
		is_delta ? " (delta mode)" : "");
	send_ack(OTA_STATUS_OK, 0, 0);
}

static void delta_validate_and_apply(void);

static void ota_validate_and_apply(void)
{
	if (ota_state.delta_mode) {
		delta_validate_and_apply();
		return;
	}

	LOG_INF("OTA: all chunks received, validating...");
	ota_state.phase = OTA_PHASE_VALIDATING;

	/* Compute CRC32 over staged image */
	uint32_t calc_crc32 = compute_flash_crc32(OTA_STAGING_ADDR,
						  ota_state.total_size);
	if (calc_crc32 != ota_state.expected_crc32) {
		LOG_ERR("OTA: CRC32 mismatch (calc=0x%08x, expected=0x%08x)",
			calc_crc32, ota_state.expected_crc32);
		send_complete(OTA_STATUS_CRC_ERR, calc_crc32);
		ota_state.phase = OTA_PHASE_ERROR;
		return;
	}

	LOG_INF("OTA: CRC32 OK (0x%08x), applying update", calc_crc32);
	send_complete(OTA_STATUS_OK, calc_crc32);

	/* Apply the update */
	ota_state.phase = OTA_PHASE_APPLYING;
	int ret = ota_apply();
	if (ret) {
		LOG_ERR("OTA: apply failed: %d", ret);
		ota_state.phase = OTA_PHASE_ERROR;
	}
	/* ota_apply reboots on success, so we only get here on failure */
}

/* ------------------------------------------------------------------ */
/*  Delta OTA: merge staging + primary, validate CRC, apply             */
/* ------------------------------------------------------------------ */

static inline bool delta_chunk_received(uint16_t idx)
{
	return (ota_state.delta_received[idx / 8] >> (idx % 8)) & 1u;
}

static void delta_validate_and_apply(void)
{
	LOG_INF("OTA: delta complete (%u/%u chunks), validating merged image...",
		ota_state.chunks_received, ota_state.full_image_chunks);
	ota_state.phase = OTA_PHASE_VALIDATING;

	/* CRC32 over merged image: staging (received) + primary (baseline) */
	uint32_t crc = 0;
	uint8_t buf[16];

	for (uint16_t i = 0; i < ota_state.full_image_chunks; i++) {
		uint32_t offset = (uint32_t)i * ota_state.chunk_size;
		uint16_t read_size = ota_state.chunk_size;
		uint32_t remaining = ota_state.total_size - offset;

		if (remaining < read_size) {
			read_size = (uint16_t)remaining;
		}

		uint32_t addr = delta_chunk_received(i)
			? (OTA_STAGING_ADDR + offset)
			: (OTA_APP_PRIMARY_ADDR + offset);

		int err = ota_flash_read(addr, buf, read_size);
		if (err) {
			LOG_ERR("OTA: delta CRC read failed at 0x%08x: %d",
				addr, err);
			send_complete(OTA_STATUS_FLASH_ERR, 0);
			ota_state.phase = OTA_PHASE_ERROR;
			return;
		}
		crc = crc32_ieee_update(crc, buf, read_size);
	}

	if (crc != ota_state.expected_crc32) {
		LOG_ERR("OTA: delta CRC32 mismatch (calc=0x%08x, expected=0x%08x)",
			crc, ota_state.expected_crc32);
		send_complete(OTA_STATUS_CRC_ERR, crc);
		ota_state.phase = OTA_PHASE_ERROR;
		return;
	}

	LOG_INF("OTA: delta CRC32 OK (0x%08x), applying", crc);
	send_complete(OTA_STATUS_OK, crc);

	/* Stop all app callbacks before erasing primary partition */
	if (ota_pre_apply_hook) {
		LOG_INF("OTA: stopping app callbacks before delta apply");
		ota_pre_apply_hook();
	}

	/* Apply: page by page, assemble from staging + primary → primary */
	ota_state.phase = OTA_PHASE_APPLYING;

	uint32_t total_pages = (ota_state.total_size + OTA_FLASH_PAGE_SIZE - 1) /
			       OTA_FLASH_PAGE_SIZE;
	uint8_t *page_buf = ota_page_buf;

	int err = write_metadata(OTA_META_STATE_APPLYING, ota_state.total_size,
				 ota_state.expected_crc32, ota_state.app_version,
				 0, total_pages);
	if (err) {
		ota_state.phase = OTA_PHASE_ERROR;
		return;
	}

	for (uint32_t page = 0; page < total_pages; page++) {
		uint32_t page_offset = page * OTA_FLASH_PAGE_SIZE;
		uint32_t copy_size = OTA_FLASH_PAGE_SIZE;

		if (page_offset + copy_size > ota_state.total_size) {
			copy_size = ota_state.total_size - page_offset;
		}

		/* Read baseline from primary first */
		err = ota_flash_read(OTA_APP_PRIMARY_ADDR + page_offset,
				     page_buf, copy_size);
		if (err) {
			LOG_ERR("OTA: delta baseline read failed page %u: %d",
				page, err);
			ota_state.phase = OTA_PHASE_ERROR;
			return;
		}

		/* Overlay received chunks from staging */
		uint16_t first_chunk = page_offset / ota_state.chunk_size;
		uint16_t last_chunk = (page_offset + copy_size - 1) /
				      ota_state.chunk_size;
		if (last_chunk >= ota_state.full_image_chunks) {
			last_chunk = ota_state.full_image_chunks - 1;
		}

		for (uint16_t ci = first_chunk; ci <= last_chunk; ci++) {
			if (!delta_chunk_received(ci)) {
				continue;
			}
			/* Chunk start/end within this page */
			uint32_t chunk_abs = (uint32_t)ci * ota_state.chunk_size;
			uint32_t start = (chunk_abs > page_offset)
				? (chunk_abs - page_offset) : 0;
			uint32_t src_offset = (chunk_abs > page_offset)
				? 0 : (page_offset - chunk_abs);
			uint32_t end = ((uint32_t)(ci + 1) * ota_state.chunk_size)
				       - page_offset;
			if (end > copy_size) {
				end = copy_size;
			}
			uint16_t len = (uint16_t)(end - start);

			err = ota_flash_read(OTA_STAGING_ADDR + chunk_abs + src_offset,
					     &page_buf[start], len);
			if (err) {
				LOG_ERR("OTA: delta staging read ci=%u: %d",
					ci, err);
				ota_state.phase = OTA_PHASE_ERROR;
				return;
			}
		}

		/* Erase primary page and write assembled data */
		err = ota_flash_erase_pages(OTA_APP_PRIMARY_ADDR + page_offset,
					    OTA_FLASH_PAGE_SIZE);
		if (err) {
			LOG_ERR("OTA: delta primary erase page %u: %d", page, err);
			ota_state.phase = OTA_PHASE_ERROR;
			return;
		}

		err = ota_flash_write(OTA_APP_PRIMARY_ADDR + page_offset,
				      page_buf, copy_size);
		if (err) {
			LOG_ERR("OTA: delta primary write page %u: %d", page, err);
			ota_state.phase = OTA_PHASE_ERROR;
			return;
		}

		write_metadata(OTA_META_STATE_APPLYING, ota_state.total_size,
			       ota_state.expected_crc32, ota_state.app_version,
			       page + 1, total_pages);
	}

	/* Verify magic at primary */
	uint32_t magic;
	ota_flash_read(OTA_APP_PRIMARY_ADDR, (uint8_t *)&magic, sizeof(magic));
	if (magic != APP_CALLBACK_MAGIC) {
		LOG_ERR("OTA: delta magic check failed (got 0x%08x)", magic);
		ota_state.phase = OTA_PHASE_ERROR;
		return;
	}

	LOG_INF("OTA: delta apply complete, rebooting");
	clear_metadata();
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_WARM);
}

static void handle_ota_chunk(const uint8_t *data, size_t len)
{
	/* Compact format (fits 19B Sidewalk LoRa limit):
	 * cmd(1) + sub(1) + chunk_idx(2) + data(N)
	 * data_len derived from message size; no per-chunk CRC
	 * (AEAD protects integrity, full CRC32 verified at end) */
	if (len < 5) {
		LOG_ERR("OTA CHUNK: payload too short (%u)", len);
		send_ack(OTA_STATUS_SIZE_ERR, ota_state.chunks_received, ota_state.chunks_received);
		return;
	}

	if (ota_state.phase != OTA_PHASE_RECEIVING) {
		LOG_ERR("OTA CHUNK: not in RECEIVING phase (phase=%d)", ota_state.phase);
		send_ack(OTA_STATUS_NO_SESSION, 0, 0);
		return;
	}

	const uint8_t *p = data + 2;
	uint16_t chunk_idx  = p[0] | (p[1] << 8);
	uint16_t data_len   = len - 4;  /* total msg minus 4B header */
	const uint8_t *chunk_data = p + 2;

	/* --- Blast mode (windowed) --- */
	if (ota_state.window_size > 0) {
		/* Reject chunks outside current window */
		if (chunk_idx < ota_state.window_start ||
		    chunk_idx >= ota_state.window_start + ota_state.window_size) {
			/* Could be from a previous window retry — ignore silently */
			if (chunk_idx < ota_state.window_start) {
				LOG_DBG("OTA CHUNK %u: before window %u, ignoring",
					chunk_idx, ota_state.window_start);
				return;
			}
			LOG_WRN("OTA CHUNK %u: beyond current window [%u..%u), ignoring",
				chunk_idx, ota_state.window_start,
				ota_state.window_start + ota_state.window_size);
			return;
		}

		uint16_t offset_in_window = chunk_idx - ota_state.window_start;

		/* Duplicate check within window */
		if (window_bitfield_get(offset_in_window)) {
			LOG_DBG("OTA CHUNK %u: duplicate in window, ignoring", chunk_idx);
			return;
		}

		/* Write to staging at absolute position */
		uint32_t write_addr = OTA_STAGING_ADDR +
				      (uint32_t)chunk_idx * ota_state.chunk_size;
		int err = ota_flash_write(write_addr, chunk_data, data_len);
		if (err) {
			LOG_ERR("OTA CHUNK %u: flash write failed at 0x%08x: %d",
				chunk_idx, write_addr, err);
			return; /* No ACK in blast mode; gap fill will retry */
		}

		window_bitfield_set(offset_in_window);
		ota_state.chunks_received++;
		ota_state.bytes_written += data_len;

		LOG_INF("OTA CHUNK %u/%u (win %u+%u)",
			chunk_idx + 1, ota_state.total_chunks,
			ota_state.window_start, offset_in_window);

		/* No per-chunk ACK in blast mode */
		return;
	}

	/* --- Delta mode (sparse chunks with absolute positioning) --- */
	if (ota_state.delta_mode) {
		if (chunk_idx >= ota_state.full_image_chunks) {
			LOG_ERR("OTA DELTA: idx %u beyond image (%u)",
				chunk_idx, ota_state.full_image_chunks);
			send_ack(OTA_STATUS_SIZE_ERR,
				 ota_state.chunks_received,
				 ota_state.chunks_received);
			return;
		}

		if (delta_chunk_received(chunk_idx)) {
			LOG_WRN("OTA DELTA %u: dup, ACK ok", chunk_idx);
			send_ack(OTA_STATUS_OK,
				 ota_state.chunks_received,
				 ota_state.chunks_received);
			return;
		}

		uint32_t write_addr = OTA_STAGING_ADDR +
				      (uint32_t)chunk_idx * ota_state.chunk_size;
		int err = ota_flash_write(write_addr, chunk_data, data_len);
		if (err) {
			LOG_ERR("OTA DELTA %u: flash write err %d",
				chunk_idx, err);
			send_ack(OTA_STATUS_FLASH_ERR,
				 ota_state.chunks_received,
				 ota_state.chunks_received);
			return;
		}

		ota_state.delta_received[chunk_idx / 8] |=
			(1u << (chunk_idx % 8));
		ota_state.chunks_received++;
		ota_state.bytes_written += data_len;

		LOG_INF("OTA DELTA %u/%u (abs idx %u)",
			ota_state.chunks_received, ota_state.total_chunks,
			chunk_idx);

		if (ota_state.chunks_received >= ota_state.total_chunks) {
			ota_validate_and_apply();
		} else {
			send_ack(OTA_STATUS_OK,
				 ota_state.chunks_received,
				 ota_state.chunks_received);
		}
		return;
	}

	/* --- Legacy mode (per-chunk ACK, sequential) --- */

	/* Handle duplicate chunks (retry from cloud) */
	if (chunk_idx < ota_state.chunks_received) {
		LOG_WRN("OTA CHUNK %u: duplicate (already have %u), ACK ok",
			chunk_idx, ota_state.chunks_received);
		send_ack(OTA_STATUS_OK, ota_state.chunks_received, ota_state.chunks_received);
		return;
	}

	/* Verify sequential order */
	if (chunk_idx != ota_state.chunks_received) {
		LOG_ERR("OTA CHUNK: expected %u, got %u", ota_state.chunks_received, chunk_idx);
		send_ack(OTA_STATUS_CRC_ERR, ota_state.chunks_received, ota_state.chunks_received);
		return;
	}

	/* Write to staging */
	uint32_t write_addr = OTA_STAGING_ADDR + ota_state.bytes_written;
	int err = ota_flash_write(write_addr, chunk_data, data_len);
	if (err) {
		LOG_ERR("OTA CHUNK %u: flash write failed at 0x%08x: %d",
			chunk_idx, write_addr, err);
		send_ack(OTA_STATUS_FLASH_ERR, chunk_idx, ota_state.chunks_received);
		return;
	}

	ota_state.chunks_received++;
	ota_state.bytes_written += data_len;

	LOG_INF("OTA CHUNK %u/%u: %u bytes at 0x%08x (total %u/%u)",
		chunk_idx + 1, ota_state.total_chunks,
		data_len, write_addr,
		ota_state.bytes_written, ota_state.total_size);

	/* Check if this was the last chunk */
	if (ota_state.chunks_received >= ota_state.total_chunks) {
		ota_validate_and_apply();
	} else {
		/* ACK with next expected chunk */
		send_ack(OTA_STATUS_OK, ota_state.chunks_received, ota_state.chunks_received);
	}
}

static void handle_window_done(const uint8_t *data, size_t len)
{
	/* Format: cmd(1) + sub(1) + window_idx(1) = 3B */
	if (len < 3) {
		LOG_ERR("OTA WINDOW_DONE: too short (%u)", len);
		return;
	}

	if (ota_state.phase != OTA_PHASE_RECEIVING || ota_state.window_size == 0) {
		LOG_ERR("OTA WINDOW_DONE: not in blast receive mode");
		return;
	}

	uint8_t window_idx = data[2];

	/* How many chunks are in this window? */
	uint16_t win_chunks = ota_state.window_size;
	uint16_t remaining = ota_state.total_chunks - ota_state.window_start;
	if (remaining < win_chunks) {
		win_chunks = remaining;
	}

	/* Count received chunks in this window */
	uint16_t received_in_window = 0;
	for (uint16_t i = 0; i < win_chunks; i++) {
		if (window_bitfield_get(i)) {
			received_in_window++;
		}
	}

	LOG_INF("OTA WINDOW_DONE: win=%u received=%u/%u (total %u/%u)",
		window_idx, received_in_window, win_chunks,
		ota_state.chunks_received, ota_state.total_chunks);

	if (received_in_window < win_chunks) {
		/* Gaps found — report them */
		send_gap_report(window_idx);
		return;
	}

	/* Window complete — send ACK and advance */
	send_window_ack(window_idx);

	ota_state.window_start += ota_state.window_size;
	window_bitfield_clear();

	/* Check if all chunks received */
	if (ota_state.chunks_received >= ota_state.total_chunks) {
		ota_validate_and_apply();
	} else {
		LOG_INF("OTA: window %u complete, next window starts at chunk %u",
			window_idx, ota_state.window_start);
	}
}

static void handle_ota_abort(void)
{
	LOG_WRN("OTA: abort received");
	ota_state.phase = OTA_PHASE_IDLE;
	memset(&ota_state, 0, sizeof(ota_state));
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void ota_init(int (*send_fn)(const uint8_t *data, size_t len))
{
	ota_send_msg = send_fn;
	memset(&ota_state, 0, sizeof(ota_state));
	ota_state.phase = OTA_PHASE_IDLE;

	/* Pre-initialize flash device */
	ota_flash_init();

	LOG_INF("OTA: module initialized");
}

void ota_set_pre_apply_hook(void (*fn)(void))
{
	ota_pre_apply_hook = fn;
}

bool ota_boot_recovery_check(void)
{
	if (ota_flash_init() != 0) {
		return false;
	}

	struct ota_metadata meta;
	int err = read_metadata(&meta);
	if (err) {
		/* No valid metadata — normal boot */
		return false;
	}

	if (meta.state == OTA_META_STATE_APPLYING) {
		LOG_WRN("OTA: detected interrupted apply, resuming...");
		ota_state.total_size = meta.image_size;
		ota_state.expected_crc32 = meta.image_crc32;
		ota_state.app_version = meta.app_version;
		ota_resume_apply(&meta);
		/* ota_resume_apply reboots on success */
		return true;
	}

	if (meta.state == OTA_META_STATE_STAGED) {
		/* Staged but never applied — clear and continue normal boot */
		LOG_WRN("OTA: found staged but unapplied image, clearing");
		clear_metadata();
	}

	return false;
}

void ota_process_msg(const uint8_t *data, size_t len)
{
	if (len < 2) {
		LOG_ERR("OTA: message too short (%u)", len);
		return;
	}

	if (data[0] != OTA_CMD_TYPE) {
		LOG_ERR("OTA: unexpected cmd type 0x%02x", data[0]);
		return;
	}

	uint8_t subtype = data[1];

	switch (subtype) {
	case OTA_SUB_START:
		handle_ota_start(data, len);
		break;
	case OTA_SUB_CHUNK:
		handle_ota_chunk(data, len);
		break;
	case OTA_SUB_ABORT:
		handle_ota_abort();
		break;
	case OTA_SUB_WINDOW_DONE:
		handle_window_done(data, len);
		break;
	default:
		LOG_ERR("OTA: unknown subtype 0x%02x", subtype);
		break;
	}
}

void ota_abort(void)
{
	if (ota_state.phase != OTA_PHASE_IDLE) {
		LOG_WRN("OTA: manually aborted (was in phase %s)",
			ota_phase_str(ota_state.phase));
	}
	ota_state.phase = OTA_PHASE_IDLE;
	memset(&ota_state, 0, sizeof(ota_state));
}

enum ota_phase ota_get_phase(void)
{
	return ota_state.phase;
}

const char *ota_phase_str(enum ota_phase phase)
{
	switch (phase) {
	case OTA_PHASE_IDLE:       return "IDLE";
	case OTA_PHASE_RECEIVING:  return "RECEIVING";
	case OTA_PHASE_VALIDATING: return "VALIDATING";
	case OTA_PHASE_APPLYING:   return "APPLYING";
	case OTA_PHASE_COMPLETE:   return "COMPLETE";
	case OTA_PHASE_ERROR:      return "ERROR";
	default:                   return "UNKNOWN";
	}
}

void ota_test_delta(uint32_t new_size, uint32_t new_crc32, uint32_t new_version)
{
	/* Test helper: trigger delta validate+apply using chunks already
	 * written to staging via pyOCD.  Caller provides the new image
	 * metadata; the delta_received bitfield must be pre-populated
	 * (call ota_test_delta_mark_chunk for each written chunk). */
	if (ota_state.phase != OTA_PHASE_RECEIVING || !ota_state.delta_mode) {
		LOG_ERR("ota_test_delta: not in delta receive mode");
		return;
	}
	ota_state.total_size = new_size;
	ota_state.expected_crc32 = new_crc32;
	ota_state.app_version = new_version;
	ota_state.full_image_chunks = (new_size + ota_state.chunk_size - 1) /
				      ota_state.chunk_size;
	delta_validate_and_apply();
}

void ota_test_delta_setup(uint16_t chunk_size, uint16_t total_delta_chunks,
			  uint32_t new_size, uint32_t new_crc32)
{
	/* Prepare state for a flash-based delta test.
	 * After calling this, use pyOCD to write changed chunks to staging,
	 * then call ota_test_delta_mark_chunk() for each, then ota_test_delta(). */
	memset(&ota_state, 0, sizeof(ota_state));
	ota_state.phase = OTA_PHASE_RECEIVING;
	ota_state.delta_mode = true;
	ota_state.chunk_size = chunk_size;
	ota_state.total_chunks = total_delta_chunks;
	ota_state.total_size = new_size;
	ota_state.expected_crc32 = new_crc32;
	ota_state.full_image_chunks = (new_size + chunk_size - 1) / chunk_size;
	LOG_INF("Delta test setup: %u delta chunks, %u full, size=%u crc=0x%08x",
		total_delta_chunks, ota_state.full_image_chunks, new_size, new_crc32);
}

void ota_test_delta_mark_chunk(uint16_t abs_chunk_idx)
{
	if (abs_chunk_idx < sizeof(ota_state.delta_received) * 8) {
		ota_state.delta_received[abs_chunk_idx / 8] |=
			(1u << (abs_chunk_idx % 8));
		ota_state.chunks_received++;
	}
}

void ota_send_status(void)
{
	if (!ota_send_msg) {
		return;
	}

	uint8_t buf[11];
	buf[0] = OTA_CMD_TYPE;
	buf[1] = OTA_SUB_STATUS;
	buf[2] = (uint8_t)ota_state.phase;
	buf[3] = (uint8_t)(ota_state.chunks_received & 0xFF);
	buf[4] = (uint8_t)(ota_state.chunks_received >> 8);
	buf[5] = (uint8_t)(ota_state.total_chunks & 0xFF);
	buf[6] = (uint8_t)(ota_state.total_chunks >> 8);
	buf[7] = (uint8_t)(ota_state.app_version & 0xFF);
	buf[8] = (uint8_t)((ota_state.app_version >> 8) & 0xFF);
	buf[9] = (uint8_t)((ota_state.app_version >> 16) & 0xFF);
	buf[10] = (uint8_t)((ota_state.app_version >> 24) & 0xFF);

	ota_send_msg(buf, sizeof(buf));
}
