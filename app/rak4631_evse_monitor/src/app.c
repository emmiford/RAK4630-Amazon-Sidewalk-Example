/*
 * Platform-side App Loader — Boot Sequence
 *
 * Discovers the app callback table at 0x90000, initializes OTA,
 * configures Sidewalk, and starts the periodic timer.
 */

#include <sidewalk.h>
#include <app.h>
#include <app_leds.h>
#include <app_ble_config.h>
#include <sidewalk_dfu/nordic_dfu.h>
#include <app_subGHz_config.h>
#include <platform_api.h>
#include <ota_update.h>
#include <sidewalk_dispatch.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#ifdef CONFIG_SIDEWALK_FILE_TRANSFER_DFU
#include <sbdt/dfu_file_transfer.h>
#endif

LOG_MODULE_REGISTER(app, CONFIG_SIDEWALK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/*  External: platform API table (defined in platform_api_impl.c)      */
/* ------------------------------------------------------------------ */

extern const struct platform_api platform_api_table;

/* ------------------------------------------------------------------ */
/*  App callback table discovery                                       */
/* ------------------------------------------------------------------ */

static const struct app_callbacks *app_cb;
static const char *app_reject_reason;  /* NULL = loaded OK or not checked yet */

bool app_image_valid(void)
{
	return (app_cb != NULL);
}

const struct app_callbacks *app_get_callbacks(void)
{
	return app_cb;
}

const char *app_get_reject_reason(void)
{
	return app_reject_reason;
}

static void discover_app_image(void)
{
	const struct app_callbacks *cb =
		(const struct app_callbacks *)APP_CALLBACKS_ADDR;

	if (cb->magic != APP_CALLBACK_MAGIC) {
		LOG_ERR("No valid app image at 0x%08x (magic=0x%08x, expected=0x%08x)",
			APP_CALLBACKS_ADDR, cb->magic, APP_CALLBACK_MAGIC);
		app_cb = NULL;
		app_reject_reason = "bad magic";
		return;
	}

	if (cb->version != APP_CALLBACK_VERSION) {
		/* ADR-001: Hard stop on version mismatch.
		 *
		 * Originally this was a warning (forward-compatible by convention)
		 * to allow iterating on platform or app independently. Changed to
		 * hard stop because mismatched function pointer tables cause hard
		 * faults or silent memory corruption on bare metal.
		 *
		 * Version should ONLY be bumped when the table layout changes
		 * (add/remove/reorder pointers), not on every build. */
		LOG_ERR("App API version mismatch (app=%u, platform=%u) — refusing to load. "
			"Mismatched function pointer tables cause hard faults.",
			cb->version, APP_CALLBACK_VERSION);
		app_cb = NULL;
		app_reject_reason = "version mismatch";
		return;
	}

	app_cb = cb;
	app_reject_reason = NULL;
	LOG_INF("App image found at 0x%08x (version %u)", APP_CALLBACKS_ADDR, cb->version);
}

/* ------------------------------------------------------------------ */
/*  Timer — periodic sensor/TX tick                                    */
/* ------------------------------------------------------------------ */

#define NOTIFY_TIMER_INITIAL_MS (10000)
#define NOTIFY_TIMER_DEFAULT_MS (60000)

static uint32_t timer_interval_ms;  /* 0 = use default */

static void notify_timer_cb(struct k_timer *timer_id);
K_TIMER_DEFINE(notify_timer, notify_timer_cb, NULL);

static void timer_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	if (app_image_valid() && app_cb->on_timer) {
		app_cb->on_timer();
	}
}
K_WORK_DEFINE(timer_work, timer_work_handler);

static void notify_timer_cb(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	k_work_submit(&timer_work);
}

int app_set_timer_interval(uint32_t interval_ms)
{
	if (interval_ms < 100 || interval_ms > 300000) {
		return -1;
	}
	timer_interval_ms = interval_ms;
	LOG_INF("Timer interval set to %u ms", interval_ms);
	return 0;
}

/* ------------------------------------------------------------------ */
/*  App start — boot sequence                                          */
/* ------------------------------------------------------------------ */

static sidewalk_ctx_t sid_ctx;

static void prepare_for_ota_apply(void)
{
	LOG_WRN("OTA: stopping app callbacks for apply");
	k_timer_stop(&notify_timer);
	app_cb = NULL;
}

void app_start(void)
{
	LOG_INF("=== PLATFORM START ===");

	if (app_led_init()) {
		LOG_ERR("Cannot init leds");
	}

	/* Initialize OTA module and check for interrupted apply */
	ota_init(platform_api_table.send_msg);
	ota_set_pre_apply_hook(prepare_for_ota_apply);
	if (ota_boot_recovery_check()) {
		/* Recovery in progress — will reboot when done */
		return;
	}

	/* Discover app image */
	discover_app_image();

	/* Initialize app if present */
	if (app_image_valid() && app_cb->init) {
		int err = app_cb->init(&platform_api_table);
		if (err) {
			LOG_ERR("App init failed: %d", err);
			app_cb = NULL;
		} else {
			LOG_INF("App loaded and initialized");
		}
	} else {
		LOG_WRN("Running in platform-only mode (no app image)");
	}

	/* Configure Sidewalk */
	static struct sid_event_callbacks event_callbacks;
	sidewalk_dispatch_fill_callbacks(&event_callbacks, &sid_ctx);

	struct sid_end_device_characteristics dev_ch = {
		.type = SID_END_DEVICE_TYPE_STATIC,
		.power_type = SID_END_DEVICE_POWERED_BY_BATTERY_AND_LINE_POWER,
		.qualification_id = 0x0001,
	};

	sid_ctx.config = (struct sid_config){
		.link_mask = SID_LINK_TYPE_1 | SID_LINK_TYPE_3,
		.dev_ch = dev_ch,
		.callbacks = &event_callbacks,
		.link_config = app_get_ble_config(),
		.sub_ghz_link_config = app_get_sub_ghz_config(),
	};

	int err = sidewalk_dispatch_register_gatt_auth();
	if (err) {
		LOG_ERR("Registering GATT authorization callbacks failed (err %d)", err);
		return;
	}

	/* Start Sidewalk */
	sidewalk_start(&sid_ctx);
	sidewalk_event_send(sidewalk_event_platform_init, NULL, NULL);
	sidewalk_event_send(sidewalk_event_autostart, NULL, NULL);

	/* Start periodic timer — interval configurable via app's set_timer_interval() */
	uint32_t interval = timer_interval_ms ? timer_interval_ms : NOTIFY_TIMER_DEFAULT_MS;
	LOG_INF("Starting app timer (10s delay, %ums period)", interval);
	k_timer_start(&notify_timer, K_MSEC(NOTIFY_TIMER_INITIAL_MS), K_MSEC(interval));
}
