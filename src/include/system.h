/**
 * @file system.h
 * @brief System utilities and management function declarations
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 *
 * Header file for system-level functions including daemon process management,
 * file locking, signal handling, and system status checking.
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#include <signal.h>

/* ============================================================================
 * FUNCTION DECLARATIONS
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
int check_daemon_running(void);

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
int acquire_daemon_lock(void);

/**
 * Release daemon lock and cleanup resources
 * 
 * Releases the file lock, closes file descriptors, and removes
 * PID and lock files. Ensures clean shutdown and resource cleanup.
 * 
 * Following clig.dev guidelines for graceful shutdown and resource
 * management.
 */
void release_daemon_lock(void);

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
void signal_handler(int sig);

/**
 * Global shutdown flag for signal handling
 * Using sig_atomic_t for signal-safe access
 */
extern volatile sig_atomic_t shutdown_requested;

#endif /* SYSTEM_H */
