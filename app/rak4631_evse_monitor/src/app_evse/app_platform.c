/*
 * Shared platform API pointer — set once by app_init(), used by all app modules.
 */

#include <app_platform.h>

const struct platform_api *platform = NULL;
