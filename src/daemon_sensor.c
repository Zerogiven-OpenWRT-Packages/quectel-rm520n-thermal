/*
 * daemon_sensor.c
 *
 * Daemon utility for interacting with the Quectel RM520N virtual temperature sensor.
 * Provides functionality to write temperature values to the virtual sensor
 * located at ALT_SENSOR_PATH.
 *
 * Author: Christopher Sollinger
 * License: GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include "daemon.h"

#define ALT_SENSOR_PATH "/sys/devices/platform/quectel_rm520n@0/cur_temp"

/* -------------------------------------------------------------------------- */
/* write_temp_to_sensor_module()                                              */
/* Writes the temperature value to the virtual sensor (ALT_SENSOR_PATH).      */
/* Checks if the sensor is available before attempting to write.              */
/* -------------------------------------------------------------------------- */
void write_temp_to_sensor_module(const char *temp_str) {
    static int sensor_checked = 0;
    static int sensor_available = 0;

    if (!sensor_checked) {
        if (access(ALT_SENSOR_PATH, W_OK) == 0) {
            sensor_available = 1;
            do_log(LOG_INFO, "Detected virtual sensor module. Will write temperature also to '%s'.", ALT_SENSOR_PATH);
        }
        sensor_checked = 1;
    }

    if (!sensor_available)
        return;

    write_temp_to_path(ALT_SENSOR_PATH, temp_str);
}
