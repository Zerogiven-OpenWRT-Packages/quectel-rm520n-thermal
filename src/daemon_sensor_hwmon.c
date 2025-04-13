/*
 * daemon_sensor_hwmon.c
 *
 * This file contains functions for interacting with the hwmon subsystem
 * to find and write temperature values to the appropriate hwmon sensor.
 *
 * The hwmon sensor is identified by its name "quectel_rm520n" in the
 * /sys/class/hwmon directory. If found, the temperature is written to
 * the corresponding "temp1_input" file.
 *
 * Author: Christopher Sollinger
 * License: GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include "daemon.h" // Required for do_log function

// Global variable to store the hwmon sensor path
char hwmon_path[PATH_MAX] = {0};

/* Searches for the hwmon sensor with the name "quectel_rm520n" */
static int find_hwmon_sensor(char *path, size_t path_len)
{
    DIR *dir;
    struct dirent *entry;
    char namefile[PATH_MAX];
    FILE *fp;
    char devname[64];

    // Open the hwmon directory
    dir = opendir("/sys/class/hwmon");
    if (!dir)
    {
        do_log(LOG_ERR, "Failed to open /sys/class/hwmon: %s", strerror(errno));
        return -1;
    }

    // Iterate through the directory entries
    while ((entry = readdir(dir)) != NULL)
    {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        // Construct the path to the "name" file
        snprintf(namefile, sizeof(namefile), "/sys/class/hwmon/%s/name", entry->d_name);
        fp = fopen(namefile, "r");
        if (!fp)
            continue;

        // Read the device name and check if it matches "quectel_rm520n"
        if (fgets(devname, sizeof(devname), fp) != NULL)
        {
            devname[strcspn(devname, "\n")] = '\0'; // Remove newline character
            if (strcmp(devname, "quectel_rm520n") == 0)
            {
                // Construct the path to the "temp1_input" file
                snprintf(path, path_len, "/sys/class/hwmon/%s/temp1_input", entry->d_name);
                fclose(fp);
                closedir(dir);
                return 0; // Sensor found
            }
        }
        fclose(fp);
    }

    closedir(dir);
    return -1; // Sensor not found
}

/* Initializes the hwmon sensor by finding its path */
int init_hwmon_sensor()
{
    // Attempt to find the hwmon sensor and store its path
    return find_hwmon_sensor(hwmon_path, sizeof(hwmon_path));
}

/* Writes the temperature value to the hwmon sensor */
void write_temp_to_hwmon(const char *temp_str) {
    write_temp_to_path(hwmon_path, temp_str);
}
