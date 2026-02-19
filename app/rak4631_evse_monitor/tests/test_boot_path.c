/*
 * Host-side unit tests for the platform boot path.
 *
 * Tests discover_app_image(), app_route_message(), and
 * app_set_timer_interval() — all defined in app.c.
 *
 * Compiles app.c against mock headers (tests/mock_include/) and mock
 * implementations (mock_boot.c) on the host.  This is the Grenning
 * dual-target pattern extended to platform code.
 *
 * Covers:
 * - App image discovery: magic, version, reject reasons
 * - Message routing: OTA (0x20) vs app dispatch, NULL safety
 * - Timer interval bounds validation
 */

#include <app.h>
#include <platform_api.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/*  Test infrastructure                                                 */
/* ------------------------------------------------------------------ */

static int tests_run;
static int tests_passed;

#define RUN_TEST(fn) do { \
	tests_run++; \
	printf("  %-60s", #fn); \
	fn(); \
	tests_passed++; \
	printf("PASS\n"); \
} while (0)

/* ------------------------------------------------------------------ */
/*  Access to HOST_TEST hooks in app.c                                  */
/* ------------------------------------------------------------------ */

extern const struct app_callbacks *test_app_cb_addr;
extern void discover_app_image(void);

/* ------------------------------------------------------------------ */
/*  Observable mock state from mock_boot.c                              */
/* ------------------------------------------------------------------ */

extern int mock_ota_process_msg_called;
extern void mock_ota_reset(void);

/* ------------------------------------------------------------------ */
/*  Mock app callback table with controllable magic/version             */
/* ------------------------------------------------------------------ */

static int mock_on_msg_received_count;
static uint8_t mock_on_msg_received_data[64];
static size_t mock_on_msg_received_len;

static void mock_on_msg_received(const uint8_t *data, size_t len)
{
	mock_on_msg_received_count++;
	if (len <= sizeof(mock_on_msg_received_data)) {
		memcpy(mock_on_msg_received_data, data, len);
	}
	mock_on_msg_received_len = len;
}

static int mock_init_count;

static int mock_init(const struct platform_api *api)
{
	(void)api;
	mock_init_count++;
	return 0;
}

static struct app_callbacks test_cb;

static void setup_valid_cb(void)
{
	memset(&test_cb, 0, sizeof(test_cb));
	test_cb.magic = APP_CALLBACK_MAGIC;
	test_cb.version = APP_CALLBACK_VERSION;
	test_cb.init = mock_init;
	test_cb.on_msg_received = mock_on_msg_received;
}

static void reset_all(void)
{
	mock_on_msg_received_count = 0;
	mock_on_msg_received_len = 0;
	mock_init_count = 0;
	mock_ota_reset();
	memset(mock_on_msg_received_data, 0, sizeof(mock_on_msg_received_data));
}

/* ================================================================== */
/*  Test 1: valid magic + version → app callbacks invoked               */
/* ================================================================== */

static void test_valid_magic_version_loads_app(void)
{
	reset_all();
	setup_valid_cb();
	test_app_cb_addr = &test_cb;

	discover_app_image();

	assert(app_image_valid());
	assert(app_get_callbacks() == &test_cb);
	assert(app_get_reject_reason() == NULL);
}

/* ================================================================== */
/*  Test 2: wrong magic → app not loaded, platform boots standalone     */
/* ================================================================== */

static void test_wrong_magic_rejects_app(void)
{
	reset_all();
	setup_valid_cb();
	test_cb.magic = 0xDEADBEEF;
	test_app_cb_addr = &test_cb;

	discover_app_image();

	assert(!app_image_valid());
	assert(app_get_callbacks() == NULL);
	assert(app_get_reject_reason() != NULL);
	assert(strcmp(app_get_reject_reason(), "bad magic") == 0);
}

/* ================================================================== */
/*  Test 3: wrong version → app not loaded                              */
/* ================================================================== */

static void test_wrong_version_rejects_app(void)
{
	reset_all();
	setup_valid_cb();
	test_cb.version = APP_CALLBACK_VERSION + 1;
	test_app_cb_addr = &test_cb;

	discover_app_image();

	assert(!app_image_valid());
	assert(app_get_callbacks() == NULL);
	assert(app_get_reject_reason() != NULL);
	assert(strcmp(app_get_reject_reason(), "version mismatch") == 0);
}

/* ================================================================== */
/*  Test 4: OTA message (cmd 0x20) routed to OTA engine, not app        */
/* ================================================================== */

static void test_ota_message_routed_to_ota_engine(void)
{
	reset_all();
	setup_valid_cb();
	test_app_cb_addr = &test_cb;
	discover_app_image();
	assert(app_image_valid());

	uint8_t ota_msg[] = {0x20, 0x01, 0x02, 0x03};
	app_route_message(ota_msg, sizeof(ota_msg));

	assert(mock_ota_process_msg_called == 1);
	assert(mock_on_msg_received_count == 0);
}

/* ================================================================== */
/*  Test 5: non-OTA message routed to app_cb->on_msg_received           */
/* ================================================================== */

static void test_non_ota_message_routed_to_app(void)
{
	reset_all();
	setup_valid_cb();
	test_app_cb_addr = &test_cb;
	discover_app_image();
	assert(app_image_valid());

	uint8_t app_msg[] = {0x10, 0x01, 0x02};
	app_route_message(app_msg, sizeof(app_msg));

	assert(mock_on_msg_received_count == 1);
	assert(mock_on_msg_received_len == sizeof(app_msg));
	assert(mock_on_msg_received_data[0] == 0x10);
	assert(mock_ota_process_msg_called == 0);
}

/* ================================================================== */
/*  Test 6: app_cb NULL → messages handled safely (no crash)            */
/* ================================================================== */

static void test_null_app_cb_message_safety(void)
{
	reset_all();
	/* Set up an invalid image so app_cb is NULL */
	setup_valid_cb();
	test_cb.magic = 0xBAD00000;
	test_app_cb_addr = &test_cb;
	discover_app_image();
	assert(!app_image_valid());

	/* Non-OTA message with NULL app_cb — must not crash */
	uint8_t msg[] = {0x10, 0x01};
	app_route_message(msg, sizeof(msg));

	assert(mock_on_msg_received_count == 0);
	assert(mock_ota_process_msg_called == 0);

	/* OTA message with NULL app_cb — OTA engine still receives it */
	uint8_t ota_msg[] = {0x20, 0x01};
	app_route_message(ota_msg, sizeof(ota_msg));

	assert(mock_ota_process_msg_called == 1);
	assert(mock_on_msg_received_count == 0);

	/* Empty message — must not crash */
	app_route_message(msg, 0);
	assert(mock_ota_process_msg_called == 1);
	assert(mock_on_msg_received_count == 0);
}

/* ================================================================== */
/*  Test 7: timer interval bounds (< 100ms rejected, > 300000ms rej.)   */
/* ================================================================== */

static void test_timer_interval_bounds(void)
{
	/* Below minimum */
	assert(app_set_timer_interval(0) == -1);
	assert(app_set_timer_interval(1) == -1);
	assert(app_set_timer_interval(99) == -1);

	/* At minimum boundary */
	assert(app_set_timer_interval(100) == 0);

	/* Normal values */
	assert(app_set_timer_interval(1000) == 0);
	assert(app_set_timer_interval(60000) == 0);
	assert(app_set_timer_interval(150000) == 0);

	/* At maximum boundary */
	assert(app_set_timer_interval(300000) == 0);

	/* Above maximum */
	assert(app_set_timer_interval(300001) == -1);
	assert(app_set_timer_interval(999999) == -1);
	assert(app_set_timer_interval(0xFFFFFFFF) == -1);
}

/* ================================================================== */
/*  Additional edge-case tests                                          */
/* ================================================================== */

static void test_version_zero_mismatch(void)
{
	reset_all();
	setup_valid_cb();
	test_cb.version = 0;
	test_app_cb_addr = &test_cb;

	discover_app_image();

	assert(!app_image_valid());
	assert(strcmp(app_get_reject_reason(), "version mismatch") == 0);
}

static void test_rediscover_clears_previous_state(void)
{
	reset_all();

	/* First: valid image */
	setup_valid_cb();
	test_app_cb_addr = &test_cb;
	discover_app_image();
	assert(app_image_valid());

	/* Second: invalid magic — should clear previous valid state */
	test_cb.magic = 0x00000000;
	discover_app_image();
	assert(!app_image_valid());
	assert(app_get_callbacks() == NULL);
	assert(strcmp(app_get_reject_reason(), "bad magic") == 0);
}

static void test_null_on_msg_received_callback(void)
{
	reset_all();
	setup_valid_cb();
	test_cb.on_msg_received = NULL;  /* valid image but NULL callback */
	test_app_cb_addr = &test_cb;
	discover_app_image();
	assert(app_image_valid());

	/* Non-OTA message with NULL on_msg_received — must not crash */
	uint8_t msg[] = {0x10, 0x01};
	app_route_message(msg, sizeof(msg));

	assert(mock_on_msg_received_count == 0);
	assert(mock_ota_process_msg_called == 0);
}

/* ================================================================== */
/*  Main                                                                */
/* ================================================================== */

int main(void)
{
	printf("\n=== Boot Path Tests ===\n\n");

	printf("--- App Image Discovery ---\n");
	RUN_TEST(test_valid_magic_version_loads_app);
	RUN_TEST(test_wrong_magic_rejects_app);
	RUN_TEST(test_wrong_version_rejects_app);
	RUN_TEST(test_version_zero_mismatch);
	RUN_TEST(test_rediscover_clears_previous_state);

	printf("\n--- Message Routing ---\n");
	RUN_TEST(test_ota_message_routed_to_ota_engine);
	RUN_TEST(test_non_ota_message_routed_to_app);
	RUN_TEST(test_null_app_cb_message_safety);
	RUN_TEST(test_null_on_msg_received_callback);

	printf("\n--- Timer Interval Bounds ---\n");
	RUN_TEST(test_timer_interval_bounds);

	printf("\n%d/%d tests passed\n\n", tests_passed, tests_run);
	return (tests_passed == tests_run) ? 0 : 1;
}
