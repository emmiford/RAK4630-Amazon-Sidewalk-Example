/*
 * Platform API Interface — shared between platform and app images
 *
 * The platform image exposes a function table at a fixed flash address
 * (PLATFORM_API_ADDR).  The app image exposes a callback table at the
 * start of its flash partition (APP_CALLBACKS_ADDR).
 *
 * Both tables carry a magic word and a version number so that each side
 * can detect incompatible images at boot time.
 *
 * The platform is a generic Sidewalk sensor device runtime — it provides
 * connectivity, hardware access, timers, OTA, and shell infrastructure.
 * All application-specific logic (sensor interpretation, payload format,
 * domain knowledge) lives in the app image, which is OTA-updatable.
 */

#ifndef PLATFORM_API_H
#define PLATFORM_API_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Flash addresses                                                    */
/* ------------------------------------------------------------------ */

#define PLATFORM_API_ADDR       0x8FF00   /* last 256 bytes of 576KB platform partition */
#define APP_CALLBACKS_ADDR      0x90000   /* start of app partition */

/* ------------------------------------------------------------------ */
/*  Platform API table (provided by platform at PLATFORM_API_ADDR)    */
/* ------------------------------------------------------------------ */

#define PLATFORM_API_MAGIC      0x504C4154  /* "PLAT" */
#define PLATFORM_API_VERSION    3

struct platform_api {
    uint32_t magic;
    uint32_t version;

    /* --- Sidewalk --- */
    int   (*send_msg)(const uint8_t *data, size_t len);
    bool  (*is_ready)(void);
    int   (*get_link_mask)(void);
    int   (*set_link_mask)(uint32_t mask);
    int   (*factory_reset)(void);

    /* --- Hardware --- */
    int   (*adc_read_mv)(int channel);          /* returns millivolts, <0 on error */
    int   (*gpio_get)(int pin_index);           /* returns 0/1, <0 on error */
    int   (*gpio_set)(int pin_index, int val);  /* returns 0 on success */
    void  (*led_set)(int led_id, bool on);      /* control board LEDs (0-3) */

    /* --- System --- */
    uint32_t (*uptime_ms)(void);
    void     (*reboot)(void);

    /* --- Timer --- */
    int   (*set_timer_interval)(uint32_t interval_ms);  /* configure on_timer period */

    /* --- Logging (variadic, printf-style) --- */
    void (*log_inf)(const char *fmt, ...);
    void (*log_err)(const char *fmt, ...);
    void (*log_wrn)(const char *fmt, ...);

    /* --- Shell output (used inside on_shell_cmd) --- */
    void (*shell_print)(const char *fmt, ...);
    void (*shell_error)(const char *fmt, ...);

    /* --- Memory --- */
    void *(*malloc)(size_t size);
    void  (*free)(void *ptr);

    /* --- MFG diagnostics --- */
    uint32_t (*mfg_get_version)(void);
    bool     (*mfg_get_dev_id)(uint8_t *id_out);
};

/* ------------------------------------------------------------------ */
/*  App callback table (provided by app at APP_CALLBACKS_ADDR)        */
/* ------------------------------------------------------------------ */

#define APP_CALLBACK_MAGIC      0x53415050  /* "SAPP" (Sidewalk App) */
#define APP_CALLBACK_VERSION    3

struct app_callbacks {
    uint32_t magic;
    uint32_t version;

    /* Lifecycle */
    int   (*init)(const struct platform_api *api);
    void  (*on_ready)(bool ready);

    /* Messages */
    void  (*on_msg_received)(const uint8_t *data, size_t len);
    void  (*on_msg_sent)(uint32_t msg_id);
    void  (*on_send_error)(uint32_t msg_id, int error);

    /* Periodic timer — app configures interval via set_timer_interval() */
    void  (*on_timer)(void);

    /* Shell command dispatch */
    int   (*on_shell_cmd)(const char *cmd, const char *args,
                          void (*print)(const char *fmt, ...),
                          void (*error)(const char *fmt, ...));
};

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_API_H */
