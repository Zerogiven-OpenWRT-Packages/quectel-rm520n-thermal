/*
 * daemon.h
 *
 * Header file for the Quectel RM520N temperature daemon.
 * Provides function declarations, constants, and global variables for managing
 * serial communication, UCI configuration, and temperature sensor handling.
 *
 * Author: Christopher Sollinger
 * License: GPL
 */

#ifndef QUECTEL_RM520N_TEMP_DAEMON_H
#define QUECTEL_RM520N_TEMP_DAEMON_H

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

/* Global configuration – default values that can be overridden by UCI */
extern char serial_port[64]; // Serial port for modem communication
extern int interval;         // Interval for temperature polling in seconds
extern speed_t baud_rate;    // Baud rate for serial communication
extern char error_value[64]; // Error value to handle AT command failures
extern char hwmon_path[PATH_MAX]; // Path to the hwmon sensor

/* AT command and other constants */
#define AT_TIMEOUT_SEC 2 // Timeout for AT command responses
#define AT_COMMAND "AT+QTEMP\r" // Command to query temperature from the modem
#define SYSFS_PATH "/sys/kernel/quectel_rm520n/temp" // Sysfs path for temperature
#define ALT_SENSOR_PATH "/sys/devices/platform/quectel_rm520n@0/cur_temp" // Alternate sensor path
#define PID_FILE "/var/run/quectel_rm520n_temp_daemon.pid" // PID file for the daemon

#define MAX_PATHS 10

/* Function declarations for serial communication and UCI management */
int init_serial_port(const char *port, speed_t baud_rate); // Initialize serial port
int send_at_command(int fd, const char *command, char *response, size_t response_len); // Send AT command
int read_modem_response(int fd, char *buf, size_t buflen); // Read response from modem
void process_at_response(const char *response); // Process AT command response
void handle_at_error(char *error_value); // Handle errors in AT command responses
void cleanup_daemon(int pid_fd, int fd); // Cleanup resources during daemon shutdown
void read_uci_config(void); // Read configuration from UCI
void do_log(int err, const char *message, ...);

/* PID file functions */
int create_pid_file(const char *pid_file); // Create a PID file
int check_pid_file(const char *pid_file); // Check if PID file exists

/* Function declarations for hwmon sensor */
int init_hwmon_sensor(void); // Initialize hwmon sensor
void write_temp_to_sysfs(const char *temp_str); // Write temperature to sysfs
void write_temp_to_hwmon(const char *temp_str); // Write temperature to hwmon path
void write_temp_to_sensor_module(const char *temp_str); // Write temperature to sensor module

// Add a generic function declaration for writing temperature values
void write_temp_to_path(const char *path, const char *temp_str);

/* Function declarations for path tracking */
int find_or_add_path(const char *path);

#endif /* QUECTEL_RM520N_TEMP_DAEMON_H */
