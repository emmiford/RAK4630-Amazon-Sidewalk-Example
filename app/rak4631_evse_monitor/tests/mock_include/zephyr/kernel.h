/*
 * Mock zephyr/kernel.h — minimal stubs for host-side compilation of app.c
 */

#ifndef MOCK_ZEPHYR_KERNEL_H
#define MOCK_ZEPHYR_KERNEL_H

#include <stdint.h>

struct k_timer { int _unused; };
struct k_work { int _unused; };

#define K_TIMER_DEFINE(name, expiry_fn, stop_fn) \
	struct k_timer name = {0}

#define K_WORK_DEFINE(name, handler) \
	struct k_work name = {0}

#define K_MSEC(x) ((uint32_t)(x))
#define ARG_UNUSED(x) (void)(x)

static inline void k_timer_start(struct k_timer *t, uint32_t d, uint32_t p)
{
	(void)t; (void)d; (void)p;
}

static inline void k_timer_stop(struct k_timer *t)
{
	(void)t;
}

static inline int k_work_submit(struct k_work *w)
{
	(void)w;
	return 0;
}

#endif /* MOCK_ZEPHYR_KERNEL_H */
