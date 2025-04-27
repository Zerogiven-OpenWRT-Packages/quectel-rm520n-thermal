/*
 * daemon.h
 *
 * This header defines common structures and functions for the 
 * Quectel RM520N temperature monitoring daemon.
 *
 * Author: Christopher Sollinger
 * License: GPL
 */

#ifndef DAEMON_H
#define DAEMON_H

#include <syslog.h>
#include <stdbool.h>
#include <uci.h>

/* Configuration file paths */
#define SYSFS_PATH "/sys/kernel/quectel_rm520n/temp"
#define ALT_SENSOR_PATH "/sys/devices/platform/quectel_rm520n_temp/cur_temp"

/* Timeouts and command definitions */
#define AT_TIMEOUT_SEC 5

/* Default hwmon path, will be determined at runtime */
extern char hwmon_path[256];

/* Error value to report when temperature reading fails */
extern char error_value[64];

/* Structure to hold daemon configuration settings */
struct daemon_config {
    char *serial_port;
    int interval;
    int baud_rate;
    char *temp_prefix;
    char *error_value;
    int dt_overlay_support;
    int fallback_register;
    int debug;
};

/* Function declarations */
void do_log(int err, const char *message, ...);
void read_uci_config(void);
int parse_uci_config(struct daemon_config *config);
void write_temp_to_path(const char *path, const char *temp_str);
int init_hwmon_sensor(void);
int create_pid_file(const char *pid_file);
int check_pid_file(const char *pid_file);
void cleanup_daemon(int pid_fd, int serial_fd);

/* Extract temperature values from response */
int extract_temp_values(const char *response, int *modem_temp, int *ap_temp, int *pa_temp);

/* Serial/TTY functions */
int init_serial_port(const char *port, speed_t baud_rate);
int send_at_command(int fd, const char *command, char *response, size_t response_len);
void process_at_response(const char *response);
void handle_at_error(char *error_value);

#endif /* DAEMON_H */
