/*
 * Platform TX State
 *
 * Tracks Sidewalk ready state and link mask on the platform side.
 * The actual payload building and sending is done by the app image
 * via the platform API.
 */

#ifndef TX_STATE_H
#define TX_STATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void tx_state_set_ready(bool ready);
bool tx_state_is_ready(void);
void tx_state_set_link_mask(uint32_t link_mask);
uint32_t tx_state_get_link_mask(void);
int tx_state_send_evse_data(void);

#ifdef __cplusplus
}
#endif

#endif /* TX_STATE_H */
