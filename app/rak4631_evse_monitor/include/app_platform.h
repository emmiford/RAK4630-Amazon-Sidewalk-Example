/*
 * Shared platform API pointer — set once by app_init(), used by all app modules.
 */

#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#include <platform_api.h>

extern const struct platform_api *platform;

/* Convenience logging macros — NULL-safe, match Zephyr LOG_* naming */
#define LOG_INF(...) do { if (platform) platform->log_inf(__VA_ARGS__); } while (0)
#define LOG_WRN(...) do { if (platform) platform->log_wrn(__VA_ARGS__); } while (0)
#define LOG_ERR(...) do { if (platform) platform->log_err(__VA_ARGS__); } while (0)

#endif /* APP_PLATFORM_H */
