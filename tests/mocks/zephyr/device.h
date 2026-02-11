/* Mock Zephyr device.h for host-side OTA tests */
#ifndef ZEPHYR_DEVICE_H_MOCK
#define ZEPHYR_DEVICE_H_MOCK

#include <stdbool.h>

struct device {
	const char *name;
};

/* Singleton mock flash device */
extern struct device mock_flash_device;

#define DEVICE_DT_GET(node) (&mock_flash_device)
#define DT_CHOSEN(name) 0

static inline bool device_is_ready(const struct device *dev)
{
	return (dev != NULL);
}

#endif /* ZEPHYR_DEVICE_H_MOCK */
