/*
 * Mock sidewalk_dispatch.h — stubs for host-side compilation
 */

#ifndef MOCK_SIDEWALK_DISPATCH_H
#define MOCK_SIDEWALK_DISPATCH_H

struct sid_event_callbacks;  /* forward declaration, full def in sidewalk.h */

void sidewalk_dispatch_fill_callbacks(struct sid_event_callbacks *cbs,
				      void *context);
int sidewalk_dispatch_register_gatt_auth(void);

#endif /* MOCK_SIDEWALK_DISPATCH_H */
