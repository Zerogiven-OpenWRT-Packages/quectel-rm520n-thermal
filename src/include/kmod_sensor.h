/**
 * @file kmod_sensor.h
 * @brief Header for virtual thermal sensor kernel module
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 *
 * This header defines the interface for the thermal sensor kernel module
 * that registers a virtual thermal zone with the Linux Thermal Framework.
 */

#ifndef KMOD_SENSOR_H
#define KMOD_SENSOR_H

#ifdef __KERNEL__
#include <linux/thermal.h>
#include <linux/platform_device.h>
#endif

/**
 * Thermal zone integration features:
 * - Registers virtual thermal zone with Linux Thermal Framework
 * - Provides temperature data to thermal subsystem
 * - Supports system-wide thermal management and fan control
 * - Device Tree compatible with fallback platform device support
 *
 * Thermal Zone Operations:
 * - get_temp: Read current temperature from sysfs
 * - get_trip_type: Report trip point types (passive, hot, critical)
 * - get_trip_temp: Report trip point temperatures
 * - set_trip_temp: Update trip point temperatures (configurable)
 *
 * Platform device path:
 * - /sys/devices/platform/soc/soc:quectel-temp-sensor/
 */

#ifdef __KERNEL__

/**
 * struct quectel_sensor_data - Private data for thermal sensor
 * @dev: Pointer to platform device
 * @tz: Pointer to thermal zone device
 *
 * This structure holds the runtime state of the thermal sensor.
 */
struct quectel_sensor_data {
	struct device *dev;
	struct thermal_zone_device *tz;
};

#endif /* __KERNEL__ */

/* Module does not export any public functions */

#endif /* KMOD_SENSOR_H */
