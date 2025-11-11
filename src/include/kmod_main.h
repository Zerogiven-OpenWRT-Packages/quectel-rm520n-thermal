/**
 * @file kmod_main.h
 * @brief Header for sysfs temperature interface kernel module
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 *
 * This header defines the interface for the main sysfs kernel module
 * that provides /sys/kernel/quectel_rm520n_thermal/ interface.
 */

#ifndef KMOD_MAIN_H
#define KMOD_MAIN_H

#ifdef __KERNEL__
#include <linux/kobject.h>
#include <linux/sysfs.h>
#endif

/**
 * Sysfs paths exported by this module:
 * - /sys/kernel/quectel_rm520n_thermal/temp        (rw) - Current temperature in m°C
 * - /sys/kernel/quectel_rm520n_thermal/temp_min    (rw) - Minimum threshold in m°C
 * - /sys/kernel/quectel_rm520n_thermal/temp_max    (rw) - Maximum threshold in m°C
 * - /sys/kernel/quectel_rm520n_thermal/temp_crit   (rw) - Critical threshold in m°C
 * - /sys/kernel/quectel_rm520n_thermal/temp_default (rw) - Default temperature in m°C
 * - /sys/kernel/quectel_rm520n_thermal/stats       (r)  - Statistics (total_updates, last_update_time)
 */

/* Module does not export any public functions */

#endif /* KMOD_MAIN_H */
