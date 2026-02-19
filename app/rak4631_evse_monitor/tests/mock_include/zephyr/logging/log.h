/*
 * Mock zephyr/logging/log.h — LOG macros expand to nothing on host
 */

#ifndef MOCK_ZEPHYR_LOG_H
#define MOCK_ZEPHYR_LOG_H

#define LOG_MODULE_REGISTER(...)
#define LOG_ERR(...)
#define LOG_INF(...)
#define LOG_WRN(...)
#define LOG_DBG(...)
#define LOG_HEXDUMP_INF(...)

#ifndef CONFIG_SIDEWALK_LOG_LEVEL
#define CONFIG_SIDEWALK_LOG_LEVEL 0
#endif

#endif /* MOCK_ZEPHYR_LOG_H */
