/*
 * daemon_tty.c
 *
 * This file contains functions for serial communication with the modem.
 * It includes initialization of the serial port, sending AT commands,
 * and processing responses from the modem.
 *
 * Author: Christopher Sollinger
 * License: GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <syslog.h>
#include <errno.h>
#include "daemon.h"

/* Extracts the temperature value from the AT response and writes it to all output channels */
void process_at_response(const char *response)
{
    const char *prefix = "modem-ambient-usr";
    char *temp_ptr = strstr(response, prefix);

    if (!temp_ptr)
    {
        // Handle missing temperature prefix in the response
        write_temp_to_path(SYSFS_PATH, error_value);
        write_temp_to_path(ALT_SENSOR_PATH, error_value);
        write_temp_to_path(hwmon_path, error_value);

        do_log(LOG_WARNING, "No temp_prefix in response; wrote '%s' to all outputs", error_value);
        return;
    }

    temp_ptr += strlen(prefix) + 3; // Skip prefix and ','"
    char *end = strchr(temp_ptr, '"');
    if (end)
        *end = '\0';
    int degC = atoi(temp_ptr);
    int temp_milli = degC * 1000;
    char temp_str[16];

    snprintf(temp_str, sizeof(temp_str), "%d", temp_milli);

    // Write the extracted temperature to all output channels
    write_temp_to_path(SYSFS_PATH, temp_str);
    write_temp_to_path(ALT_SENSOR_PATH, temp_str);
    write_temp_to_path(hwmon_path, temp_str);

    do_log(LOG_INFO, "Extracted temperature from response: %d°C => %s (m°C)", degC, temp_str);
}

/* Handles missing responses by writing an error value to sysfs */
void handle_at_error(char *error_value)
{
    // Write the error value to all output channels
    write_temp_to_path(SYSFS_PATH, error_value);
    write_temp_to_path(ALT_SENSOR_PATH, error_value);
    write_temp_to_path(hwmon_path, error_value);

    do_log(LOG_WARNING, "No data from modem; wrote '%s' to outputs", error_value);
}

/* Initializes the serial port and configures it */
int init_serial_port(const char *port, speed_t baud_rate)
{
    int fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        do_log(LOG_ERR, "Failed to open serial port %s: %s", port, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0)
    {
        do_log(LOG_ERR, "Failed to get termios for %s: %s", port, strerror(errno));
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        do_log(LOG_ERR, "Failed to set termios for %s: %s", port, strerror(errno));
        close(fd);
        return -1;
    }

    do_log(LOG_INFO, "Serial port %s initialized with baud rate %d", port, baud_rate);
    return fd;
}

/* Reads from the serial port until "OK" appears or timeout is reached */
int read_modem_response(int fd, char *buf, size_t buflen)
{
    memset(buf, 0, buflen);
    time_t start_time = time(NULL);
    int total = 0;

    while ((time(NULL) - start_time) < AT_TIMEOUT_SEC)
    {
        int n = read(fd, buf + total, buflen - total - 1);
        if (n > 0)
        {
            total += n;
            buf[total] = '\0';
            if (strstr(buf, "\nOK") || strstr(buf, "\rOK"))
                break;
        }
        else
        {
            usleep(10000); // Wait 10ms
        }
    }

    return total;
}

/* Sends an AT command and reads the response */
int send_at_command(int fd, const char *command, char *response, size_t response_len)
{
    tcflush(fd, TCIFLUSH);

    if (write(fd, command, strlen(command)) < 0)
    {
        do_log(LOG_ERR, "Failed to write AT command to serial port");
        return -1;
    }

    return read_modem_response(fd, response, response_len);
}
