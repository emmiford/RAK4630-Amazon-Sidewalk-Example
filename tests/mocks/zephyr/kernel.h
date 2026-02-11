/* Mock Zephyr kernel.h for host-side OTA tests */
#ifndef ZEPHYR_KERNEL_H_MOCK
#define ZEPHYR_KERNEL_H_MOCK

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* errno codes used by OTA */
#ifndef ENODEV
#define ENODEV 19
#endif
#ifndef ENOENT
#define ENOENT 2
#endif
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif

/* k_work stubs */
struct k_work { int dummy; };
struct k_work_delayable { struct k_work work; };

#define K_WORK_DELAYABLE_DEFINE(name, fn) \
	struct k_work_delayable name = { .work = { .dummy = 0 } }

#define K_MSEC(ms) (ms)
#define K_SECONDS(s) ((s) * 1000)

static inline int k_work_schedule(struct k_work_delayable *w, int delay)
{
	(void)w; (void)delay;
	return 0;
}

static inline int k_work_cancel_delayable(struct k_work_delayable *w)
{
	(void)w;
	return 0;
}

#endif /* ZEPHYR_KERNEL_H_MOCK */
