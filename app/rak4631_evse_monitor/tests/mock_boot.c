/*
 * Mock implementations for platform dependencies when testing app.c on the host.
 *
 * Provides stub implementations for Sidewalk, OTA, LED, and config functions
 * that app.c calls. Only the OTA mock tracks calls (for routing tests).
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <platform_api.h>

/* ------------------------------------------------------------------ */
/*  Observable OTA mock state (used by routing tests)                   */
/* ------------------------------------------------------------------ */

int mock_ota_process_msg_called;
uint8_t mock_ota_last_data[64];
size_t mock_ota_last_len;

void mock_ota_reset(void)
{
	mock_ota_process_msg_called = 0;
	mock_ota_last_len = 0;
	memset(mock_ota_last_data, 0, sizeof(mock_ota_last_data));
}

/* ------------------------------------------------------------------ */
/*  OTA stubs                                                           */
/* ------------------------------------------------------------------ */

void ota_init(int (*send_fn)(const uint8_t *, size_t))
{
	(void)send_fn;
}

void ota_set_pre_apply_hook(void (*fn)(void))
{
	(void)fn;
}

bool ota_boot_recovery_check(void)
{
	return false;
}

void ota_process_msg(const uint8_t *data, size_t len)
{
	mock_ota_process_msg_called++;
	if (len <= sizeof(mock_ota_last_data)) {
		memcpy(mock_ota_last_data, data, len);
	}
	mock_ota_last_len = len;
}

/* ------------------------------------------------------------------ */
/*  Sidewalk stubs                                                      */
/* ------------------------------------------------------------------ */

void sidewalk_start(void *ctx)
{
	(void)ctx;
}

void sidewalk_event_send(void (*handler)(), void *data, void (*free_fn)(void *))
{
	(void)handler; (void)data; (void)free_fn;
}

void sidewalk_event_platform_init(void)
{
}

void sidewalk_event_autostart(void)
{
}

/* ------------------------------------------------------------------ */
/*  LED / config stubs                                                  */
/* ------------------------------------------------------------------ */

int app_led_init(void)
{
	return 0;
}

void *app_get_ble_config(void)
{
	return NULL;
}

void *app_get_sub_ghz_config(void)
{
	return NULL;
}

void sidewalk_dispatch_fill_callbacks(void *cbs, void *context)
{
	(void)cbs; (void)context;
}

int sidewalk_dispatch_register_gatt_auth(void)
{
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Platform API table (extern required by app.c, unused in tests)      */
/* ------------------------------------------------------------------ */

const struct platform_api platform_api_table = {0};
