/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Sidewalk Shell Commands for debugging and testing
 */

#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <tx_state.h>
#include <sidewalk.h>
#include <sid_pal_mfg_store_ifc.h>
#include <sid_pal_crypto_ifc.h>

LOG_MODULE_REGISTER(sid_shell, CONFIG_SIDEWALK_LOG_LEVEL);

static const char *link_type_str(uint32_t link_mask)
{
	if (link_mask & SID_LINK_TYPE_1) return "BLE";
	if (link_mask & SID_LINK_TYPE_2) return "FSK";
	if (link_mask & SID_LINK_TYPE_3) return "LoRa";
	return "None";
}

static int cmd_sid_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	bool ready = tx_state_is_ready();
	uint32_t link_mask = tx_state_get_link_mask();
	sid_init_status_t init = sidewalk_get_init_status();

	shell_print(sh, "Sidewalk Status:");
	shell_print(sh, "  Init state: %s (err=%d)",
		    sidewalk_init_state_str(init.state), init.err_code);
	shell_print(sh, "  Ready: %s", ready ? "YES" : "NO");
	shell_print(sh, "  Link type: %s (0x%x)", link_type_str(link_mask), link_mask);

	/* Explain what the init state means */
	switch (init.state) {
	case SID_INIT_NOT_STARTED:
		shell_warn(sh, "  -> Init never ran. app_start() may have failed early.");
		break;
	case SID_INIT_PLATFORM_INIT_ERR:
		shell_error(sh, "  -> sid_platform_init() failed (err=%d). Check radio/SPI config.",
			    init.err_code);
		break;
	case SID_INIT_MFG_EMPTY:
		shell_error(sh, "  -> MFG store is empty! Flash mfg.hex with device credentials.");
		break;
	case SID_INIT_RADIO_INIT_ERR:
		shell_error(sh, "  -> Radio init failed (err=%d). Check SX1262 SPI/GPIO wiring.",
			    init.err_code);
		break;
	case SID_INIT_SID_INIT_ERR:
		shell_error(sh, "  -> sid_init() failed (err=%d). Config or memory issue.",
			    init.err_code);
		break;
	case SID_INIT_SID_START_ERR:
		shell_error(sh, "  -> sid_start() failed (err=%d). Link mask or state issue.",
			    init.err_code);
		break;
	case SID_INIT_STARTED_OK:
		if (!ready) {
			shell_warn(sh, "  -> Sidewalk started but not READY. Waiting for gateway.");
			shell_warn(sh, "     Ensure a Sidewalk gateway (Ring/Echo) is in range.");
		} else {
			shell_print(sh, "  -> Sidewalk running and connected.");
		}
		break;
	}

	return 0;
}

static int cmd_sid_mfg(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint32_t ver = sid_pal_mfg_store_get_version();
	shell_print(sh, "MFG Store:");
	shell_print(sh, "  Version: %u", ver);
	if (ver == 0 || ver == 0xFFFFFFFF) {
		shell_error(sh, "  -> MFG partition is EMPTY or ERASED!");
		return -1;
	}

	/* Device ID */
	uint8_t dev_id[5] = {0};
	bool ok = sid_pal_mfg_store_dev_id_get(dev_id);
	shell_print(sh, "  Device ID: %s %02x:%02x:%02x:%02x:%02x",
		    ok ? "" : "(FAIL)",
		    dev_id[0], dev_id[1], dev_id[2], dev_id[3], dev_id[4]);

	/* Check key sizes — nonzero means key exists */
	uint8_t key_buf[64];
	static const uint8_t zeros[32] = {0};

	/* ED25519 private key */
	memset(key_buf, 0, sizeof(key_buf));
	sid_pal_mfg_store_read(SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519,
			       key_buf, SID_PAL_MFG_STORE_DEVICE_PRIV_ED25519_SIZE);
	bool ed_priv_ok = (memcmp(key_buf, zeros, 32) != 0);
	shell_print(sh, "  ED25519 priv key: %s (first 4: %02x%02x%02x%02x)",
		    ed_priv_ok ? "PRESENT" : "MISSING/ZERO",
		    key_buf[0], key_buf[1], key_buf[2], key_buf[3]);

	/* ED25519 public key */
	memset(key_buf, 0, sizeof(key_buf));
	sid_pal_mfg_store_read(SID_PAL_MFG_STORE_DEVICE_PUB_ED25519,
			       key_buf, SID_PAL_MFG_STORE_DEVICE_PUB_ED25519_SIZE);
	bool ed_pub_ok = (memcmp(key_buf, zeros, 32) != 0);
	shell_print(sh, "  ED25519 pub key:  %s (first 4: %02x%02x%02x%02x)",
		    ed_pub_ok ? "PRESENT" : "MISSING/ZERO",
		    key_buf[0], key_buf[1], key_buf[2], key_buf[3]);

	/* P256R1 private key */
	memset(key_buf, 0, sizeof(key_buf));
	sid_pal_mfg_store_read(SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1,
			       key_buf, SID_PAL_MFG_STORE_DEVICE_PRIV_P256R1_SIZE);
	bool p256_priv_ok = (memcmp(key_buf, zeros, 32) != 0);
	shell_print(sh, "  P256R1 priv key:  %s (first 4: %02x%02x%02x%02x)",
		    p256_priv_ok ? "PRESENT" : "MISSING/ZERO",
		    key_buf[0], key_buf[1], key_buf[2], key_buf[3]);

	/* P256R1 public key */
	memset(key_buf, 0, sizeof(key_buf));
	sid_pal_mfg_store_read(SID_PAL_MFG_STORE_DEVICE_PUB_P256R1,
			       key_buf, SID_PAL_MFG_STORE_DEVICE_PUB_P256R1_SIZE);
	bool p256_pub_ok = (memcmp(key_buf, zeros, 32) != 0);
	shell_print(sh, "  P256R1 pub key:   %s (first 4: %02x%02x%02x%02x)",
		    p256_pub_ok ? "PRESENT" : "MISSING/ZERO",
		    key_buf[0], key_buf[1], key_buf[2], key_buf[3]);

	/* Clear sensitive key data from stack */
	memset(key_buf, 0, sizeof(key_buf));

	if (!ed_priv_ok || !ed_pub_ok) {
		shell_error(sh, "  -> ED25519 keys MISSING! Re-provision device.");
	}
	if (!p256_priv_ok || !p256_pub_ok) {
		shell_error(sh, "  -> P256R1 keys MISSING! Re-provision device.");
	}
	if (ed_priv_ok && ed_pub_ok && p256_priv_ok && p256_pub_ok) {
		shell_print(sh, "  -> All MFG keys present.");
	}

	return 0;
}

static int cmd_sid_reinit(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Re-running Sidewalk init sequence...");
	shell_print(sh, "(Watch log output for errors)");

	/* Queue platform init and autostart on the sidewalk thread */
	int err = sidewalk_event_send(sidewalk_event_platform_init, NULL, NULL);
	if (err) {
		shell_error(sh, "Failed to queue platform_init: %d", err);
		return err;
	}

	err = sidewalk_event_send(sidewalk_event_autostart, NULL, NULL);
	if (err) {
		shell_error(sh, "Failed to queue autostart: %d", err);
		return err;
	}

	shell_print(sh, "Init events queued. Run 'sid status' in a few seconds to check result.");
	return 0;
}

static int cmd_sid_send(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Triggering manual sensor read and transmit...");

	int err = tx_state_send_evse_data();
	if (err) {
		shell_error(sh, "Send failed: %d", err);
		return err;
	}

	shell_print(sh, "Send queued successfully");
	return 0;
}

static int cmd_sid_lora(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint32_t mask = SID_LINK_TYPE_3;

	tx_state_set_link_mask(mask);
	sidewalk_event_send(sidewalk_event_set_link,
			    (void *)(uintptr_t)mask, NULL);

	shell_print(sh, "Switching Sidewalk stack to LoRa (reinit)...");
	return 0;
}

static int cmd_sid_ble(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint32_t mask = SID_LINK_TYPE_1;

	tx_state_set_link_mask(mask);
	sidewalk_event_send(sidewalk_event_set_link,
			    (void *)(uintptr_t)mask, NULL);

	shell_print(sh, "Switching Sidewalk stack to BLE (reinit)...");
	return 0;
}

static int cmd_sid_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_warn(sh, "Sending factory reset — clears stored session keys and registration state.");
	shell_warn(sh, "Device will need to re-register with cloud after reboot.");

	int err = sidewalk_event_send(sidewalk_event_factory_reset, NULL, NULL);
	if (err) {
		shell_error(sh, "Failed to queue factory_reset: %d", err);
		return err;
	}

	shell_print(sh, "Factory reset queued. Device will reboot.");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sid_cmds,
	SHELL_CMD(status, NULL, "Show Sidewalk init state and status", cmd_sid_status),
	SHELL_CMD(mfg, NULL, "Check MFG store keys and device ID", cmd_sid_mfg),
	SHELL_CMD(reinit, NULL, "Re-run Sidewalk init (with visible logs)", cmd_sid_reinit),
	SHELL_CMD(send, NULL, "Trigger manual send", cmd_sid_send),
	SHELL_CMD(lora, NULL, "Switch to LoRa mode", cmd_sid_lora),
	SHELL_CMD(ble, NULL, "Switch to BLE mode", cmd_sid_ble),
	SHELL_CMD(reset, NULL, "Factory reset (clear session keys, re-register)", cmd_sid_reset),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sid, &sid_cmds, "Sidewalk commands", NULL);
