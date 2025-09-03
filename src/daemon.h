/**
 * @file daemon.h
 * @brief Daemon mode function declarations
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * Header file for daemon mode implementation including continuous temperature
 * monitoring and kernel interface integration.
 */

#ifndef DAEMON_H
#define DAEMON_H

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

/**
 * Daemon mode - background temperature monitoring
 * 
 * Implements the daemon service for continuous temperature monitoring.
 * Reads modem temperatures via AT commands and writes to kernel interfaces
 * (hwmon, thermal zones) at regular intervals.
 * 
 * Includes safety checks for thermal zones and automatic kernel module loading.
 * Following clig.dev guidelines for service robustness and graceful shutdown.
 * 
 * @param shutdown_flag Pointer to shutdown flag for graceful termination
 * @return 0 on success, 1 on error, 3 if daemon already running
 */
int daemon_mode(volatile bool *shutdown_flag);

#endif /* DAEMON_H */
