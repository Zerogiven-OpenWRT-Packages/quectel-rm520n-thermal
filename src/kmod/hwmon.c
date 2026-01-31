/**
 * @file hwmon.c
 * @brief Hwmon temperature sensor kernel module for Quectel RM520N
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This kernel module provides hwmon integration for temperature monitoring,
 * exposing temperature data and configurable thresholds through the standard
 * hwmon interface for system monitoring tools and utilities.
 * 
 * Features:
 * - Configurable temperature thresholds (min, max, critical)
 * - Thread-safe operations with mutex protection
 * - Automatic fallback to default values if sysfs reading fails
 * - Device Tree compatibility with fallback platform device support
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/string.h>

#include "../include/common.h"
#include "../include/kmod_hwmon.h"

/**
 * read_sysfs_value - Helper function to read integer values from sysfs
 * @path: Path to the sysfs file
 * @default_val: Default value to return if reading fails
 *
 * Attempts to read an integer value from a sysfs file. Returns the
 * default value if the file cannot be read or parsed.
 *
 * Return: The read value on success, default value on failure
 */
static int read_sysfs_value(const char *path, int default_val)
{
    struct file *fp;
    char buf[32];
    loff_t pos = 0;
    int result = default_val;
    ssize_t bytes_read;
    
    if (!path) {
        pr_debug("QuectelHWMon: Invalid path provided to read_sysfs_value\n");
        return default_val;
    }
    
    fp = filp_open(path, O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        bytes_read = kernel_read(fp, buf, sizeof(buf) - 1, &pos);
        if (bytes_read > 0 && pos < sizeof(buf)) {
            buf[pos] = '\0';
            if (kstrtoint(buf, 10, &result) == 0) {
                pr_debug("QuectelHWMon: Read value %d from %s\n", result, path);
            } else {
                pr_debug("QuectelHWMon: Failed to parse value from %s, using default %d\n", path, default_val);
            }
        }
        filp_close(fp, NULL);
    } else {
        pr_debug("QuectelHWMon: Could not open %s, using default %d\n", path, default_val);
    }
    
    return result;
}

/**
 * temp1_input_show - Hwmon read function for current temperature
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Output buffer for temperature value
 *
 * Reads the current temperature value with proper mutex locking
 * for thread safety.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t temp1_input_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    int temp;

    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }

    mutex_lock(&data->lock);
    temp = data->temp;
    mutex_unlock(&data->lock);

    return scnprintf(buf, PAGE_SIZE, "%d\n", temp);
}

/**
 * temp1_input_store - Hwmon write function for current temperature
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Input buffer containing temperature value
 * @count: Number of characters in input buffer
 *
 * Updates the current temperature value from hwmon input. Uses
 * proper mutex locking for thread safety.
 *
 * Return: Number of characters processed on success, negative error code on error
 */
static ssize_t temp1_input_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    int ret, val;

    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }

    /* Check if count is reasonable */
    if (count == 0 || count > PAGE_SIZE) {
        return -EINVAL;
    }

    ret = kstrtoint(buf, 10, &val);
    if (ret)
        return ret;

    /* Validate temperature is within reasonable bounds */
    if (val < TEMP_ABSOLUTE_MIN || val > TEMP_ABSOLUTE_MAX) {
        dev_err(dev, "QuectelHWMon: Temperature value %d m°C outside valid range [%d, %d] m°C\n", 
                val, TEMP_ABSOLUTE_MIN, TEMP_ABSOLUTE_MAX);
        return -EINVAL;
    }

    mutex_lock(&data->lock);
    data->temp = val;
    mutex_unlock(&data->lock);

    /* Temperature updated successfully */
    return count;
}

/**
 * temp1_min_show - Hwmon read function for minimum temperature threshold
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Output buffer for temperature value
 *
 * Returns the minimum temperature threshold in m°C.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t temp1_min_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);

    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", data->temp_min);
}

/**
 * temp1_max_show - Hwmon read function for maximum temperature threshold
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Output buffer for temperature value
 *
 * Returns the maximum temperature threshold in m°C.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t temp1_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);

    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", data->temp_max);
}

/**
 * temp1_crit_show - Hwmon read function for critical temperature threshold
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Output buffer for temperature value
 *
 * Returns the critical temperature threshold in m°C.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t temp1_crit_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);

    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", data->temp_crit);
}

/* Hwmon write functions for configurable temperature thresholds */
/**
 * temp1_min_store - Hwmon write function for minimum temperature threshold
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Input buffer containing temperature value
 * @count: Number of characters in input buffer
 *
 * Updates the minimum temperature threshold. Validates that the new value
 * is within the absolute minimum range and less than or equal to max.
 * Uses proper mutex locking for thread safety.
 *
 * Return: Number of characters processed on success, -EINVAL on error
 */
static ssize_t temp1_min_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    int ret, val;

    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }

    /* Check if count is reasonable */
    if (count == 0 || count > PAGE_SIZE) {
        return -EINVAL;
    }

    ret = kstrtoint(buf, 10, &val);
    if (ret)
        return ret;

    if (val < TEMP_ABSOLUTE_MIN) {
        dev_err(dev, "QuectelHWMon: temp_min value %d m°C below absolute minimum %d m°C\n", val, TEMP_ABSOLUTE_MIN);
        return -EINVAL;
    }
    
    if (val > data->temp_max) {
        dev_err(dev, "QuectelHWMon: temp_min value %d m°C cannot exceed temp_max %d m°C\n", val, data->temp_max);
        return -EINVAL;
    }
    
    mutex_lock(&data->lock);
    data->temp_min = val;
    mutex_unlock(&data->lock);
            dev_info(dev, "QuectelHWMon: temp_min updated to %d m°C\n", val);
    return count;
}

/**
 * temp1_max_store - Hwmon write function for maximum temperature threshold
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Input buffer containing temperature value
 * @count: Number of characters in input buffer
 *
 * Updates the maximum temperature threshold. Validates that the new value
 * is greater than or equal to min and within the absolute maximum range.
 * Uses proper mutex locking for thread safety.
 *
 * Return: Number of characters processed on success, -EINVAL on error
 */
static ssize_t temp1_max_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    int ret, val;

    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }

    /* Check if count is reasonable */
    if (count == 0 || count > PAGE_SIZE) {
        return -EINVAL;
    }

    ret = kstrtoint(buf, 10, &val);
    if (ret)
        return ret;

    if (val < data->temp_min) {
        dev_err(dev, "QuectelHWMon: temp_max value %d m°C cannot be below temp_min %d m°C\n", val, data->temp_min);
        return -EINVAL;
    }
    
    if (val > TEMP_ABSOLUTE_MAX) {
        dev_err(dev, "QuectelHWMon: temp_max value %d m°C above absolute maximum %d m°C\n", val, TEMP_ABSOLUTE_MAX);
        return -EINVAL;
    }
    
    mutex_lock(&data->lock);
    data->temp_max = val;
    mutex_unlock(&data->lock);
            dev_info(dev, "QuectelHWMon: temp_max updated to %d m°C\n", val);
    return count;
}

/**
 * temp1_crit_store - Hwmon write function for critical temperature threshold
 * @dev: Device pointer
 * @attr: Device attribute
 * @buf: Input buffer containing temperature value
 * @count: Number of characters in input buffer
 *
 * Updates the critical temperature threshold. Validates that the new value
 * is greater than or equal to max and within the absolute maximum range.
 * Uses proper mutex locking for thread safety.
 *
 * Return: Number of characters processed on success, -EINVAL on error
 */
static ssize_t temp1_crit_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    int ret, val;

    /* Validate input parameters */
    if (!dev || !attr || !buf || !data) {
        return -EINVAL;
    }

    /* Check if count is reasonable */
    if (count == 0 || count > PAGE_SIZE) {
        return -EINVAL;
    }

    ret = kstrtoint(buf, 10, &val);
    if (ret)
        return ret;

    if (val < data->temp_max) {
        dev_err(dev, "QuectelHWMon: temp_crit value %d m°C cannot be below temp_max %d m°C\n", val, data->temp_max);
        return -EINVAL;
    }
    
    if (val > TEMP_ABSOLUTE_MAX) {
        dev_err(dev, "QuectelHWMon: temp_crit value %d m°C above absolute maximum %d m°C\n", val, TEMP_ABSOLUTE_MAX);
        return -EINVAL;
    }
    
    mutex_lock(&data->lock);
    data->temp_crit = val;
    mutex_unlock(&data->lock);
    dev_info(dev, "QuectelHWMon: temp_crit updated to %d m°C\n", val);
    return count;
}

/**
 * SENSOR_DEVICE_ATTR_2 - Hwmon sensor attribute definitions
 *
 * Creates the sysfs attributes with explicit permissions for the hwmon device.
 * Uses SENSOR_DEVICE_ATTR_2 to have full control over permissions:
 * - S_IRUGO = 0444 (readable by all)
 * - S_IWUSR = 0200 (writable by owner)
 * - Combined: 0644 (readable by all, writable by owner)
 *
 * SENSOR_DEVICE_ATTR_2 requires 6 arguments: (name, mode, show, store, nr, index)
 */
static SENSOR_DEVICE_ATTR_2(temp1_input, S_IRUGO | S_IWUSR, temp1_input_show, temp1_input_store, 0, 0);
static SENSOR_DEVICE_ATTR_2(temp1_min, S_IRUGO | S_IWUSR, temp1_min_show, temp1_min_store, 0, 0);
static SENSOR_DEVICE_ATTR_2(temp1_max, S_IRUGO | S_IWUSR, temp1_max_show, temp1_max_store, 0, 0);
static SENSOR_DEVICE_ATTR_2(temp1_crit, S_IRUGO | S_IWUSR, temp1_crit_show, temp1_crit_store, 0, 0);

/**
 * quectel_hwmon_attrs - Hwmon device attribute array
 *
 * Defines the sysfs attributes exposed by the hwmon device. Includes
 * temperature input, minimum, maximum, and critical thresholds.
 * The ATTRIBUTE_GROUPS macro creates the attribute groups structure.
 */
static struct attribute *quectel_hwmon_attrs[] = {
    &sensor_dev_attr_temp1_input.dev_attr.attr,
    &sensor_dev_attr_temp1_min.dev_attr.attr,
    &sensor_dev_attr_temp1_max.dev_attr.attr,
    &sensor_dev_attr_temp1_crit.dev_attr.attr,
    NULL,
};
ATTRIBUTE_GROUPS(quectel_hwmon);

/**
 * quectel_hwmon_probe - Platform driver probe function
 * @pdev: Platform device pointer
 *
 * Initializes the hwmon sensor data structure, reads current temperature
 * thresholds from the main sysfs interface if available, and registers
 * the hwmon device with the Linux hwmon subsystem.
 *
 * Return: 0 on success, negative error code on failure
 */
static int quectel_hwmon_probe(struct platform_device *pdev)
{
    struct quectel_hwmon_data *data;
    struct device *hwmon_dev;
    struct device *hwmon_parent;
    struct kobject *hwmon_kobj;
    int main_temp_min;
    int main_temp_max;
    int main_temp_crit;
    int main_temp_default;

    /* Allocate memory for the hwmon sensor data structure */
    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    mutex_init(&data->lock);

    /* Read current temperature thresholds from main sysfs interface */
    main_temp_min = read_sysfs_value("/sys/kernel/quectel_rm520n_thermal/temp_min", DEFAULT_TEMP_MIN);
    main_temp_max = read_sysfs_value("/sys/kernel/quectel_rm520n_thermal/temp_max", DEFAULT_TEMP_MAX);
    main_temp_crit = read_sysfs_value("/sys/kernel/quectel_rm520n_thermal/temp_crit", DEFAULT_TEMP_CRIT);
    main_temp_default = read_sysfs_value("/sys/kernel/quectel_rm520n_thermal/temp_default", DEFAULT_TEMP_DEFAULT);

    /* Set values from main sysfs if available, otherwise use defaults */
    data->temp = main_temp_default;
    data->temp_min = main_temp_min;
    data->temp_max = main_temp_max;
    data->temp_crit = main_temp_crit;

    /* Log the final values that were set (debug level) */
    dev_dbg(&pdev->dev, "Hwmon initialized with values: temp=%d, min=%d, max=%d, crit=%d m°C\n",
             data->temp, data->temp_min, data->temp_max, data->temp_crit);

    platform_set_drvdata(pdev, data);

    /* The hwmon_device_register_with_groups API has been stable since kernel 3.13
     * so we don't need conditional compilation for basic registration
     */
    hwmon_dev = devm_hwmon_device_register_with_groups(&pdev->dev,
                                                     "quectel_rm520n_thermal",
                                                     data,
                                                     quectel_hwmon_groups);

    if (IS_ERR(hwmon_dev)) {
        dev_err(&pdev->dev, "Failed to register hwmon device: %ld\n", PTR_ERR(hwmon_dev));
        return PTR_ERR(hwmon_dev);
    }

    dev_info(&pdev->dev, "Quectel RM520N hwmon sensor registered\n");

    /* Log hwmon device hierarchy for debugging (debug level) */
    hwmon_parent = hwmon_dev->parent;
    if (hwmon_parent) {
        dev_dbg(&pdev->dev, "Hwmon parent device: %s", dev_name(hwmon_parent));

        /* Log the hwmon kobject path for debugging */
        hwmon_kobj = &hwmon_dev->kobj;
        if (hwmon_kobj) {
            dev_dbg(&pdev->dev, "Hwmon kobject path: %s", kobject_name(hwmon_kobj));
        }
    }

    return 0;
}

/**
 * quectel_hwmon_remove - Platform driver remove function
 * @pdev: Platform device pointer
 *
 * Called when the platform device is removed. Currently a no-op
 * as devm_* functions handle cleanup automatically.
 */
static void quectel_hwmon_remove(struct platform_device *pdev)
{
    (void)pdev;
}

/**
 * quectel_hwmon_of_match - Device Tree compatible string table
 *
 * Defines the compatible string for automatic binding to Device Tree nodes.
 * This allows the driver to be automatically loaded on systems with
 * matching DT nodes.
 */
static const struct of_device_id quectel_hwmon_of_match[] = {
    { .compatible = "quectel-rm520n-hwmon", },
    {},
};
MODULE_DEVICE_TABLE(of, quectel_hwmon_of_match);

/**
 * quectel_hwmon_driver - Platform driver structure
 *
 * Defines the platform driver operations and metadata for the hwmon sensor.
 * Includes probe/remove functions and Device Tree compatibility.
 */
static struct platform_driver quectel_hwmon_driver = {
    .probe = quectel_hwmon_probe,
    .remove_new = quectel_hwmon_remove,
    .driver = {
        .name = "quectel_rm520n_temp_sensor_hwmon",
        .of_match_table = quectel_hwmon_of_match,
        .owner = THIS_MODULE,
    },
};

/**
 * fallback_pdev - Fallback platform device pointer
 *
 * Stores a reference to the fallback platform device created when no
 * Device Tree node is found. Used for cleanup during module unload.
 */
static struct platform_device *fallback_pdev;

/**
 * quectel_hwmon_init - Module initialization function
 *
 * Registers the platform driver and creates a fallback platform device
 * if no Device Tree node is found. This ensures the driver works on
 * systems without DT support.
 *
 * Return: 0 on success, negative error code on failure
 */
static int __init quectel_rm520n_temp_sensor_hwmon_init(void)
{
    int ret;
    struct device_node *dt_node;

    ret = platform_driver_register(&quectel_hwmon_driver);
    if (ret) {
        pr_err("Failed to register platform driver: %d\n", ret);
        return ret;
    }

    /* Only create fallback device if no DT node exists */
    dt_node = of_find_compatible_node(NULL, "quectel-rm520n-hwmon", NULL);
    if (!dt_node) {
        fallback_pdev = platform_device_register_simple("quectel_rm520n_temp_sensor_hwmon", -1, NULL, 0);

        if (IS_ERR(fallback_pdev)) {
            ret = PTR_ERR(fallback_pdev);
            pr_err("Failed to register fallback platform device: %d\n", ret);
            platform_driver_unregister(&quectel_hwmon_driver);
            return ret;
        }

        dev_info(&fallback_pdev->dev, "Fallback platform device registered for quectel hwmon sensor\n");
    } else {
        of_node_put(dt_node);  /* Release reference to DT node */
    }

    return 0;
}

/**
 * quectel_hwmon_exit - Module cleanup function
 *
 * Unregisters the platform driver and removes the fallback platform device
 * if it was created. Called when the module is unloaded.
 */
static void __exit quectel_rm520n_temp_sensor_hwmon_exit(void)
{
    if (fallback_pdev)
        platform_device_unregister(fallback_pdev);
    platform_driver_unregister(&quectel_hwmon_driver);
}

module_init(quectel_rm520n_temp_sensor_hwmon_init);
module_exit(quectel_rm520n_temp_sensor_hwmon_exit);

MODULE_DESCRIPTION(KMOD_NAME " - Hwmon Temperature Sensor Driver");
MODULE_AUTHOR(KMOD_AUTHOR);
MODULE_VERSION(KMOD_VERSION);
MODULE_LICENSE(KMOD_LICENSE);
