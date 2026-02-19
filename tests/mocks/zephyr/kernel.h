/* Mock Zephyr kernel.h for host-side tests */
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

/* k_timer stubs (used by app.c boot path) */
struct k_timer { int _unused; };

#define K_TIMER_DEFINE(name, expiry_fn, stop_fn) \
	struct k_timer name = {0}

static inline void k_timer_start(struct k_timer *t, uint32_t d, uint32_t p)
{
	(void)t; (void)d; (void)p;
}

static inline void k_timer_stop(struct k_timer *t)
{
	(void)t;
}

/* k_work stubs */
struct k_work { int dummy; };
struct k_work_delayable { struct k_work work; };

#define K_WORK_DEFINE(name, handler) \
	struct k_work name = {0}

#define K_WORK_DELAYABLE_DEFINE(name, fn) \
	struct k_work_delayable name = { .work = { .dummy = 0 } }

#define K_MSEC(ms) ((uint32_t)(ms))
#define K_SECONDS(s) ((s) * 1000)
#define ARG_UNUSED(x) (void)(x)

static inline int k_work_submit(struct k_work *w)
{
	(void)w;
	return 0;
}

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
