/**
 * @file main.c
 * @brief Sysfs temperature interface kernel module for Quectel RM520N
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This kernel module provides a sysfs-based temperature interface,
 * creating /sys/kernel/quectel_rm520n/temp for reading and writing
 * temperature values from userspace applications.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/version.h>

/* Package metadata definitions */
#include "../common.h"

/* Compatibility defines for kernel version differences */
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

/* Temperature threshold storage (in m°C) */
static int temp_min = DEFAULT_TEMP_MIN;
static int temp_max = DEFAULT_TEMP_MAX;
static int temp_crit = DEFAULT_TEMP_CRIT;
static int temp_default = DEFAULT_TEMP_DEFAULT;

/* Current temperature value in m°C */
static int modem_temp = DEFAULT_TEMP_DEFAULT;

/**
 * temp_show - Sysfs read function for current temperature
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Output buffer for temperature string
 *
 * Reads the current temperature value and formats it for sysfs output.
 * Returns the temperature as a string with newline.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t temp_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    (void)kobj;
    (void)attr;
    
    /* Validate output buffer */
    if (!buf) {
        return -EINVAL;
    }
    
    return scnprintf(buf, PAGE_SIZE, "%d\n", modem_temp);
}

/**
 * temp_store - Sysfs write function for current temperature
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Input buffer containing temperature string
 * @count: Number of characters in input buffer
 *
 * Updates the current temperature value from sysfs input. Handles
 * newline removal and ensures proper string termination.
 *
 * Return: Number of characters processed on success
 */
static ssize_t temp_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    (void)kobj;
    (void)attr;
    int value;
    
    /* Validate input parameters */
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    /* Parse the temperature value from the buffer */
    if (kstrtoint(buf, 10, &value) == 0) {
        /* Store the temperature value in m°C */
        modem_temp = value;
        /* Temperature updated successfully */
        return count;
    }
    
    pr_err("Quectel RM520N: Failed to parse temperature value from input\n");
    return -EINVAL;
}

/* Temperature threshold show functions */
/**
 * temp_min_show - Sysfs read function for minimum temperature threshold
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Output buffer for temperature value
 *
 * Returns the minimum temperature threshold in m°C.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t temp_min_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    (void)kobj;
    (void)attr;
    
    /* Validate output buffer */
    if (!buf) {
        return -EINVAL;
    }
    
    return scnprintf(buf, PAGE_SIZE, "%d\n", temp_min);
}

/**
 * temp_max_show - Sysfs read function for maximum temperature threshold
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Output buffer for temperature value
 *
 * Returns the maximum temperature threshold in m°C.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t temp_max_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    (void)kobj;
    (void)attr;
    
    /* Validate output buffer */
    if (!buf) {
        return -EINVAL;
    }
    
    return scnprintf(buf, PAGE_SIZE, "%d\n", temp_max);
}

/**
 * temp_crit_show - Sysfs read function for critical temperature threshold
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Output buffer for temperature value
 *
 * Returns the critical temperature threshold in m°C.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t temp_crit_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    (void)kobj;
    (void)attr;
    
    /* Validate output buffer */
    if (!buf) {
        return -EINVAL;
    }
    
    return scnprintf(buf, PAGE_SIZE, "%d\n", temp_crit);
}

/**
 * temp_default_show - Sysfs read function for default temperature threshold
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Output buffer for temperature value
 *
 * Returns the default temperature threshold in m°C.
 *
 * Return: Number of characters written to buffer
 */
static ssize_t temp_default_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    (void)kobj;
    (void)attr;
    
    /* Validate output buffer */
    if (!buf) {
        return -EINVAL;
    }
    
    return scnprintf(buf, PAGE_SIZE, "%d\n", temp_default);
}

/* Temperature threshold store functions */
/**
 * temp_min_store - Sysfs write function for minimum temperature threshold
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Input buffer containing temperature value
 * @count: Number of characters in input buffer
 *
 * Updates the minimum temperature threshold. Validates that the new value
 * is within the absolute minimum range and less than or equal to max.
 *
 * Return: Number of characters processed on success, -EINVAL on error
 */
static ssize_t temp_min_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    (void)kobj;
    (void)attr;
    int value;
    
    /* Validate input parameters */
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    if (kstrtoint(buf, 10, &value) == 0) {
        if (value < TEMP_ABSOLUTE_MIN) {
            pr_err("Quectel RM520N: temp_min value %d m°C below absolute minimum %d m°C\n", 
                   value, TEMP_ABSOLUTE_MIN);
            return -EINVAL;
        }
        
        if (value > temp_max) {
            pr_err("Quectel RM520N: temp_min value %d m°C cannot exceed temp_max %d m°C\n", 
                   value, temp_max);
            return -EINVAL;
        }
        
        temp_min = value;
        pr_info("Quectel RM520N: Updated temp_min to %d m°C\n", temp_min);
        return count;
    }
    
    pr_err("Quectel RM520N: Failed to parse temp_min value from input\n");
    return -EINVAL;
}

/**
 * temp_max_store - Sysfs write function for maximum temperature threshold
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Input buffer containing temperature value
 * @count: Number of characters in input buffer
 *
 * Updates the maximum temperature threshold. Validates that the new value
 * is greater than or equal to min and within the absolute maximum range.
 *
 * Return: Number of characters processed on success, -EINVAL on error
 */
static ssize_t temp_max_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    (void)kobj;
    (void)attr;
    int value;
    
    /* Validate input parameters */
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    if (kstrtoint(buf, 10, &value) == 0) {
        if (value < temp_min) {
            pr_err("Quectel RM520N: temp_max value %d m°C cannot be below temp_min %d m°C\n", 
                   value, temp_min);
            return -EINVAL;
        }
        
        if (value > TEMP_ABSOLUTE_MAX) {
            pr_err("Quectel RM520N: temp_max value %d m°C above absolute maximum %d m°C\n", 
                   value, TEMP_ABSOLUTE_MAX);
            return -EINVAL;
        }
        
        temp_max = value;
        pr_info("Quectel RM520N: Updated temp_max to %d m°C\n", temp_max);
        return count;
    }
    
    pr_err("Quectel RM520N: Failed to parse temp_max value from input\n");
    return -EINVAL;
}

/**
 * temp_crit_store - Sysfs write function for critical temperature threshold
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Input buffer containing temperature value
 * @count: Number of characters in input buffer
 *
 * Updates the critical temperature threshold. Validates that the new value
 * is greater than or equal to max and within the absolute maximum range.
 *
 * Return: Number of characters processed on success, -EINVAL on error
 */
static ssize_t temp_crit_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    (void)kobj;
    (void)attr;
    int value;
    
    /* Validate input parameters */
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    if (kstrtoint(buf, 10, &value) == 0) {
        if (value < temp_max) {
            pr_err("Quectel RM520N: temp_crit value %d m°C cannot be below temp_max %d m°C\n", 
                   value, temp_max);
            return -EINVAL;
        }
        
        if (value > TEMP_ABSOLUTE_MAX) {
            pr_err("Quectel RM520N: temp_crit value %d m°C above absolute maximum %d m°C\n", 
                   value, TEMP_ABSOLUTE_MAX);
            return -EINVAL;
        }
        
        temp_crit = value;
        pr_info("Quectel RM520N: Updated temp_crit to %d m°C\n", temp_crit);
        return count;
    }
    
    pr_err("Quectel RM520N: Failed to parse temp_crit value from input\n");
    return -EINVAL;
}

/**
 * temp_default_store - Sysfs write function for default temperature threshold
 * @kobj: Kernel object pointer
 * @attr: Kernel object attribute
 * @buf: Input buffer containing temperature value
 * @count: Number of characters in input buffer
 *
 * Updates the default temperature threshold. Validates that the new value
 * is within the min to max range.
 *
 * Return: Number of characters processed on success, -EINVAL on error
 */
static ssize_t temp_default_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    (void)kobj;
    (void)attr;
    int value;
    
    /* Validate input parameters */
    if (!buf || count == 0) {
        return -EINVAL;
    }
    
    if (kstrtoint(buf, 10, &value) == 0) {
        if (value < temp_min) {
            pr_err("Quectel RM520N: temp_default value %d m°C cannot be below temp_min %d m°C\n", 
                   value, temp_min);
            return -EINVAL;
        }
        
        if (value > temp_max) {
            pr_err("Quectel RM520N: temp_default value %d m°C cannot exceed temp_max %d m°C\n", 
                   value, temp_max);
            return -EINVAL;
        }
        
        temp_default = value;
        pr_info("Quectel RM520N: Updated temp_default to %d m°C\n", temp_default);
        return count;
    }
    
    pr_err("Quectel RM520N: Failed to parse temp_default value from input\n");
    return -EINVAL;
}

/* Sysfs attributes: 0644 (read/write for owner, read for others) */
static struct kobj_attribute temp_attribute = __ATTR(temp, 0644, temp_show, temp_store);
static struct kobj_attribute temp_min_attribute = __ATTR(temp_min, 0644, temp_min_show, temp_min_store);
static struct kobj_attribute temp_max_attribute = __ATTR(temp_max, 0644, temp_max_show, temp_max_store);
static struct kobj_attribute temp_crit_attribute = __ATTR(temp_crit, 0644, temp_crit_show, temp_crit_store);
static struct kobj_attribute temp_default_attribute = __ATTR(temp_default, 0644, temp_default_show, temp_default_store);

static struct kobject *temp_kobj;

/**
 * quectel_rm520n_temp_init - Module initialization function
 *
 * Creates the sysfs directory and all temperature-related attributes.
 * Implements proper error handling with cleanup on failure.
 * Sets up the complete sysfs interface for temperature management.
 *
 * Return: 0 on success, negative error code on failure
 */
static int __init quectel_rm520n_temp_init(void)
{
    int ret = 0;
    
    /* Create the sysfs directory "quectel_rm520n" */
    temp_kobj = kobject_create_and_add("quectel_rm520n", kernel_kobj);
    if (!temp_kobj) {
        pr_err("Quectel RM520N: Failed to create sysfs kobject\n");
        return -ENOMEM;
    }

    pr_info("Quectel RM520N: Created sysfs directory\n");

    /* Create sysfs files individually with proper error handling */
    ret = sysfs_create_file(temp_kobj, &temp_attribute.attr);
    if (ret) {
        pr_err("Quectel RM520N: Failed to create temp attribute (error: %d)\n", ret);
        goto cleanup;
    }

    ret = sysfs_create_file(temp_kobj, &temp_min_attribute.attr);
    if (ret) {
        pr_err("Quectel RM520N: Failed to create temp_min attribute (error: %d)\n", ret);
        goto cleanup;
    }

    ret = sysfs_create_file(temp_kobj, &temp_max_attribute.attr);
    if (ret) {
        pr_err("Quectel RM520N: Failed to create temp_max attribute (error: %d)\n", ret);
        goto cleanup;
    }

    ret = sysfs_create_file(temp_kobj, &temp_crit_attribute.attr);
    if (ret) {
        pr_err("Quectel RM520N: Failed to create temp_crit attribute (error: %d)\n", ret);
        goto cleanup;
    }

    ret = sysfs_create_file(temp_kobj, &temp_default_attribute.attr);
    if (ret) {
        pr_err("Quectel RM520N: Failed to create temp_default attribute (error: %d)\n", ret);
        goto cleanup;
    }

    pr_info("Quectel RM520N temperature module loaded successfully with thresholds: min=%d, max=%d, crit=%d, default=%d m°C\n",
            temp_min, temp_max, temp_crit, temp_default);
    return 0;

cleanup:
    pr_err("Quectel RM520N: Module initialization failed, cleaning up\n");
    kobject_put(temp_kobj);
    return ret;
}

/**
 * quectel_rm520n_temp_exit - Module cleanup function
 *
 * Removes all sysfs attributes and cleans up the kobject.
 * Called when the module is unloaded to prevent resource leaks.
 */
static void __exit quectel_rm520n_temp_exit(void)
{
    /* Remove all sysfs files before removing the kobject */
    sysfs_remove_file(temp_kobj, &temp_attribute.attr);
    sysfs_remove_file(temp_kobj, &temp_min_attribute.attr);
    sysfs_remove_file(temp_kobj, &temp_max_attribute.attr);
    sysfs_remove_file(temp_kobj, &temp_crit_attribute.attr);
    sysfs_remove_file(temp_kobj, &temp_default_attribute.attr);
    
    /* Remove the kobject (this also removes the directory) */
    kobject_put(temp_kobj);
    
    pr_info("Quectel RM520N temperature module unloaded.\n");
}

module_init(quectel_rm520n_temp_init);
module_exit(quectel_rm520n_temp_exit);

MODULE_DESCRIPTION(KMOD_NAME " - Kernel Module");
MODULE_AUTHOR(KMOD_AUTHOR);
MODULE_VERSION(KMOD_VERSION);
MODULE_LICENSE(KMOD_LICENSE);
