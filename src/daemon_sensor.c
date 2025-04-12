/*
 * daemon_sensor.c
 *
 * Daemon utility for interacting with the Quectel RM520N virtual temperature sensor.
 * Provides functionality to write temperature values to the virtual sensor
 * located at ALT_SENSOR_PATH.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <syslog.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <sys/file.h>
#include <uci.h>
#include "daemon.h"

#define ALT_SENSOR_PATH "/sys/devices/platform/quectel_rm520n_temp-sensor@0/cur_temp"

/* -------------------------------------------------------------------------- */
/* write_temp_to_sensor_module()                                              */
/* Writes the temperature value to the virtual sensor (ALT_SENSOR_PATH).      */
/* Checks if the sensor is available before attempting to write.              */
/* -------------------------------------------------------------------------- */
void write_temp_to_sensor_module(const char *temp_str)
{
    static int sensor_checked = 0; // Flag to check sensor availability only once
    static int sensor_available = 0; // Flag indicating if the sensor is writable
    if (!sensor_checked)
    {
        // Check if the sensor path is writable
        if (access(ALT_SENSOR_PATH, W_OK) == 0)
        {
            sensor_available = 1;
            syslog(LOG_INFO, "Detected virtual sensor module. Will write temperature also to '%s'.", ALT_SENSOR_PATH);
        }
        sensor_checked = 1;
    }
    if (!sensor_available)
        return;

    // Open the sensor file for writing
    FILE *fp = fopen(ALT_SENSOR_PATH, "w");
    if (!fp)
    {
        syslog(LOG_ERR, "Failed to open sensor path '%s': %s", ALT_SENSOR_PATH, strerror(errno));
        return;
    }

    // Write the temperature value to the sensor
    fprintf(fp, "%s\n", temp_str);
    fclose(fp);
    syslog(LOG_DEBUG, "Wrote temperature %s to %s", temp_str, ALT_SENSOR_PATH);
}
