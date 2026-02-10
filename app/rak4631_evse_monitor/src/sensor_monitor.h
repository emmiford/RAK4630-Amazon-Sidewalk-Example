/*
 * Sensor Monitor — event-driven change detection (platform side)
 *
 * Monitors thermostat GPIOs via edge interrupts and J1772 pilot / current
 * clamp via 500ms ADC poll.  Calls app_cb->on_sensor_change(source) when
 * a state transition is detected.
 */

#ifndef SENSOR_MONITOR_H
#define SENSOR_MONITOR_H

#include <platform_api.h>

/* Start monitoring.  Requires GPIO init already done (platform_api_impl).
 * cb must remain valid for the lifetime of monitoring. */
int sensor_monitor_init(const struct app_callbacks *cb);

/* Stop all monitoring (timers + interrupts). */
void sensor_monitor_stop(void);

#endif /* SENSOR_MONITOR_H */
