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
#include <ota_flash.h>
#include <ota_signing.h>
#include <platform_api.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>
#include <string.h>

LOG_MODULE_REGISTER(ota_update, CONFIG_SIDEWALK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/*  Deferred apply — delay reboot to let COMPLETE uplink transmit       */
/* ------------------------------------------------------------------ */

static void ota_deferred_apply_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(ota_deferred_apply_work, ota_deferred_apply_handler);
#define OTA_APPLY_DELAY_SEC 15

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
	/* Signing: OTA_START indicated signed firmware */
	bool     is_signed;
	/* Delta mode: total_chunks < full image chunks */
	bool     delta_mode;
	uint16_t full_image_chunks;    /* chunks in the full image */
	uint8_t  delta_received[128];  /* bitfield: up to 1024 chunks (~15KB) */
} ota_state;

static int (*ota_send_msg)(const uint8_t *data, size_t len);
static void (*ota_pre_apply_hook)(void);

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
/*  Stale page cleanup — erase pages beyond new image                   */
/* ------------------------------------------------------------------ */

static void erase_stale_app_pages(uint32_t image_size)
{
	uint32_t next_page = OTA_APP_PRIMARY_ADDR +
		((image_size + OTA_FLASH_PAGE_SIZE - 1) & ~(OTA_FLASH_PAGE_SIZE - 1));
	uint32_t metadata_page = OTA_METADATA_ADDR & ~(OTA_FLASH_PAGE_SIZE - 1);

	if (next_page >= metadata_page) {
		return;
	}

	uint32_t erase_size = metadata_page - next_page;
	LOG_INF("OTA: erasing %u stale pages at 0x%08x",
		erase_size / OTA_FLASH_PAGE_SIZE, next_page);
	ota_flash_erase_pages(next_page, erase_size);
}

/* ------------------------------------------------------------------ */
/*  Apply: copy staging → primary                                       */
/* ------------------------------------------------------------------ */

/* Static buffer for page-at-a-time copy AND ED25519 signature verify.
 * Must be large enough to hold the full app image for signature verification
 * (ED25519 is not streaming — needs entire message at once). */
static uint8_t ota_page_buf[OTA_VERIFY_BUF_SIZE];

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

	/* Erase stale pages beyond new image to prevent inflated baselines */
	erase_stale_app_pages(ota_state.total_size);

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

	/* Erase stale pages beyond new image to prevent inflated baselines */
	erase_stale_app_pages(meta->image_size);

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

	/* Parse optional flags byte (byte 19 = data[18]) */
	uint8_t flags = 0;
	if (len >= 19) {
		flags = data[18];
	}
	bool is_signed = (flags & OTA_START_FLAGS_SIGNED) != 0;

	/* Detect delta mode: fewer chunks than the full image requires */
	uint16_t full_image_chunks = (total_size + chunk_size - 1) / chunk_size;
	bool is_delta = (total_chunks < full_image_chunks);

	LOG_INF("OTA START: size=%u chunks=%u/%u chunk_size=%u crc=0x%08x ver=%u%s%s",
		total_size, total_chunks, full_image_chunks, chunk_size, crc32,
		version, is_delta ? " DELTA" : "", is_signed ? " SIGNED" : "");

	/* Reject START during active apply phases */
	if (ota_state.phase == OTA_PHASE_APPLYING || ota_state.phase == OTA_PHASE_COMPLETE) {
		LOG_WRN("OTA START: busy (phase=%s), rejecting", ota_phase_str(ota_state.phase));
		send_ack(OTA_STATUS_NO_SESSION, 0, 0);
		return;
	}

	/* Check if firmware already applied (handles lost COMPLETE after reboot) */
	uint32_t primary_crc = compute_flash_crc32(OTA_APP_PRIMARY_ADDR, total_size);
	if (primary_crc == crc32) {
		LOG_INF("OTA START: firmware already applied (CRC 0x%08x), sending COMPLETE", crc32);
		send_complete(OTA_STATUS_OK, primary_crc);
		return;
	}

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
	ota_state.is_signed = is_signed;
	ota_state.delta_mode = is_delta;
	ota_state.full_image_chunks = full_image_chunks;
	if (is_delta) {
		memset(ota_state.delta_received, 0, sizeof(ota_state.delta_received));
	}

	LOG_INF("OTA: staging erased, ready for chunks%s",
		is_delta ? " (delta mode)" : "");
	send_ack(OTA_STATUS_OK, 0, 0);
}

static void delta_validate_and_apply(void);

/* Static buffer for reading signature from flash */
static uint8_t ota_sig_buf[OTA_SIG_SIZE];

/* Read firmware data from staging and verify ED25519 signature.
 * The signed image layout is: [firmware_data][64-byte signature].
 * Returns 0 on success, negative on failure. */
static int ota_verify_staged_signature(uint32_t staging_addr, uint32_t total_size)
{
	if (total_size <= OTA_SIG_SIZE) {
		LOG_ERR("OTA: signed image too small (%u)", total_size);
		return -EINVAL;
	}

	uint32_t fw_size = total_size - OTA_SIG_SIZE;

	/* Read signature from end of staged image */
	int err = ota_flash_read(staging_addr + fw_size, ota_sig_buf, OTA_SIG_SIZE);
	if (err) {
		LOG_ERR("OTA: failed to read signature: %d", err);
		return err;
	}

	/* Verify signature over firmware data (read in chunks to avoid
	 * allocating the entire firmware in RAM). For the initial
	 * implementation, ota_verify_signature() reads from the provided
	 * buffer, so we pass staging address and let the verify function
	 * handle it. However, the current API takes a flat buffer.
	 *
	 * Compromise: ED25519 requires the full message at once (not streaming),
	 * so we read the entire firmware into ota_page_buf (16KB). */
	if (fw_size > OTA_VERIFY_BUF_SIZE) {
		LOG_ERR("OTA: signed firmware too large for verify buffer (%u > %u)",
			fw_size, OTA_VERIFY_BUF_SIZE);
		return -ENOMEM;
	}

	err = ota_flash_read(staging_addr, ota_page_buf, fw_size);
	if (err) {
		LOG_ERR("OTA: failed to read firmware for verify: %d", err);
		return err;
	}

	err = ota_verify_signature(ota_page_buf, fw_size, ota_sig_buf);
	if (err) {
		LOG_ERR("OTA: ED25519 signature verification failed: %d", err);
		return err;
	}

	LOG_INF("OTA: ED25519 signature verified OK (%u bytes firmware)", fw_size);
	return 0;
}

static void ota_validate_and_apply(void)
{
	if (ota_state.delta_mode) {
		delta_validate_and_apply();
		return;
	}

	LOG_INF("OTA: all chunks received, validating...");
	ota_state.phase = OTA_PHASE_VALIDATING;

	/* Compute CRC32 over staged image (includes signature if signed) */
	uint32_t calc_crc32 = compute_flash_crc32(OTA_STAGING_ADDR,
						  ota_state.total_size);
	if (calc_crc32 != ota_state.expected_crc32) {
		LOG_ERR("OTA: CRC32 mismatch (calc=0x%08x, expected=0x%08x)",
			calc_crc32, ota_state.expected_crc32);
		send_complete(OTA_STATUS_CRC_ERR, calc_crc32);
		ota_state.phase = OTA_PHASE_ERROR;
		return;
	}

	/* ED25519 signature verification (if flagged as signed) */
	if (ota_state.is_signed) {
		int sig_err = ota_verify_staged_signature(OTA_STAGING_ADDR,
							  ota_state.total_size);
		if (sig_err) {
			send_complete(OTA_STATUS_SIG_ERR, calc_crc32);
			ota_state.phase = OTA_PHASE_ERROR;
			return;
		}
	}

	LOG_INF("OTA: CRC32 OK (0x%08x), scheduling apply in %ds", calc_crc32, OTA_APPLY_DELAY_SEC);
	send_complete(OTA_STATUS_OK, calc_crc32);
	ota_state.phase = OTA_PHASE_COMPLETE;
	k_work_schedule(&ota_deferred_apply_work, K_SECONDS(OTA_APPLY_DELAY_SEC));
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

	/* ED25519 signature verification for delta mode.
	 * The merged image is firmware+signature; we need to read the
	 * merged content into RAM for verification. ED25519 requires the
	 * full message, so ota_page_buf must be large enough. */
	if (ota_state.is_signed) {
		if (ota_state.total_size <= OTA_SIG_SIZE ||
		    ota_state.total_size > OTA_VERIFY_BUF_SIZE) {
			LOG_ERR("OTA: delta signed image too large for verify (%u > %u)",
				ota_state.total_size, OTA_VERIFY_BUF_SIZE);
			send_complete(OTA_STATUS_SIG_ERR, crc);
			ota_state.phase = OTA_PHASE_ERROR;
			return;
		}

		uint32_t fw_size = ota_state.total_size - OTA_SIG_SIZE;

		/* Read merged image into ota_page_buf (same merge logic as CRC) */
		for (uint16_t ci = 0; ci < ota_state.full_image_chunks; ci++) {
			uint32_t offset = (uint32_t)ci * ota_state.chunk_size;
			uint16_t read_size = ota_state.chunk_size;
			uint32_t remaining = ota_state.total_size - offset;

			if (remaining < read_size) {
				read_size = (uint16_t)remaining;
			}

			uint32_t addr = delta_chunk_received(ci)
				? (OTA_STAGING_ADDR + offset)
				: (OTA_APP_PRIMARY_ADDR + offset);

			int err = ota_flash_read(addr, &ota_page_buf[offset],
						 read_size);
			if (err) {
				LOG_ERR("OTA: delta sig read chunk %u: %d",
					ci, err);
				send_complete(OTA_STATUS_SIG_ERR, crc);
				ota_state.phase = OTA_PHASE_ERROR;
				return;
			}
		}

		/* Signature is last 64 bytes of merged image */
		int sig_err = ota_verify_signature(ota_page_buf, fw_size,
						   &ota_page_buf[fw_size]);
		if (sig_err) {
			LOG_ERR("OTA: delta ED25519 signature verification failed");
			send_complete(OTA_STATUS_SIG_ERR, crc);
			ota_state.phase = OTA_PHASE_ERROR;
			return;
		}

		LOG_INF("OTA: delta ED25519 signature verified OK");
	}

	LOG_INF("OTA: delta CRC32 OK (0x%08x), scheduling apply in %ds", crc, OTA_APPLY_DELAY_SEC);
	send_complete(OTA_STATUS_OK, crc);
	ota_state.phase = OTA_PHASE_COMPLETE;
	k_work_schedule(&ota_deferred_apply_work, K_SECONDS(OTA_APPLY_DELAY_SEC));
}

static void delta_apply(void)
{
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

	/* Erase stale pages beyond new image to prevent inflated baselines */
	erase_stale_app_pages(ota_state.total_size);

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

static void ota_deferred_apply_handler(struct k_work *work)
{
	if (ota_state.phase != OTA_PHASE_COMPLETE) {
		LOG_WRN("OTA: deferred apply cancelled (phase=%s)", ota_phase_str(ota_state.phase));
		return;
	}
	LOG_INF("OTA: deferred apply firing after %ds delay", OTA_APPLY_DELAY_SEC);
	if (ota_state.delta_mode) {
		delta_apply();
	} else {
		ota_state.phase = OTA_PHASE_APPLYING;
		int ret = ota_apply();
		if (ret) {
			LOG_ERR("OTA: apply failed: %d", ret);
			ota_state.phase = OTA_PHASE_ERROR;
		}
	}
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
		/* Verify total bytes match (full-mode only sanity check) */
		if (ota_state.bytes_written != ota_state.total_size) {
			LOG_ERR("OTA: size mismatch (written %u, expected %u)",
				ota_state.bytes_written, ota_state.total_size);
			send_complete(OTA_STATUS_SIZE_ERR, 0);
			ota_state.phase = OTA_PHASE_ERROR;
			return;
		}

		ota_validate_and_apply();
	} else {
		/* ACK with next expected chunk */
		send_ack(OTA_STATUS_OK, ota_state.chunks_received, ota_state.chunks_received);
	}
}

static void handle_ota_abort(void)
{
	k_work_cancel_delayable(&ota_deferred_apply_work);
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
	default:
		LOG_ERR("OTA: unknown subtype 0x%02x", subtype);
		break;
	}
}

void ota_abort(void)
{
	k_work_cancel_delayable(&ota_deferred_apply_work);
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
