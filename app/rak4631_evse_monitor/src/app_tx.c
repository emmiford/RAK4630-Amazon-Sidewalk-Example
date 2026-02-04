/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <app_tx.h>
#include <sidewalk.h>
#include <sid_hal_memory_ifc.h>
#include <rak_sidewalk.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_tx, CONFIG_SIDEWALK_LOG_LEVEL);

/* EVSE payload format constants */
#define EVSE_MAGIC   0xE5
#define EVSE_VERSION 0x01
#define EVSE_PAYLOAD_SIZE 8

/* Sidewalk message configuration */
#define APP_SID_MSG_TTL_MAX 60
#define APP_SID_MSG_RETRIES_MAX 3

/* Sidewalk ready state */
static bool sidewalk_ready = false;
static uint32_t last_link_mask = 0;

/**
 * @brief Free sidewalk message context
 */
static void free_sid_msg_ctx(void *ctx)
{
	sidewalk_msg_t *sid_msg = (sidewalk_msg_t *)ctx;
	if (sid_msg == NULL) {
		return;
	}
	if (sid_msg->msg.data) {
		sid_hal_free(sid_msg->msg.data);
	}
	sid_hal_free(sid_msg);
}

/**
 * @brief Send raw payload over Sidewalk
 */
static int send_raw_sidewalk_msg(const uint8_t *payload, size_t len)
{
	/* Allocate message context */
	sidewalk_msg_t *sid_msg = sid_hal_malloc(sizeof(sidewalk_msg_t));
	if (!sid_msg) {
		LOG_ERR("Failed to alloc message context");
		return -ENOMEM;
	}
	memset(sid_msg, 0, sizeof(*sid_msg));

	/* Allocate and copy payload data */
	sid_msg->msg.size = len;
	sid_msg->msg.data = sid_hal_malloc(len);
	if (!sid_msg->msg.data) {
		sid_hal_free(sid_msg);
		LOG_ERR("Failed to alloc message data");
		return -ENOMEM;
	}
	memcpy(sid_msg->msg.data, payload, len);

	/* Configure message descriptor */
	sid_msg->desc.link_type = last_link_mask;
	sid_msg->desc.type = SID_MSG_TYPE_NOTIFY;
	sid_msg->desc.link_mode = SID_LINK_MODE_CLOUD;
	sid_msg->desc.msg_desc_attr.tx_attr.ttl_in_seconds = APP_SID_MSG_TTL_MAX;
	sid_msg->desc.msg_desc_attr.tx_attr.num_retries = APP_SID_MSG_RETRIES_MAX;
	sid_msg->desc.msg_desc_attr.tx_attr.request_ack = true;

	/* Request BLE connection if using BLE link */
	if (last_link_mask & SID_LINK_TYPE_1) {
		sidewalk_event_send(sidewalk_event_connect, NULL, NULL);
	}

	/* Send the message */
	int err = sidewalk_event_send(sidewalk_event_send_msg, sid_msg, free_sid_msg_ctx);
	if (err) {
		free_sid_msg_ctx(sid_msg);
		LOG_ERR("Event send err %d", err);
		return -EIO;
	}

	return 0;
}

void app_tx_set_ready(bool ready)
{
	sidewalk_ready = ready;
	LOG_INF("Sidewalk %s", ready ? "READY" : "NOT READY");
}

void app_tx_set_link_mask(uint32_t link_mask)
{
	if (link_mask) {
		last_link_mask = link_mask;
	}
}

int app_tx_send_evse_data(void)
{
	if (!sidewalk_ready) {
		LOG_WRN("Sidewalk not ready, skipping send");
		return -ENODEV;
	}

	/* Read current sensor data */
	evse_payload_t data = rak_sidewalk_get_payload();

	/* Build 8-byte raw payload */
	uint8_t payload[EVSE_PAYLOAD_SIZE] = {
		EVSE_MAGIC,
		EVSE_VERSION,
		data.j1772_state,
		data.j1772_mv & 0xFF,
		(data.j1772_mv >> 8) & 0xFF,
		data.current_ma & 0xFF,
		(data.current_ma >> 8) & 0xFF,
		data.thermostat_flags
	};

	LOG_INF("EVSE TX: state=%d, pilot=%dmV, current=%dmA, therm=0x%02x",
		data.j1772_state, data.j1772_mv, data.current_ma, data.thermostat_flags);

	return send_raw_sidewalk_msg(payload, sizeof(payload));
}
