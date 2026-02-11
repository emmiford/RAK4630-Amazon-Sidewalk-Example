/* Mock Zephyr logging for host-side tests */
#ifndef ZEPHYR_LOG_H_MOCK
#define ZEPHYR_LOG_H_MOCK

#define CONFIG_SIDEWALK_LOG_LEVEL 4

#define LOG_MODULE_REGISTER(name, level)

#define LOG_INF(...) do { } while (0)
#define LOG_ERR(...) do { } while (0)
#define LOG_WRN(...) do { } while (0)
#define LOG_DBG(...) do { } while (0)
#define LOG_PANIC()  do { } while (0)

#endif /* ZEPHYR_LOG_H_MOCK */
