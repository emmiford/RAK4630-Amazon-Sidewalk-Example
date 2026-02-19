/*
 * Mock sidewalk.h — minimal Sidewalk type stubs for host-side compilation
 */

#ifndef MOCK_SIDEWALK_H
#define MOCK_SIDEWALK_H

#include <stdint.h>
#include <stdbool.h>

/* Minimal type stubs — just enough for app.c to compile */

struct sid_event_callbacks {
	void *context;
	void (*on_event)(bool, void *);
	void (*on_msg_received)(const void *, const void *, void *);
	void (*on_msg_sent)(const void *, void *);
	void (*on_send_error)(int, const void *, void *);
	void (*on_status_changed)(const void *, void *);
	void (*on_factory_reset)(void *);
};

struct sid_end_device_characteristics {
	int type;
	int power_type;
	uint32_t qualification_id;
};

struct sid_config {
	uint32_t link_mask;
	struct sid_end_device_characteristics dev_ch;
	struct sid_event_callbacks *callbacks;
	void *link_config;
	void *sub_ghz_link_config;
};

typedef struct {
	struct sid_config config;
} sidewalk_ctx_t;

#define SID_LINK_TYPE_1  0x01
#define SID_LINK_TYPE_3  0x04

#define SID_END_DEVICE_TYPE_STATIC                       0
#define SID_END_DEVICE_POWERED_BY_BATTERY_AND_LINE_POWER 2

/* Function stubs — implementations in mock_boot.c */
void sidewalk_start(sidewalk_ctx_t *ctx);
void sidewalk_event_send(void (*handler)(), void *data, void (*free_fn)(void *));

/* Event handler functions (used as function pointers) */
void sidewalk_event_platform_init(void);
void sidewalk_event_autostart(void);
void sidewalk_event_process(void);

#endif /* MOCK_SIDEWALK_H */
