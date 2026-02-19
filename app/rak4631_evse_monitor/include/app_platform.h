/*
 * Shared platform API pointer — set once by app_init(), used by all app modules.
 */

#ifndef APP_PLATFORM_H
#define APP_PLATFORM_H

#include <platform_api.h>

extern const struct platform_api *platform;

#endif /* APP_PLATFORM_H */
