/*
 * quectel_rm520n_temp_sensor.c
 *
 * Virtual Thermal Sensor for Quectel RM520N using the new thermal API.
 * Registers a virtual sensor via the Thermal Framework and creates a sysfs
 * attribute "cur_temp" for manual updating via Userspace.
 *
 * Optional OF-Matching is enabled (compatible = "quectel-rm520n-temp").
 * If no matching DT node is found, the sensor will not be registered.
 *
 * Note: The virtual sensor is exposed via the Thermal Framework as a Thermal Zone
 * under /sys/class/thermal.
 *
 * Author: Christopher Sollinger
 * License: GPL
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/version.h>

#ifndef PKG_NAME
#define PKG_NAME "-"
#endif
#ifndef PKG_TAG
#define PKG_TAG "0.0.0-r0"
#endif
#ifndef PKG_MAINTAINER
#define PKG_MAINTAINER "-"
#endif
#ifndef PKG_LICENSE
#define PKG_LICENSE "-"
#endif
#ifndef PKG_COPYRIGHT_YEAR
#define PKG_COPYRIGHT_YEAR "2025"
#endif

/* 
 * The THERMAL_EVENT_UNSPECIFIED enum was introduced in kernel 5.17
 * For earlier kernels, we define it here to ensure compatibility
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0)
/* Use 0 as a safe default value for older kernels (OpenWRT 23.05 compatibility) */
#define THERMAL_EVENT_UNSPECIFIED 0
#endif

/* Data structure to store the current temperature (in m°C) */
struct quectel_temp_data {
    struct thermal_zone_device *tzd; /* Handle to the Thermal Zone */
    int cur_temp;                    /* Current temperature in milli-degrees Celsius (m°C) */
};

/* Callback to retrieve the current temperature */
static int quectel_temp_get_temp(struct thermal_zone_device *tzd, int *temp)
{
    struct quectel_temp_data *data = tzd->devdata;
    if (!data)
        return -EINVAL; // Return error if data is not available
    *temp = data->cur_temp; // Provide the current temperature in m°C
    return 0;
}

/* Thermal zone operations structure */
static const struct thermal_zone_device_ops quectel_temp_ops = {
    .get_temp = quectel_temp_get_temp,
};

/* Sysfs read function for "cur_temp" */
static ssize_t cur_temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct quectel_temp_data *data = dev_get_drvdata(dev);
    return scnprintf(buf, PAGE_SIZE, "%d\n", data->cur_temp); // Return the current temperature
}

/* Sysfs write function for "cur_temp" */
static ssize_t cur_temp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct quectel_temp_data *data = dev_get_drvdata(dev);
    int val;

    if (kstrtoint(buf, 10, &val) == 0) { // Parse the input value
        data->cur_temp = val; // Update the current temperature
        dev_info(dev, "[QuectelTemp] cur_temp updated to %d m°C\n", val);
        
        /* Notify the thermal framework with appropriate backward compatibility */
        thermal_zone_device_update(data->tzd, THERMAL_EVENT_UNSPECIFIED);
    }

    return count;
}

/* Define the sysfs attribute for "cur_temp" with read/write permissions */
static DEVICE_ATTR_RW(cur_temp);

/* Probe function: Initializes the virtual sensor */
static int quectel_temp_probe(struct platform_device *pdev)
{
    struct quectel_temp_data *data;
    int ret;

    dev_info(&pdev->dev, "Probing quectel_rm520n_temp...\n");

    // Allocate memory for the sensor data
    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data) {
        dev_err(&pdev->dev, "Memory allocation failed for quectel_rm520n_temp_data\n");
        return -ENOMEM;
    }

    // Default temperature value: 40°C (40000 m°C)
    data->cur_temp = 40000;

    // Register the sensor only if a Device Tree node is present
    if (!pdev->dev.of_node) {
        dev_err(&pdev->dev, "No DT node found for quectel_rm520n_temp\n");
        return -ENODEV;
    }

    // Register the thermal zone with the thermal framework
    // Using proper conditional compilation based on kernel version
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0)
    // For OpenWRT with kernel 6.6 (and future versions)
    data->tzd = devm_thermal_of_zone_register(&pdev->dev, 0, data, &quectel_temp_ops);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
    // Use devm_thermal_of_zone_register for kernel 5.15+
    data->tzd = devm_thermal_of_zone_register(&pdev->dev, 0, data, &quectel_temp_ops);
#else
    // Fallback to the traditional registration method for older kernels
    data->tzd = devm_thermal_zone_device_register(&pdev->dev, "quectel_rm520n", 
                                                0, data, &quectel_temp_ops, 
                                                NULL, 0, 0);
#endif

    if (IS_ERR(data->tzd)) {
        ret = PTR_ERR(data->tzd);
        dev_err(&pdev->dev, "Failed to register thermal zone, error %d\n", ret);
        return ret;
    }
    dev_info(&pdev->dev, "Thermal zone registered at %p\n", data->tzd);

    // Store the sensor data in the platform device
    platform_set_drvdata(pdev, data);

    // Create the sysfs file for "cur_temp"
    ret = device_create_file(&pdev->dev, &dev_attr_cur_temp);
    if (ret) {
        dev_err(&pdev->dev, "Failed to create cur_temp sysfs file, error %d\n", ret);
        return ret;
    }

    dev_info(&pdev->dev, "Quectel RM520N virtual sensor loaded\n");
    return 0;
}

/* Remove function: Cleans up the virtual sensor */
static int quectel_temp_remove(struct platform_device *pdev)
{
    device_remove_file(&pdev->dev, &dev_attr_cur_temp); // Remove the sysfs file
    dev_info(&pdev->dev, "Quectel RM520N virtual sensor removed\n");
    return 0;
}

/* Device Tree match table for automatic binding */
static const struct of_device_id quectel_temp_of_match[] = {
    { .compatible = "quectel-rm520n-temp", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, quectel_temp_of_match);

/* Platform driver structure */
static struct platform_driver quectel_temp_driver = {
    .probe = quectel_temp_probe,
    .remove = quectel_temp_remove,
    .driver = {
        .name = "quectel_rm520n_temp",
        .of_match_table = quectel_temp_of_match,
    },
};

/* Module initialization function */
static int __init quectel_temp_init(void)
{
    return platform_driver_register(&quectel_temp_driver);
}

/* Module exit function */
static void __exit quectel_temp_exit(void)
{
    platform_driver_unregister(&quectel_temp_driver);
}

module_init(quectel_temp_init);
module_exit(quectel_temp_exit);

MODULE_AUTHOR(PKG_MAINTAINER);
MODULE_DESCRIPTION(PKG_NAME " Virtual Thermal Sensor with DT-only registration");
MODULE_LICENSE(PKG_LICENSE);
MODULE_VERSION(PKG_TAG);
