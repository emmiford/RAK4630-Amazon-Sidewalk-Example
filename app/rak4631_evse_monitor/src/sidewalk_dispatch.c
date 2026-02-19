/*
 * Sidewalk Event Dispatch
 *
 * Routes Sidewalk events to the app callback table and platform
 * subsystems (OTA, TX state).  Also handles BLE GATT authorization.
 */

#include <sidewalk.h>
#include <tx_state.h>
#include <app.h>
#include <platform_api.h>
#include <ota_update.h>
#include <sid_hal_reset_ifc.h>
#include <sid_hal_memory_ifc.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <bt_app_callbacks.h>
#include <json_printer/sidTypes2str.h>

LOG_MODULE_REGISTER(sidewalk_dispatch, CONFIG_SIDEWALK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/*  Sidewalk event callbacks                                           */
/* ------------------------------------------------------------------ */

static void on_sidewalk_event(bool in_isr, void *context)
{
	int err = sidewalk_event_send(sidewalk_event_process, NULL, NULL);
	if (err) {
		LOG_ERR("Send event err %d", err);
	}
}

static void on_sidewalk_msg_received(const struct sid_msg_desc *msg_desc,
				     const struct sid_msg *msg, void *context)
{
	LOG_DBG("Received message(type: %d, link_mode: %d, id: %u size %u)",
		(int)msg_desc->type, (int)msg_desc->link_mode, msg_desc->id, msg->size);
	LOG_HEXDUMP_INF((uint8_t *)msg->data, msg->size, "Received message: ");

	if (msg_desc->type == SID_MSG_TYPE_RESPONSE &&
	    msg_desc->msg_desc_attr.rx_attr.is_msg_ack) {
		LOG_DBG("Received Ack for msg id %d", msg_desc->id);
	} else {
		app_route_message((const uint8_t *)msg->data, msg->size);
	}
}

static void on_sidewalk_msg_sent(const struct sid_msg_desc *msg_desc, void *context)
{
	LOG_DBG("sent message(type: %d, id: %u)", (int)msg_desc->type, msg_desc->id);
	if (app_image_valid()) {
		const struct app_callbacks *cb = app_get_callbacks();
		if (cb->on_msg_sent) {
			cb->on_msg_sent(msg_desc->id);
		}
	}
}

static void on_sidewalk_send_error(sid_error_t error, const struct sid_msg_desc *msg_desc,
				   void *context)
{
	LOG_ERR("Send message err %d (%s)", (int)error, SID_ERROR_T_STR(error));
	if (app_image_valid()) {
		const struct app_callbacks *cb = app_get_callbacks();
		if (cb->on_send_error) {
			cb->on_send_error(msg_desc->id, (int)error);
		}
	}
}

static void on_sidewalk_factory_reset(void *context)
{
	ARG_UNUSED(context);
	LOG_INF("Factory reset notification received from sid api");
	if (sid_hal_reset(SID_HAL_RESET_NORMAL)) {
		LOG_WRN("Cannot reboot");
	}
}

static void on_sidewalk_status_changed(const struct sid_status *status, void *context)
{
	struct sid_status *new_status = sid_hal_malloc(sizeof(struct sid_status));
	if (!new_status) {
		LOG_ERR("Failed to allocate memory for new status value");
	} else {
		memcpy(new_status, status, sizeof(struct sid_status));
	}
	sidewalk_event_send(sidewalk_event_new_status, new_status, sid_hal_free);

	/* Update platform TX module */
	tx_state_set_link_mask(status->detail.link_status_mask);

	/* Determine ready state */
	bool ready = false;
	switch (status->state) {
	case SID_STATE_READY:
	case SID_STATE_SECURE_CHANNEL_READY:
		ready = true;
		break;
	default:
		break;
	}

	tx_state_set_ready(ready);

	/* Notify app */
	if (app_image_valid()) {
		const struct app_callbacks *cb = app_get_callbacks();
		if (cb->on_ready) {
			cb->on_ready(ready);
		}
	}

	LOG_INF("Device %sregistered, Time Sync %s, Link status: {BLE: %s, FSK: %s, LoRa: %s}",
		(SID_STATUS_REGISTERED == status->detail.registration_status) ? "Is " : "Un",
		(SID_STATUS_TIME_SYNCED == status->detail.time_sync_status) ? "Success" : "Fail",
		(status->detail.link_status_mask & SID_LINK_TYPE_1) ? "Up" : "Down",
		(status->detail.link_status_mask & SID_LINK_TYPE_2) ? "Up" : "Down",
		(status->detail.link_status_mask & SID_LINK_TYPE_3) ? "Up" : "Down");

	for (int i = 0; i < SID_LINK_TYPE_MAX_IDX; i++) {
		enum sid_link_mode mode =
			(enum sid_link_mode)status->detail.supported_link_modes[i];
		if (mode) {
			LOG_INF("Link mode on %s = {Cloud: %s, Mobile: %s}",
				(SID_LINK_TYPE_1_IDX == i) ? "BLE" :
				(SID_LINK_TYPE_2_IDX == i) ? "FSK" :
				(SID_LINK_TYPE_3_IDX == i) ? "LoRa" : "unknow",
				(mode & SID_LINK_MODE_CLOUD) ? "True" : "False",
				(mode & SID_LINK_MODE_MOBILE) ? "True" : "False");
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Public: fill callbacks struct                                      */
/* ------------------------------------------------------------------ */

void sidewalk_dispatch_fill_callbacks(struct sid_event_callbacks *cbs,
				      void *context)
{
	cbs->context = context;
	cbs->on_event = on_sidewalk_event;
	cbs->on_msg_received = on_sidewalk_msg_received;
	cbs->on_msg_sent = on_sidewalk_msg_sent;
	cbs->on_send_error = on_sidewalk_send_error;
	cbs->on_status_changed = on_sidewalk_status_changed;
	cbs->on_factory_reset = on_sidewalk_factory_reset;
}

/* ------------------------------------------------------------------ */
/*  BLE GATT authorization                                             */
/* ------------------------------------------------------------------ */

static bool gatt_authorize(struct bt_conn *conn, const struct bt_gatt_attr *attr)
{
	struct bt_conn_info cinfo = {};
	int ret = bt_conn_get_info(conn, &cinfo);
	if (ret != 0) {
		LOG_ERR("Failed to get id of connection err %d", ret);
		return false;
	}

	if (cinfo.id == BT_ID_SIDEWALK) {
		if (sid_ble_bt_attr_is_SMP(attr)) {
			return false;
		}
	}

#if defined(CONFIG_SIDEWALK_DFU)
	if (cinfo.id == BT_ID_SMP_DFU) {
		if (sid_ble_bt_attr_is_SIDEWALK(attr)) {
			return false;
		}
	}
#endif
	return true;
}

static const struct bt_gatt_authorization_cb gatt_authorization_callbacks = {
	.read_authorize = gatt_authorize,
	.write_authorize = gatt_authorize,
};

int sidewalk_dispatch_register_gatt_auth(void)
{
	return bt_gatt_authorization_cb_register(&gatt_authorization_callbacks);
}
