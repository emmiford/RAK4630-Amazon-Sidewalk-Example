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

#define APP_RX_PAYLOAD_MAX_SIZE 255

/* Used by platform to queue messages (platform-side only) */
struct app_rx_msg {
	uint8_t pld_size;
	uint8_t rx_payload[APP_RX_PAYLOAD_MAX_SIZE];
};

/* Called by app entry when platform delivers a message */
void app_rx_process_msg(const uint8_t *data, size_t len);


#ifdef __cplusplus
}
#endif

#endif /* APP_RX_H */
