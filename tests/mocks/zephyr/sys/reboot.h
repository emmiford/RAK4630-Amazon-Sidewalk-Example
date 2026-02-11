/* Mock Zephyr sys_reboot for host-side tests */
#ifndef ZEPHYR_REBOOT_H_MOCK
#define ZEPHYR_REBOOT_H_MOCK

#define SYS_REBOOT_WARM  0
#define SYS_REBOOT_COLD  1

extern int mock_reboot_count;

static inline void sys_reboot(int type)
{
	(void)type;
	mock_reboot_count++;
	/* Don't actually reboot in tests! */
}

#endif /* ZEPHYR_REBOOT_H_MOCK */
