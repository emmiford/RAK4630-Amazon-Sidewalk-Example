/*
 * Platform TX State
 *
 * Tracks Sidewalk ready state and link mask. The actual sensor
 * reading and payload building is done by the app image via
 * the platform API send_msg() function.
 */

#include <tx_state.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(tx_state, CONFIG_SIDEWALK_LOG_LEVEL);

static bool sidewalk_ready = false;
static uint32_t last_link_mask = 0;

void tx_state_set_ready(bool ready)
{
	sidewalk_ready = ready;
	LOG_INF("Sidewalk %s", ready ? "READY" : "NOT READY");
}

void tx_state_set_link_mask(uint32_t link_mask)
{
	if (link_mask) {
		last_link_mask = link_mask;
	}
}

bool tx_state_is_ready(void)
{
	return sidewalk_ready;
}

uint32_t tx_state_get_link_mask(void)
{
	return last_link_mask;
}

int tx_state_send_evse_data(void)
{
	/* In split-image architecture, this is handled by the app's on_timer callback.
	 * This stub exists for platform-only mode (no app image). */
	if (!sidewalk_ready) {
		LOG_WRN("Sidewalk not ready, skipping send");
		return -1;
	}
	LOG_WRN("tx_state_send_evse_data called in platform-only mode (no-op)");
	return 0;
}
