/**
 * @file sensor.c
 * @brief Virtual thermal sensor kernel module for Quectel RM520N
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This kernel module registers a virtual thermal sensor with the Linux
 * Thermal Framework, providing temperature data to the thermal subsystem
 * for system-wide thermal management and fan control.
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
#include <linux/string.h>

#include "../common.h"

/* 
 * The THERMAL_EVENT_UNSPECIFIED enum was introduced in kernel 5.17
 * For earlier kernels, we define it here to ensure compatibility
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,17,0)
/* Use 0 as a safe default value for kernels before 5.17 */
#define THERMAL_EVENT_UNSPECIFIED 0
#endif

/* Data structure to store the current temperature (in m°C) */
struct quectel_temp_data {
    struct thermal_zone_device *tzd; /* Handle to the Thermal Zone */
    int cur_temp;                    /* Current temperature in milli-degrees Celsius (m°C) */
};

/**
 * quectel_temp_get_temp - Retrieve current temperature from thermal zone
 * @tzd: Pointer to thermal zone device
 * @temp: Pointer to store temperature value (in m°C)
 *
 * Callback function for the thermal framework to retrieve the current
 * temperature value. Returns 0 on success, -EINVAL on error.
 *
 * Return: 0 on success, -EINVAL if data is not available
 */
static int quectel_temp_get_temp(struct thermal_zone_device *tzd, int *temp)
{
    struct quectel_temp_data *data;
    
    /* Validate input parameters */
    if (!tzd || !temp) {
        return -EINVAL;
    }
    
    data = tzd->devdata;
    if (!data) {
        return -EINVAL;
    }
    
    *temp = data->cur_temp;
    return 0;
}

/* Thermal zone operations structure */
static const struct thermal_zone_device_ops quectel_temp_ops = {
    .get_temp = quectel_temp_get_temp,
};

/**
 * cur_temp_show - Sysfs read function for current temperature
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Output buffer
 *
 * Reads the current temperature value and formats it for sysfs output.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t cur_temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct quectel_temp_data *data = dev_get_drvdata(dev);
    
    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }
    
    return scnprintf(buf, PAGE_SIZE, "%d\n", data->cur_temp);
}

/**
 * cur_temp_store - Sysfs write function for current temperature
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Input buffer containing temperature value
 * @count: Number of characters in input buffer
 *
 * Updates the current temperature value from sysfs input. Validates
 * the input range and notifies the thermal framework of changes.
 *
 * Return: Number of characters processed on success, -EINVAL on error
 */
static ssize_t cur_temp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct quectel_temp_data *data = dev_get_drvdata(dev);
    int val;

    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }

    /* Check if count is reasonable */
    if (count == 0 || count > PAGE_SIZE) {
        return -EINVAL;
    }

    if (kstrtoint(buf, 10, &val) == 0) {
        /* Validate temperature range */
        if (val < TEMP_ABSOLUTE_MIN) {
            dev_err(dev, "QuectelTemp: Temperature value %d m°C below absolute minimum %d m°C\n", 
                    val, TEMP_ABSOLUTE_MIN);
            return -EINVAL;
        }
        
        if (val > TEMP_ABSOLUTE_MAX) {
            dev_err(dev, "QuectelTemp: Temperature value %d m°C above absolute maximum %d m°C\n", 
                    val, TEMP_ABSOLUTE_MAX);
            return -EINVAL;
        }
        
        data->cur_temp = val; /* Update the current temperature */
        /* Temperature updated successfully */
        
        /* Notify the thermal framework of temperature changes */
        thermal_zone_device_update(data->tzd, THERMAL_EVENT_UNSPECIFIED);
        return count;
    }

    dev_err(dev, "QuectelTemp: Failed to parse temperature value from input\n");
    return -EINVAL;
}

/* Define the sysfs attribute for "cur_temp" with read/write permissions */
static DEVICE_ATTR_RW(cur_temp);

/**
 * quectel_temp_probe - Probe function for virtual thermal sensor
 * @pdev: Platform device structure
 *
 * Initializes the virtual thermal sensor, allocates memory, registers
 * with the thermal framework, and creates sysfs attributes. Handles
 * thermal zone registration.
 *
 * Return: 0 on success, negative error code on failure
 */
static int quectel_temp_probe(struct platform_device *pdev)
{
    struct quectel_temp_data *data;
    int ret;

    dev_info(&pdev->dev, "Probing quectel_rm520n_temp...\n");

    /* Allocate memory for the sensor data */
    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data) {
        dev_err(&pdev->dev, "Memory allocation failed for quectel_rm520n_temp_data\n");
        return -ENOMEM;
    }

    /* Default temperature value from shared constants */
    data->cur_temp = DEFAULT_TEMP_DEFAULT;

    /* Register the sensor only if a Device Tree node is present */
    if (!pdev->dev.of_node) {
        dev_err(&pdev->dev, "No Device Tree node found for quectel_rm520n_temp\n");
        return -ENODEV;
    }

    /* Register the thermal zone with the thermal framework
     * Using devm_thermal_of_zone_register
     */
    data->tzd = devm_thermal_of_zone_register(&pdev->dev, 0, data, &quectel_temp_ops);

    if (IS_ERR(data->tzd)) {
        ret = PTR_ERR(data->tzd);
        dev_err(&pdev->dev, "Failed to register thermal zone: %d\n", ret);
        return ret;
    }
    dev_dbg(&pdev->dev, "Thermal zone registered successfully\n");

    /* Store the sensor data in the platform device */
    platform_set_drvdata(pdev, data);

    /* Create the sysfs file for "cur_temp" */
    ret = device_create_file(&pdev->dev, &dev_attr_cur_temp);
    if (ret) {
        dev_err(&pdev->dev, "Failed to create cur_temp sysfs file: %d\n", ret);
        return ret;
    }

    dev_info(&pdev->dev, "Quectel RM520N virtual sensor loaded\n");
    return 0;
}

/**
 * quectel_temp_remove - Remove function for virtual thermal sensor
 * @pdev: Platform device structure
 *
 * Cleans up the virtual thermal sensor by removing sysfs attributes
 * and logging the removal. Called when the module is unloaded.
 *
 * Return: 0 on success
 */
static int quectel_temp_remove(struct platform_device *pdev)
{
    /* Validate platform device */
    if (!pdev) {
        return -EINVAL;
    }
    
    device_remove_file(&pdev->dev, &dev_attr_cur_temp);
    dev_info(&pdev->dev, "Quectel RM520N virtual sensor removed\n");
    return 0;
}

/* Device Tree match table for automatic binding */
static const struct of_device_id quectel_temp_of_match[] = {
    { .compatible = "quectel,rm520n-temp", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, quectel_temp_of_match);

/* Platform driver structure */
static struct platform_driver quectel_temp_driver = {
    .probe = quectel_temp_probe,
    .remove = quectel_temp_remove,
    .driver = {
        .name = "quectel_rm520n_temp_sensor",
        .of_match_table = quectel_temp_of_match,
    },
};

/**
 * quectel_temp_init - Module initialization function
 *
 * Registers the platform driver with the kernel. Called when the
 * module is loaded.
 *
 * Return: 0 on success, negative error code on failure
 */
static int __init quectel_rm520n_temp_sensor_init(void)
{
    int ret;
    
    ret = platform_driver_register(&quectel_temp_driver);
    if (ret) {
        pr_err("QuectelTemp: Failed to register platform driver: %d\n", ret);
        return ret;
    }
    
    pr_info("QuectelTemp: Platform driver registered successfully\n");
    return 0;
}

/**
 * quectel_temp_exit - Module exit function
 *
 * Unregisters the platform driver from the kernel. Called when the
 * module is unloaded.
 */
static void __exit quectel_rm520n_temp_sensor_exit(void)
{
    platform_driver_unregister(&quectel_temp_driver);
    pr_info("QuectelTemp: Platform driver unregistered\n");
}

module_init(quectel_rm520n_temp_sensor_init);
module_exit(quectel_rm520n_temp_sensor_exit);

MODULE_DESCRIPTION(KMOD_NAME " - Virtual Thermal Sensor Driver");
MODULE_AUTHOR(KMOD_AUTHOR);
MODULE_VERSION(KMOD_VERSION);
MODULE_LICENSE(KMOD_LICENSE);
