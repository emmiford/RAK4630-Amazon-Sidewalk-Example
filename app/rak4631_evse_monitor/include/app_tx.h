/*
 * App TX Interface
 */

#ifndef APP_TX_H
#define APP_TX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct event_snapshot;   /* forward declaration */

void app_tx_init(void);
void app_tx_set_ready(bool ready);
int app_tx_send_evse_data(void);
int app_tx_send_snapshot(const struct event_snapshot *snap);
void app_tx_set_link_mask(uint32_t link_mask);
bool app_tx_is_ready(void);
uint32_t app_tx_get_link_mask(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TX_H */
