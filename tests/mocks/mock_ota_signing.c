/*
 * Mock OTA signing — provides controllable ota_verify_signature() for tests.
 *
 * Tests call mock_ota_signing_set_result() to control the return value,
 * allowing verification of the full signing flow in ota_update.c without
 * a real ED25519 implementation.
 */

#include <ota_signing.h>

static int mock_verify_result;
static int mock_verify_call_count;

void mock_ota_signing_reset(void)
{
	mock_verify_result = 0;
	mock_verify_call_count = 0;
}

void mock_ota_signing_set_result(int result)
{
	mock_verify_result = result;
}

int mock_ota_signing_get_call_count(void)
{
	return mock_verify_call_count;
}

int ota_verify_signature(const uint8_t *data, size_t len, const uint8_t *sig)
{
	(void)data;
	(void)len;
	(void)sig;
	mock_verify_call_count++;
	return mock_verify_result;
}
