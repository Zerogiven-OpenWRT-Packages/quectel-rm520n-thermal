/*
 * quectel_rm520n_temp_sensor_hwmon.c
 *
 * Dual hwmon driver for Quectel RM520N temperature sensor.
 *
 * This driver registers an hwmon device that provides the temperature value
 * (in millidegrees Celsius) via the "temp1_input" attribute as well as the
 * thresholds "temp1_min", "temp1_max", and "temp1_crit".
 *
 * Optional OF-Matching is included (compatible "quectel,rm520n-temp-hwmon").
 * If no corresponding Device Tree node is present, a fallback platform device
 * is registered in the module code, ensuring the sensor always appears.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>

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

/* Data structure to store the temperature value (in m°C) and thresholds */
struct quectel_hwmon_data {
    int temp;
    int temp_min;
    int temp_max;
    int temp_crit;
    struct mutex lock;
};

/* hwmon Read function for temp1_input */
static ssize_t temp1_input_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    int temp;
    mutex_lock(&data->lock); // Lock to ensure thread safety
    temp = data->temp; // Retrieve the current temperature
    mutex_unlock(&data->lock);
    return scnprintf(buf, PAGE_SIZE, "%d\n", temp); // Return the temperature
}

/* hwmon Write function for temp1_input */
static ssize_t temp1_input_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    int ret, val;
    ret = kstrtoint(buf, 10, &val);
    if (ret)
        return ret;
    mutex_lock(&data->lock); // Lock to ensure thread safety
    data->temp = val;
    mutex_unlock(&data->lock);
    dev_info(dev, "[QuectelHWMon] Temperature updated to %d m°C\n", val);
    return count;
}

/* hwmon Read function for temp1_min (low) */
static ssize_t temp1_min_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    return scnprintf(buf, PAGE_SIZE, "%d\n", data->temp_min);
}

/* hwmon Read function for temp1_max (high) */
static ssize_t temp1_max_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    return scnprintf(buf, PAGE_SIZE, "%d\n", data->temp_max);
}

/* hwmon Read function for temp1_crit (crit) */
static ssize_t temp1_crit_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct quectel_hwmon_data *data = dev_get_drvdata(dev);
    return scnprintf(buf, PAGE_SIZE, "%d\n", data->temp_crit);
}

/* Create the sensor attributes.
 * The macro SENSOR_DEVICE_ATTR_RW expects 3 arguments: (name, function prefix, index).
 */
static SENSOR_DEVICE_ATTR_RW(temp1_input, temp1_input, 0);
static SENSOR_DEVICE_ATTR_RO(temp1_min, temp1_min, 0);
static SENSOR_DEVICE_ATTR_RO(temp1_max, temp1_max, 0);
static SENSOR_DEVICE_ATTR_RO(temp1_crit, temp1_crit, 0);

/* Attribute list for the hwmon device */
static struct attribute *quectel_hwmon_attrs[] = {
    &sensor_dev_attr_temp1_input.dev_attr.attr,
    &sensor_dev_attr_temp1_min.dev_attr.attr,
    &sensor_dev_attr_temp1_max.dev_attr.attr,
    &sensor_dev_attr_temp1_crit.dev_attr.attr,
    NULL,
};
ATTRIBUTE_GROUPS(quectel_hwmon);

/* Probe function: Registers the hwmon device */
static int quectel_hwmon_probe(struct platform_device *pdev)
{
    struct quectel_hwmon_data *data;
    struct device *hwmon_dev;

    // Allocate memory for the sensor data
    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    mutex_init(&data->lock);

    // Set default values:
    // temp: current temperature value (40°C = 40000 m°C)
    // temp_min: -30°C = -30000 m°C
    // temp_max: 75°C = 75000 m°C
    // temp_crit: 85°C = 85000 m°C
    data->temp = 40000;
    data->temp_min = -30000;
    data->temp_max = 75000;
    data->temp_crit = 85000;

    platform_set_drvdata(pdev, data);

    // Register the hwmon device
    hwmon_dev = devm_hwmon_device_register_with_groups(&pdev->dev,
                                                          "quectel_rm520n_temp",
                                                          data,
                                                          quectel_hwmon_groups);
    if (IS_ERR(hwmon_dev)) {
        dev_err(&pdev->dev, "Failed to register hwmon device\n");
        return PTR_ERR(hwmon_dev);
    }

    dev_info(&pdev->dev, "Quectel RM520N hwmon sensor registered\n");
    return 0;
}

static int quectel_hwmon_remove(struct platform_device *pdev)
{
    return 0;
}

/* Optional OF-Matching: Automatic binding to a DT node with */
static const struct of_device_id quectel_hwmon_of_match[] = {
    { .compatible = "quectel,rm520n-temp-hwmon", },
    {},
};
MODULE_DEVICE_TABLE(of, quectel_hwmon_of_match);

static struct platform_driver quectel_hwmon_driver = {
    .probe = quectel_hwmon_probe,
    .remove = quectel_hwmon_remove,
    .driver = {
        .name = "quectel_rm520n_temp_sensor_hwmon",
        .of_match_table = quectel_hwmon_of_match,
        .owner = THIS_MODULE,
    },
};

/* Fallback: If no matching DT node is present, register a fallback platform device */
static struct platform_device *fallback_pdev;

static int __init quectel_hwmon_init(void)
{
    int ret;

    ret = platform_driver_register(&quectel_hwmon_driver);
    if (ret)
        return ret;

    if (!of_find_compatible_node(NULL, "quectel,rm520n-temp-hwmon", NULL)) {
        fallback_pdev = platform_device_register_simple("quectel_rm520n_temp_sensor_hwmon", -1, NULL, 0);
        if (IS_ERR(fallback_pdev)) {
            ret = PTR_ERR(fallback_pdev);
            platform_driver_unregister(&quectel_hwmon_driver);
            return ret;
        }
        dev_info(&fallback_pdev->dev,
                 "Fallback platform device registered for quectel hwmon sensor\n");
    }

    return 0;
}

static void __exit quectel_hwmon_exit(void)
{
    if (fallback_pdev)
        platform_device_unregister(fallback_pdev);
    platform_driver_unregister(&quectel_hwmon_driver);
}

module_init(quectel_hwmon_init);
module_exit(quectel_hwmon_exit);

MODULE_LICENSE(PKG_LICENSE);
MODULE_AUTHOR(PKG_MAINTAINER);
MODULE_DESCRIPTION(PKG_NAME " hwmon driver with fallback registration");
MODULE_VERSION(PKG_VERSION);
