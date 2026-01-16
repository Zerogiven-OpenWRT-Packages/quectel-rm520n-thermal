/**
 * @file system.c
 * @brief System utilities and management functions for Quectel RM520N thermal management
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This file contains system-level functions including daemon process management,
 * file locking, signal handling, and system status checking. These functions
 * provide robust system integration and process control.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <dirent.h>
#include "include/common.h"
#include "include/logging.h"
#include "include/system.h"

/* ============================================================================
 * CONSTANTS & CONFIGURATION
 * ============================================================================ */

#define PID_FILE "/var/run/quectel_rm520n_temp.pid"
#define LOCK_FILE "/var/run/quectel_rm520n_temp.lock"

/* Global file descriptor for daemon lock */
static int daemon_lock_fd = -1;

/* ============================================================================
 * SYSTEM MANAGEMENT FUNCTIONS
 * ============================================================================ */

/**
 * Check if daemon is already running
 * 
 * Examines the PID file and verifies if the process is actually running.
 * Includes proper error handling and logging for robust daemon management.
 * Following clig.dev guidelines for service robustness.
 * 
 * @return 0 if not running, 1 if running, -1 on error
 */
int check_daemon_running(void)
{
    // Check PID file
    FILE *pid_file = fopen(PID_FILE, "r");
    if (!pid_file) {
        return 0; // No PID file, daemon not running
    }
    
    int pid;
    if (fscanf(pid_file, "%d", &pid) != 1) {
        fclose(pid_file);
        return 0; // Invalid PID file
    }
    fclose(pid_file);
    
    // Check if process is actually running
    if (kill(pid, 0) == 0) {
        return 1; // Daemon is running
    }
    
    // Process not running, clean up stale PID file
    unlink(PID_FILE);
    return 0;
}

/**
 * Acquire daemon lock to prevent multiple instances
 *
 * Creates a lock file using file locking (flock) to ensure only one
 * daemon instance can run at a time. Includes proper error handling
 * and cleanup on failure.
 *
 * Following clig.dev guidelines for service robustness and graceful
 * error handling.
 *
 * @return 0 on success, -1 on failure
 */
int acquire_daemon_lock(void)
{
    daemon_lock_fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0600);
    if (daemon_lock_fd < 0) {
        return -1;
    }

    // Try to acquire exclusive lock
    if (flock(daemon_lock_fd, LOCK_EX | LOCK_NB) < 0) {
        close(daemon_lock_fd);
        daemon_lock_fd = -1;
        return -1; // Lock acquisition failed
    }

    // Write PID to PID file with explicit permissions
    FILE *pid_file = fopen(PID_FILE, "w");
    if (pid_file) {
        fprintf(pid_file, "%d\n", getpid());
        fclose(pid_file);
        // Set explicit permissions: world-readable PID file is OK (0644)
        chmod(PID_FILE, 0644);
    }

    return 0;
}

/**
 * Release daemon lock and cleanup resources
 *
 * Releases the file lock, closes file descriptors, and removes
 * PID and lock files. Ensures clean shutdown and resource cleanup.
 *
 * Following clig.dev guidelines for graceful shutdown and resource
 * management.
 */
void release_daemon_lock(void)
{
    // Close and release the lock file descriptor
    if (daemon_lock_fd >= 0) {
        flock(daemon_lock_fd, LOCK_UN);
        close(daemon_lock_fd);
        daemon_lock_fd = -1;
    }

    // Remove PID file
    unlink(PID_FILE);

    // Remove lock file
    unlink(LOCK_FILE);
}

/**
 * Signal handler for graceful shutdown
 *
 * Handles SIGTERM and SIGINT signals to ensure graceful daemon shutdown.
 * Only sets the shutdown flag - logging is done in the main loop after
 * detecting the flag to maintain async-signal-safety.
 *
 * Following clig.dev guidelines for signal handling and graceful
 * shutdown procedures.
 *
 * @param sig Signal number received
 */
void signal_handler(int sig)
{
    if (sig == SIGTERM || sig == SIGINT) {
        /* Only set flag - do not call non-async-signal-safe functions */
        shutdown_requested = 1;
    }
}

/* ============================================================================
 * HWMON DISCOVERY FUNCTIONS
 * ============================================================================ */

/**
 * find_quectel_hwmon_path - Find the hwmon path for quectel_rm520n device
 * @path_buf: Buffer to store the path
 * @buf_size: Size of the buffer
 *
 * Dynamically discovers the hwmon device number for quectel_rm520n_thermal
 * by scanning /sys/class/hwmon devices. Returns the full path to temp1_input.
 *
 * @return 0 on success, -1 on failure
 */
int find_quectel_hwmon_path(char *path_buf, size_t buf_size)
{
    DIR *hwmon_dir;
    struct dirent *entry;
    int found = 0;

    if (!path_buf || buf_size == 0) {
        return -1;
    }

    hwmon_dir = opendir("/sys/class/hwmon");
    if (!hwmon_dir) {
        return -1;
    }

    while ((entry = readdir(hwmon_dir)) != NULL) {
        char name_path[PATH_MAX_LEN];
        char dev_name[DEVICE_NAME_LEN];
        FILE *name_fp;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (snprintf(name_path, sizeof(name_path), "/sys/class/hwmon/%s/name", entry->d_name) >= (int)sizeof(name_path)) {
            continue;
        }

        name_fp = fopen(name_path, "r");
        if (name_fp) {
            if (fgets(dev_name, sizeof(dev_name), name_fp) != NULL) {
                dev_name[strcspn(dev_name, "\n")] = '\0';
                if (strcmp(dev_name, "quectel_rm520n_thermal") == 0) {
                    fclose(name_fp);
                    if (snprintf(path_buf, buf_size, "/sys/class/hwmon/%s/temp1_input", entry->d_name) < (int)buf_size) {
                        found = 1;
                        logging_debug("Found quectel hwmon device: %s", path_buf);
                        break;
                    }
                }
            }
            fclose(name_fp);
        }
    }
    closedir(hwmon_dir);

    return found ? 0 : -1;
}
