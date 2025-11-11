/**
 * @file kmod_hwmon.h
 * @brief Header for hwmon temperature sensor kernel module
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 *
 * This header defines the interface for the hwmon kernel module
 * that integrates Quectel RM520N temperature monitoring with the
 * Linux hwmon subsystem.
 */

#ifndef KMOD_HWMON_H
#define KMOD_HWMON_H

#ifdef __KERNEL__
#include <linux/hwmon.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#endif

/**
 * Hwmon integration features:
 * - Device name: quectel_rm520n_thermal
 * - Attributes exported via standard hwmon interface:
 *   * temp1_input (r)  - Current temperature in m°C
 *   * temp1_min   (rw) - Minimum threshold in m°C
 *   * temp1_max   (rw) - Maximum threshold in m°C
 *   * temp1_crit  (rw) - Critical threshold in m°C
 *
 * Synchronization:
 * - Reads thresholds from /sys/kernel/quectel_rm520n_thermal/ on probe
 * - Uses mutex for thread-safe operations
 * - Supports Device Tree and fallback platform device
 */

#ifdef __KERNEL__

/**
 * struct quectel_hwmon_data - Private data for hwmon device
 * @lock: Mutex for thread-safe access
 * @temp: Current temperature in millidegrees Celsius
 * @temp_min: Minimum temperature threshold in m°C
 * @temp_max: Maximum temperature threshold in m°C
 * @temp_crit: Critical temperature threshold in m°C
 *
 * This structure holds the runtime state of the hwmon device.
 */
struct quectel_hwmon_data {
	struct mutex lock;
	int temp;
	int temp_min;
	int temp_max;
	int temp_crit;
};

#endif /* __KERNEL__ */

/* Module does not export any public functions */

#endif /* KMOD_HWMON_H */
