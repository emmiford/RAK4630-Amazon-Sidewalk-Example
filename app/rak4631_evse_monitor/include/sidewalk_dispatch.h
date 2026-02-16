/*
 * Sidewalk Event Dispatch
 *
 * Sidewalk event handlers and BLE GATT authorization.
 * Called by app.c during boot to configure the Sidewalk stack.
 */

#ifndef SIDEWALK_DISPATCH_H
#define SIDEWALK_DISPATCH_H

#include <sid_api.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fill in Sidewalk event callbacks for sid_config.
 * @param cbs      Event callbacks struct to populate
 * @param context  Context pointer (typically &sid_ctx)
 */
void sidewalk_dispatch_fill_callbacks(struct sid_event_callbacks *cbs,
				      void *context);

/**
 * Register BLE GATT authorization callbacks.
 * @return 0 on success, negative errno on failure.
 */
int sidewalk_dispatch_register_gatt_auth(void);

#ifdef __cplusplus
}
#endif

#endif /* SIDEWALK_DISPATCH_H */
