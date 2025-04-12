/*
 * daemon.c
 *
 * This file implements the main daemon logic for reading temperature
 * values from the modem via AT commands and writing them to various
 * output channels, including sysfs, hwmon, and a virtual sensor module.
 *
 * The daemon supports configuration via UCI and ensures only one instance
 * runs at a time using a PID file.
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
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include "daemon.h"

#define PID_FILE "/var/run/quectel_rm520n_temp_daemon.pid"
#define AT_COMMAND "AT+QTEMP\r"

char serial_port[64] = "/dev/ttyUSB2"; // Default serial port
int interval = 10;                     // Default polling interval in seconds
speed_t baud_rate = B115200;           // Default baud rate
char error_value[64] = "N/A";

/* Main function */
int main(int argc, char *argv[])
{
    int pid_fd = -1;

    // Check if another instance of the daemon is already running
    if (check_pid_file(PID_FILE))
    {
        fprintf(stderr, "Error: Another instance of the daemon is already running.\n");
        return 1;
    }

    // Run in the foreground if "--foreground" is passed as an argument
    if (!(argc > 1 && strcmp(argv[1], "--foreground") == 0))
    {
        pid_fd = create_pid_file(PID_FILE);
        if (pid_fd < 0)
            return 1;
    }

    openlog("quectel_rm520n_temp_daemon", LOG_PID | LOG_CONS, LOG_DAEMON);
    read_uci_config();

    // Initialize the hwmon sensor
    if (init_hwmon_sensor() == 0)
        syslog(LOG_INFO, "hwmon sensor initialized at: %s", hwmon_path);
    else
    {
        syslog(LOG_ERR, "Failed to initialize hwmon sensor.");
        hwmon_path[0] = '\0';
    }

    // Initialize the serial port
    int fd = init_serial_port(serial_port, baud_rate);
    if (fd < 0)
        return 1;

    // Main loop: Read temperature and write to outputs
    while (1)
    {
        char at_buf[1024];

        if (send_at_command(fd, AT_COMMAND, at_buf, sizeof(at_buf)) > 0)
        {
            process_at_response(at_buf);
        }
        else
        {
            handle_at_error(error_value);
        }

        sleep(interval);
    }

    cleanup_daemon(pid_fd, fd);
    return 0;
}

/* Reads the UCI configuration using libuci */
void read_uci_config(void)
{
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx)
    {
        syslog(LOG_ERR, "Failed to allocate UCI context");
        return;
    }

    struct uci_package *pkg = NULL;
    if (uci_load(ctx, "quectel_rm520n_thermal", &pkg) != UCI_OK)
    {
        syslog(LOG_ERR, "Failed to load UCI package 'quectel_rm520n_temp'");
        uci_free_context(ctx);
        return;
    }

    struct uci_section *section = uci_lookup_section(ctx, pkg, "settings");
    if (!section)
    {
        syslog(LOG_ERR, "No 'settings' section in UCI package");
        uci_unload(ctx, pkg);
        uci_free_context(ctx);
        return;
    }

    const char *value;
    value = uci_lookup_option_string(ctx, section, "serial_port");
    if (value)
    {
        strncpy(serial_port, value, sizeof(serial_port) - 1);
        serial_port[sizeof(serial_port) - 1] = '\0';
    }

    value = uci_lookup_option_string(ctx, section, "interval");
    if (value)
    {
        int tmp = atoi(value);
        if (tmp > 0)
            interval = tmp;
        else
            syslog(LOG_WARNING, "Invalid interval '%s', using default %d", value, interval);
    }

    value = uci_lookup_option_string(ctx, section, "baud_rate");
    if (value)
    {
        int tmp_baud = atoi(value);
        switch (tmp_baud)
        {
        case 9600:
            baud_rate = B9600;
            break;
        case 19200:
            baud_rate = B19200;
            break;
        case 38400:
            baud_rate = B38400;
            break;
        case 57600:
            baud_rate = B57600;
            break;
        case 115200:
            baud_rate = B115200;
            break;
        default:
            syslog(LOG_WARNING, "Unknown baud_rate '%s', fallback to 115200", value);
            baud_rate = B115200;
            break;
        }
    }

    value = uci_lookup_option_string(ctx, section, "error_value");
    if (value)
    {
        strncpy(error_value, value, sizeof(error_value) - 1);
        error_value[sizeof(error_value) - 1] = '\0';
    }

    syslog(LOG_INFO, "UCI config loaded: serial_port=%s, interval=%d, baud_rate=%d, error_value=%s",
           serial_port, interval, baud_rate, error_value);

    uci_unload(ctx, pkg);
    uci_free_context(ctx);
}

/* Writes the temperature value to SYSFS_PATH */
void write_temp_to_sysfs(const char *temp_str)
{
    FILE *fp = fopen(SYSFS_PATH, "w");
    if (!fp)
    {
        syslog(LOG_ERR, "Failed to open sysfs path '%s' for writing: %s", SYSFS_PATH, strerror(errno));
        return;
    }

    fprintf(fp, "%s\n", temp_str);
    fclose(fp);
    syslog(LOG_DEBUG, "Wrote temperature %s to %s", temp_str, SYSFS_PATH);
}

/* Creates a PID file to ensure only one instance of the daemon runs */
int create_pid_file(const char *pid_file)
{
    int fd = open(pid_file, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
    {
        syslog(LOG_ERR, "Failed to open PID file %s: %s", pid_file, strerror(errno));
        return -1;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) < 0)
    {
        syslog(LOG_ERR, "Another instance of the daemon is already running.");
        close(fd);
        return -1;
    }

    if (ftruncate(fd, 0) < 0 || dprintf(fd, "%d\n", getpid()) < 0)
    {
        syslog(LOG_ERR, "Failed to write PID to file %s: %s", pid_file, strerror(errno));
        close(fd);
        return -1;
    }

    return fd; // Keep the file open to maintain the lock
}

/* Checks if a PID file exists and if the process is still running */
int check_pid_file(const char *pid_file)
{
    int fd = open(pid_file, O_RDONLY);
    if (fd < 0)
        return 0;

    char buf[16];
    if (read(fd, buf, sizeof(buf) - 1) > 0)
    {
        buf[sizeof(buf) - 1] = '\0';
        pid_t pid = (pid_t)atoi(buf);
        if (pid > 0 && kill(pid, 0) == 0)
        {
            syslog(LOG_ERR, "Another instance of the daemon is already running with PID %d.", pid);
            close(fd);
            return 1;
        }
    }

    close(fd);
    return 0;
}
