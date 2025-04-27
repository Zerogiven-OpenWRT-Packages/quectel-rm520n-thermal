/*
 * quectel_rm520n_temp.c
 *
 * Sysfs-based temperature interface for the Quectel RM520N module.
 * Provides a virtual temperature sensor that can be read and updated
 * via the sysfs interface.
 *
 * The module creates a sysfs directory "quectel_rm520n" with a file
 * "temp" that allows reading and writing the temperature value.
 *
 * Author: Christopher Sollinger
 * License: GPL
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/version.h>

// Package metadata definitions
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

/* Compatibility defines for kernel version differences */
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

// Replace magic numbers with named constants
#define DEFAULT_TEMP 40000 // 40°C in m°C
#define TEMP_MIN -30000    // -30°C in m°C
#define TEMP_MAX 75000     // 75°C in m°C
#define TEMP_CRIT 85000    // 85°C in m°C

// Buffer to store the current temperature value
static char modem_temp[16] = "N/A";

// Read function: Returns the current temperature value
static ssize_t temp_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    (void)kobj;
    (void)attr;
    // Format the temperature value into the buffer
    return scnprintf(buf, PAGE_SIZE, "%s\n", modem_temp);
}

// Write function: Updates the temperature value (only root can write)
static ssize_t temp_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    (void)kobj;
    (void)attr;
    size_t len = (count < sizeof(modem_temp)) ? count : (sizeof(modem_temp) - 1);

    for (size_t i = 0; i < len; i++)
    {
        modem_temp[i] = buf[i];
    }

    if (len > 0 && modem_temp[len - 1] == '\n')
    {
        len--;
    }
    modem_temp[len] = '\0';

    pr_info("Quectel RM520N: Updated temperature to %s\n", modem_temp);
    return count;
}

// Sysfs attribute: 0644 (read/write for owner, read for others)
static struct kobj_attribute temp_attribute = __ATTR(temp, 0644, temp_show, temp_store);

static struct kobject *temp_kobj;

// Module initialization function
static int __init quectel_rm520n_temp_init(void)
{
    // Create the sysfs directory "quectel_rm520n"
    temp_kobj = kobject_create_and_add("quectel_rm520n", kernel_kobj);
    if (!temp_kobj)
        return -ENOMEM;

    // Create the "temp" file
    if (sysfs_create_file(temp_kobj, &temp_attribute.attr))
    {
        kobject_put(temp_kobj);
        return -ENOMEM;
    }

    pr_info("Quectel RM520N temperature module loaded.\n");
    return 0;
}

// Module cleanup function
static void __exit quectel_rm520n_temp_exit(void)
{
    kobject_put(temp_kobj);
    pr_info("Quectel RM520N temperature module unloaded.\n");
}

module_init(quectel_rm520n_temp_init);
module_exit(quectel_rm520n_temp_exit);

MODULE_LICENSE(PKG_LICENSE);
MODULE_AUTHOR(PKG_MAINTAINER);
MODULE_DESCRIPTION(PKG_NAME " Kernel Module");
MODULE_VERSION(PKG_TAG);
