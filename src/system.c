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
#include "logging.h"
#include "system.h"

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
    daemon_lock_fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0644);
    if (daemon_lock_fd < 0) {
        return -1;
    }

    // Try to acquire exclusive lock
    if (flock(daemon_lock_fd, LOCK_EX | LOCK_NB) < 0) {
        close(daemon_lock_fd);
        daemon_lock_fd = -1;
        return -1; // Lock acquisition failed
    }

    // Write PID to PID file
    FILE *pid_file = fopen(PID_FILE, "w");
    if (pid_file) {
        fprintf(pid_file, "%d\n", getpid());
        fclose(pid_file);
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
 * Sets shutdown flag and logs the event for proper service management.
 * 
 * Following clig.dev guidelines for signal handling and graceful
 * shutdown procedures.
 * 
 * @param sig Signal number received
 */
void signal_handler(int sig)
{
    if (sig == SIGTERM || sig == SIGINT) {
        logging_info("Received signal %d, initiating graceful shutdown", sig);
        shutdown_requested = 1; // Set the global shutdown flag
    }
}
