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

/* Cached hwmon device state */
static int g_hwmon_device_num = -1;
static int g_hwmon_cache_valid = 0;

/**
 * Extract hwmon device number from directory name (e.g., "hwmon3" -> 3)
 */
static int extract_hwmon_number(const char *name)
{
    if (!name || strncmp(name, "hwmon", 5) != 0) {
        return -1;
    }
    char *endptr;
    long num = strtol(name + 5, &endptr, 10);
    if (*endptr != '\0' || num < 0 || num > 999) {
        return -1;
    }
    return (int)num;
}

/**
 * invalidate_hwmon_cache - Clear the cached hwmon device information
 */
void invalidate_hwmon_cache(void)
{
    g_hwmon_device_num = -1;
    g_hwmon_cache_valid = 0;
    logging_debug("Hwmon cache invalidated");
}

/**
 * find_quectel_hwmon_device - Find the hwmon device number for quectel_rm520n
 *
 * Dynamically discovers the hwmon device number for quectel_rm520n_thermal
 * by scanning /sys/class/hwmon devices. Results are cached for performance.
 *
 * Device name matching priority:
 * 1. "quectel_rm520n_thermal" (exact match, highest priority)
 * 2. "quectel_rm520n_hwmon" (alternate name)
 * 3. Any name containing "quectel_rm520n" (fallback)
 *
 * @param use_cache: If true, return cached result if available. If false, force rescan.
 * @return Device number (>= 0) on success, -1 on failure
 */
int find_quectel_hwmon_device(int use_cache)
{
    DIR *hwmon_dir;
    struct dirent *entry;
    int exact_match = -1;
    int fallback_match = -1;

    /* Return cached result if valid and requested */
    if (use_cache && g_hwmon_cache_valid) {
        if (g_hwmon_device_num >= 0) {
            /* Verify device still exists */
            char verify_path[PATH_MAX_LEN];
            snprintf(verify_path, sizeof(verify_path), "/sys/class/hwmon/hwmon%d/temp1_input", g_hwmon_device_num);
            if (access(verify_path, R_OK) == 0) {
                logging_debug("Using cached hwmon device: hwmon%d", g_hwmon_device_num);
                return g_hwmon_device_num;
            }
            /* Cache invalid, need to rescan */
            logging_debug("Cached hwmon%d no longer valid, rescanning", g_hwmon_device_num);
        }
        g_hwmon_cache_valid = 0;
    }

    hwmon_dir = opendir("/sys/class/hwmon");
    if (!hwmon_dir) {
        return -1;
    }

    while ((entry = readdir(hwmon_dir)) != NULL) {
        char name_path[PATH_MAX_LEN];
        char verify_path[PATH_MAX_LEN];
        char dev_name[DEVICE_NAME_LEN];
        FILE *name_fp;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* Build paths */
        if (snprintf(name_path, sizeof(name_path), "/sys/class/hwmon/%s/name", entry->d_name) >= (int)sizeof(name_path))
            continue;
        if (snprintf(verify_path, sizeof(verify_path), "/sys/class/hwmon/%s/temp1_input", entry->d_name) >= (int)sizeof(verify_path))
            continue;

        /* Read device name */
        name_fp = fopen(name_path, "r");
        if (!name_fp)
            continue;

        if (fgets(dev_name, sizeof(dev_name), name_fp) == NULL) {
            fclose(name_fp);
            continue;
        }
        fclose(name_fp);
        STRIP_NEWLINE(dev_name);

        logging_debug("Found hwmon device: %s -> %s", entry->d_name, dev_name);

        /* Verify device has temp1_input */
        if (access(verify_path, R_OK) != 0)
            continue;

        int num = extract_hwmon_number(entry->d_name);
        if (num < 0)
            continue;

        /* Check for exact match (highest priority) */
        if (strcmp(dev_name, "quectel_rm520n_thermal") == 0 ||
            strcmp(dev_name, "quectel_rm520n_hwmon") == 0) {
            exact_match = num;
            logging_debug("Found exact match: hwmon%d (%s)", num, dev_name);
            break;  /* Exact match found, stop searching */
        }

        /* Check for partial match (fallback) */
        if (strstr(dev_name, "quectel_rm520n") != NULL && fallback_match < 0) {
            fallback_match = num;
            logging_debug("Found fallback match: hwmon%d (%s)", num, dev_name);
        }
    }
    closedir(hwmon_dir);

    /* Use exact match if found, otherwise use fallback */
    int result = (exact_match >= 0) ? exact_match : fallback_match;

    /* Update cache */
    g_hwmon_device_num = result;
    g_hwmon_cache_valid = 1;

    if (result >= 0) {
        logging_debug("Selected hwmon device: hwmon%d", result);
    }

    return result;
}

/**
 * find_quectel_hwmon_path - Find the hwmon path for quectel_rm520n device
 * @path_buf: Buffer to store the path
 * @buf_size: Size of the buffer
 *
 * Dynamically discovers the hwmon device number for quectel_rm520n_thermal
 * by scanning /sys/class/hwmon devices. Returns the full path to temp1_input.
 * Uses caching for performance.
 *
 * @return 0 on success, -1 on failure
 */
int find_quectel_hwmon_path(char *path_buf, size_t buf_size)
{
    if (!path_buf || buf_size == 0) {
        return -1;
    }

    int hwmon_num = find_quectel_hwmon_device(1);  /* Use caching */
    if (hwmon_num < 0) {
        return -1;
    }

    if (snprintf(path_buf, buf_size, "/sys/class/hwmon/hwmon%d/temp1_input", hwmon_num) >= (int)buf_size) {
        return -1;
    }

    logging_debug("Hwmon path: %s", path_buf);
    return 0;
}
