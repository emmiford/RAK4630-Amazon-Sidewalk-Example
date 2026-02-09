/*
 * App RX Interface
 */

#ifndef APP_RX_H
#define APP_RX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct platform_api;  /* forward declaration */

#define APP_RX_PAYLOAD_MAX_SIZE 255

/* Used by platform to queue messages (platform-side only) */
struct app_rx_msg {
	uint8_t pld_size;
	uint8_t rx_payload[APP_RX_PAYLOAD_MAX_SIZE];
};

void app_rx_set_api(const struct platform_api *platform);

/* Called by app entry when platform delivers a message */
void app_rx_process_msg(const uint8_t *data, size_t len);

/* Platform-side legacy (kept for platform app.c compatibility) */
int app_rx_msg_received(struct app_rx_msg *rx_msg);
void app_rx_task(void *dummy1, void *dummy2, void *dummy3);

#ifdef __cplusplus
}
#endif

#endif /* APP_RX_H */
