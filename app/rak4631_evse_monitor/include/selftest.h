/*
 * Self-Test and Continuous Monitoring Interface
 *
 * Provides boot self-test, continuous fault monitoring, and an
 * on-demand shell command for commissioning verification.
 *
 * Fault flags are OR'd into uplink byte 7 (bits 4-7), coexisting
 * with thermostat flags in bits 0-1.
 */

#ifndef SELFTEST_H
#define SELFTEST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct platform_api;  /* forward declaration */

/* Fault flag bits for uplink byte 7, bits 4-7 */
#define FAULT_SENSOR     0x10   /* ADC/GPIO read failure or pilot out-of-range */
#define FAULT_CLAMP      0x20   /* Current vs. J1772 state disagreement */
#define FAULT_INTERLOCK  0x40   /* Charge enable ineffective / relay stuck */
#define FAULT_SELFTEST   0x80   /* Boot self-test failure (latched) */

typedef struct {
    bool adc_pilot_ok;
    bool adc_current_ok;
    bool gpio_cool_ok;
    bool charge_en_ok;
    bool all_pass;
} selftest_boot_result_t;

void    selftest_set_api(const struct platform_api *api);
void    selftest_reset(void);                           /* clear all state (for testing) */
int     selftest_boot(selftest_boot_result_t *result);  /* <100ms, returns 0=pass -1=fail */
void    selftest_continuous_tick(uint8_t j1772_state, uint16_t pilot_mv,
            uint16_t current_ma, bool charge_allowed,
            uint8_t thermostat_flags);
uint8_t selftest_get_fault_flags(void);                 /* OR into uplink byte 7 */
int     selftest_run_shell(void (*print)(const char *, ...),
            void (*error)(const char *, ...));

#ifdef __cplusplus
}
#endif

#endif /* SELFTEST_H */
