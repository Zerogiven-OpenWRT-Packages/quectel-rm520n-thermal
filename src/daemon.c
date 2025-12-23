/**
 * @file daemon.c
 * @brief Daemon mode implementation for Quectel RM520N thermal management
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This file contains the daemon mode implementation for continuous temperature
 * monitoring, including kernel interface integration and thermal zone management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include "include/logging.h"
#include "include/config.h"
#include "include/common.h"
#include "include/serial.h"
#include "include/temperature.h"
#include "include/system.h"
#include "include/uci_config.h"

/* External variables from main.c */
extern config_t config;

/* ============================================================================
 * CONSTANTS & CONFIGURATION
 * ============================================================================ */

#define MAX_RESPONSE 1024
#define AT_COMMAND "AT+QTEMP\r"

/* Global file descriptor for emergency cleanup */
static int g_serial_fd = -1;

/* Error tracking statistics */
typedef struct {
    unsigned long serial_errors;       /* Serial port open/communication failures */
    unsigned long at_command_errors;   /* AT command send failures */
    unsigned long parse_errors;        /* Temperature parsing failures */
    unsigned long successful_reads;    /* Successful temperature reads */
    unsigned long total_iterations;    /* Total monitoring iterations */
} daemon_stats_t;

static daemon_stats_t g_stats = {0};

/* Daemon start time for uptime calculation */
static time_t g_daemon_start_time = 0;

/* ============================================================================
 * CLEANUP FUNCTIONS
 * ============================================================================ */

/**
 * daemon_cleanup - Emergency cleanup function for unexpected termination
 *
 * Registered with atexit() to ensure resources are released even if
 * the daemon exits unexpectedly. Closes open file descriptors and
 * releases locks in a signal-safe manner.
 */
static void daemon_cleanup(void)
{
    // Close serial port if open
    if (g_serial_fd >= 0) {
        close(g_serial_fd);
        g_serial_fd = -1;
    }

    // Release daemon lock
    release_daemon_lock();
}

/* ============================================================================
 * DAEMON MODE IMPLEMENTATION
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
int daemon_mode(volatile sig_atomic_t *shutdown_flag)
{
    // Check if daemon is already running
    if (check_daemon_running()) {
        fprintf(stderr, "Error: Daemon is already running. Use 'status' to check or stop existing instance.\n");
        fprintf(stderr, "Try 'quectel_rm520n_temp --help' for more information\n");
        return 3;
    }
    
    // Acquire daemon lock
    if (acquire_daemon_lock() < 0) {
        fprintf(stderr, "Error: Cannot acquire daemon lock. Another instance may be running.\n");
        fprintf(stderr, "Try 'quectel_rm520n_temp --help' for more information\n");
        return 3;
    }

    // Register cleanup function for emergency shutdown
    // This ensures resources are released even on unexpected termination
    if (atexit(daemon_cleanup) != 0) {
        fprintf(stderr, "Warning: Failed to register cleanup handler\n");
    }

    // Initialize logging system for daemon
    // Daemon mode: use syslog output, no stderr
    int log_threshold = config_parse_log_level(config.log_level);
    logging_init(true, false, (log_threshold == LOG_DEBUG), BINARY_NAME);

    // Set up signal handlers for graceful shutdown
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // Record daemon start time for uptime metrics
    g_daemon_start_time = time(NULL);

    logging_info("Daemon started successfully");

    // Check kernel module status
    logging_info("Checking kernel module status...");
    FILE *modules_fp = fopen("/proc/modules", "r");
    if (modules_fp) {
        char line[MODULE_LINE_LEN];  // Reduced from 256 - module lines are typically short
        while (fgets(line, sizeof(line), modules_fp)) {
            if (strstr(line, "quectel_rm520n_temp")) {
                logging_info("Kernel module loaded: %s", line);
            }
        }
        fclose(modules_fp);
    }
    
    // Check platform devices
    DIR *platform_dir = opendir("/sys/devices/platform");
    if (platform_dir) {
        struct dirent *entry;
        while ((entry = readdir(platform_dir)) != NULL) {
            if (strstr(entry->d_name, "quectel_rm520n")) {
                logging_info("Platform device found: %s", entry->d_name);
            }
        }
        closedir(platform_dir);
    }
    
    // Check thermal zones (for informational purposes only)
    logging_info("Scanning thermal zones to identify available interfaces...");
    DIR *thermal_dir = opendir("/sys/devices/virtual/thermal");
    if (thermal_dir) {
        struct dirent *entry;
        while ((entry = readdir(thermal_dir)) != NULL) {
            if (strncmp(entry->d_name, "thermal_zone", 12) == 0) {
                char type_path[PATH_MAX_LEN];  // Must accommodate entry->d_name (up to 255 chars) + path prefix
                if (snprintf(type_path, sizeof(type_path), "/sys/devices/virtual/thermal/%s/type", entry->d_name) >= sizeof(type_path)) {
                    logging_warning("Thermal zone type path truncated, zone skipped: %s", entry->d_name);
                    continue;
                }
                
                FILE *type_fp = fopen(type_path, "r");
                if (type_fp) {
                    char zone_type[DEVICE_NAME_LEN];
                    if (fgets(zone_type, sizeof(zone_type), type_fp) != NULL) {
                        zone_type[strcspn(zone_type, "\n")] = '\0';
                        logging_info("Thermal zone found: %s (type: %s)", entry->d_name, zone_type);
                        
                        // Note which zones are safe for modem temperature writing
                        if (strstr(zone_type, "cpu") != NULL || 
                            strstr(zone_type, "gpu") != NULL ||
                            strstr(zone_type, "soc") != NULL) {
                            logging_info("  -> System thermal zone (will NOT write modem temperatures here)");
                        } else if (strstr(zone_type, "modem") != NULL || 
                                   strstr(zone_type, "quectel") != NULL) {
                            logging_info("  -> Modem thermal zone (safe for temperature writing)");
                        } else {
                            logging_info("  -> Unknown thermal zone (will NOT write modem temperatures here)");
                        }
                    }
                    fclose(type_fp);
                }
            }
        }
        closedir(thermal_dir);
    }
    
    // Main daemon loop
    int serial_reconnect_attempts = 0;
    int reconnect_delay = SERIAL_INITIAL_RECONNECT_DELAY;
    g_serial_fd = -1;  // Use global for emergency cleanup access

    // Find hwmon path once (cached for performance)
    char hwmon_path[PATH_MAX_LEN] = {0};
    int hwmon_available = (find_quectel_hwmon_path(hwmon_path, sizeof(hwmon_path)) == 0);
    if (hwmon_available) {
        logging_info("Hwmon interface available: %s", hwmon_path);
    } else {
        logging_warning("Hwmon interface not found, will skip hwmon writes");
    }

    // Check shutdown flag for graceful termination
    while (shutdown_flag && !(*shutdown_flag)) {
        // Increment iteration counter
        g_stats.total_iterations++;

        // Make a local copy of config for this iteration to avoid race conditions
        // This ensures config doesn't change mid-operation even if updated by reload
        config_t loop_config = config;

        // Update kernel module thresholds from UCI config (if changed)
        static time_t last_config_check = 0;
        time_t current_time = time(NULL);
        if (current_time - last_config_check >= CONFIG_CHECK_INTERVAL) {
            logging_debug("Checking for UCI config changes...");

            // Store current config for comparison
            config_t previous_config = config;

            // Reload UCI configuration (UCI library provides its own file locking)
            if (config_read_uci(&config) == 0) {
                logging_debug("UCI configuration reloaded successfully");
                logging_debug("New config: port=%s, baud=%d", config.serial_port, (int)config.baud_rate);

                // Check if config actually changed
                int config_changed = (strcmp(previous_config.serial_port, config.serial_port) != 0) ||
                                    (previous_config.baud_rate != config.baud_rate) ||
                                    (previous_config.interval != config.interval) ||
                                    (strcmp(previous_config.log_level, config.log_level) != 0) ||
                                    (strcmp(previous_config.temp_modem_prefix, config.temp_modem_prefix) != 0) ||
                                    (strcmp(previous_config.temp_ap_prefix, config.temp_ap_prefix) != 0) ||
                                    (strcmp(previous_config.temp_pa_prefix, config.temp_pa_prefix) != 0);

                if (config_changed) {
                    logging_info("UCI configuration changed, updating settings");

                    // If log level changed, update logging threshold
                    if (strcmp(previous_config.log_level, config.log_level) != 0) {
                        int new_threshold = config_parse_log_level(config.log_level);
                        ulog_threshold(new_threshold);
                        logging_info("Log level changed to '%s'", config.log_level);
                    }

                    // If serial port or baud rate changed, close current connection
                    // It will be reopened with new settings on next iteration
                    if (strcmp(previous_config.serial_port, config.serial_port) != 0 ||
                        previous_config.baud_rate != config.baud_rate) {
                        if (g_serial_fd >= 0) {
                            logging_info("Serial configuration changed, closing current connection");
                            close(g_serial_fd);
                            g_serial_fd = -1;
                        }
                    }

                    if (uci_config_mode() == 0) {
                        logging_info("Kernel module thresholds updated from UCI config");
                    } else {
                        logging_warning("Failed to update kernel module thresholds");
                    }

                    // Update loop_config for this iteration
                    loop_config = config;
                } else {
                    logging_debug("No UCI config changes detected, skipping kernel module update");
                }
            } else {
                logging_warning("Failed to reload UCI configuration");
            }
            last_config_check = current_time;
        }
        
        // Initialize or reconnect serial port
        if (g_serial_fd < 0) {
            g_serial_fd = init_serial_port(loop_config.serial_port, loop_config.baud_rate);
            if (g_serial_fd < 0) {
                g_stats.serial_errors++;
                if (serial_reconnect_attempts < SERIAL_MAX_RECONNECT_ATTEMPTS) {
                    serial_reconnect_attempts++;
                    logging_warning("Serial port init failed, retry %d/%d in %d seconds",
                                   serial_reconnect_attempts, SERIAL_MAX_RECONNECT_ATTEMPTS, reconnect_delay);
                    sleep(reconnect_delay);
                    reconnect_delay *= 2; // Exponential backoff
                    if (reconnect_delay > SERIAL_MAX_RECONNECT_DELAY) {
                        reconnect_delay = SERIAL_MAX_RECONNECT_DELAY;
                    }
                    continue;
                }
                logging_error("Failed to initialize serial port after %d attempts, continuing with error reporting",
                             SERIAL_MAX_RECONNECT_ATTEMPTS);
            } else {
                logging_info("Serial port initialized successfully");
                serial_reconnect_attempts = 0;
                reconnect_delay = SERIAL_INITIAL_RECONNECT_DELAY;
            }
        }
        
        // Read temperature if serial port is available
        if (g_serial_fd >= 0) {
            char response[MAX_RESPONSE];
            if (send_at_command(g_serial_fd, AT_COMMAND, response, sizeof(response)) > 0) {
                // Process temperature response
                logging_debug("Raw AT+QTEMP response length: %zu bytes", strlen(response));
                logging_debug("Raw AT+QTEMP response: %s", response);
                int modem_temp = 0, ap_temp = 0, pa_temp = 0;
                if (extract_temp_values(response, &modem_temp, &ap_temp, &pa_temp,
                                       loop_config.temp_modem_prefix, loop_config.temp_ap_prefix, loop_config.temp_pa_prefix)) {
                    // Find highest temperature
                    int best_temp = modem_temp;
                    if (ap_temp > best_temp) best_temp = ap_temp;
                    if (pa_temp > best_temp) best_temp = pa_temp;

                    // Validate temperature range before conversion to prevent overflow
                    if (best_temp < (TEMP_ABSOLUTE_MIN / 1000) || best_temp > (TEMP_ABSOLUTE_MAX / 1000)) {
                        logging_warning("Temperature %d°C out of valid range (%d to %d°C), skipping",
                                       best_temp, TEMP_ABSOLUTE_MIN / 1000, TEMP_ABSOLUTE_MAX / 1000);
                        g_stats.parse_errors++;
                        continue;
                    }

                    // Convert to millidegrees and write to kernel interfaces
                    int best_temp_mdeg = best_temp * 1000;
                    char temp_str[32];
                    if (snprintf(temp_str, sizeof(temp_str), "%d", best_temp_mdeg) >= sizeof(temp_str)) {
                        logging_warning("Temperature millidegree string truncated, using fallback: N/A");
                        SAFE_STRNCPY(temp_str, "N/A", sizeof(temp_str));
                    }
                    
                    // Write to various kernel interfaces
                    logging_debug("Temperature: %d°C (modem: %d°C, AP: %d°C, PA: %d°C)", 
                               best_temp, modem_temp, ap_temp, pa_temp);
                    
                    // Increment successful read counter
                    g_stats.successful_reads++;

                    // Write to main sysfs interface (primary interface for CLI tool)
                    if (access("/sys/kernel/quectel_rm520n_thermal/temp", W_OK) == 0) {
                        FILE *fp = fopen("/sys/kernel/quectel_rm520n_thermal/temp", "w");
                        if (fp) {
                            fprintf(fp, "%d", best_temp_mdeg);
                            fclose(fp);
                            logging_debug("Wrote temperature to main sysfs interface: %d m°C", best_temp_mdeg);
                        } else {
                            logging_warning("Failed to open main sysfs interface for writing");
                        }
                    } else {
                        logging_debug("Main sysfs interface not writable: /sys/kernel/quectel_rm520n_thermal/temp");
                    }
                    
                    // Write to hwmon interface (for system monitoring tools)
                    if (hwmon_available && access(hwmon_path, W_OK) == 0) {
                        FILE *fp = fopen(hwmon_path, "w");
                        if (fp) {
                            fprintf(fp, "%d", best_temp_mdeg);
                            fclose(fp);
                            logging_debug("Wrote temperature to hwmon interface: %d m°C", best_temp_mdeg);
                        } else {
                            logging_warning("Failed to open hwmon interface for writing: %s", hwmon_path);
                        }
                    } else {
                        logging_debug("Hwmon interface not available or not writable: %s", hwmon_available ? hwmon_path : "not found");
                    }
                    
                    // Write to platform device interface if available
                    char platform_path[PLATFORM_PATH_LEN];  // Reduced from 256 - platform paths are short
                    if (snprintf(platform_path, sizeof(platform_path), "/sys/devices/platform/quectel_rm520n_temp/cur_temp") >= sizeof(platform_path)) {
                        logging_warning("Platform path truncated, device write skipped");
                    } else {
                        if (access(platform_path, W_OK) == 0) {
                            FILE *fp = fopen(platform_path, "w");
                            if (fp) {
                                fprintf(fp, "%d", best_temp_mdeg);
                                fclose(fp);
                                logging_debug("Wrote temperature to platform device: %s", platform_path);
                            }
                        } else {
                            logging_debug("Platform device not available: %s", platform_path);
                        }
                    }
                    
                    // Write to platform sensor interface if available
                    if (snprintf(platform_path, sizeof(platform_path), "/sys/devices/platform/soc/soc:quectel-temp-sensor/cur_temp") >= sizeof(platform_path)) {
                        logging_warning("Platform sensor path truncated, sensor write skipped");
                    } else {
                        if (access(platform_path, W_OK) == 0) {
                            FILE *fp = fopen(platform_path, "w");
                            if (fp) {
                                fprintf(fp, "%d", best_temp_mdeg);
                                fclose(fp);
                                logging_debug("Wrote temperature to platform sensor: %s", platform_path);
                            }
                        } else {
                            logging_debug("Platform sensor not available: %s", platform_path);
                        }
                    }
                    
                    // Write to thermal zone if available (for DTS integration)
                    // Find the thermal zone that belongs to our modem
                    DIR *thermal_dir = opendir("/sys/devices/virtual/thermal");
                    if (thermal_dir) {
                        struct dirent *entry;
                        while ((entry = readdir(thermal_dir)) != NULL) {
                            if (strncmp(entry->d_name, "thermal_zone", 12) == 0) {
                                char type_path[PATH_MAX_LEN];  // Must accommodate entry->d_name (up to 255 chars) + path prefix
                                char temp_path[PATH_MAX_LEN];  // Must accommodate entry->d_name (up to 255 chars) + path prefix
                                if (snprintf(type_path, sizeof(type_path), "/sys/devices/virtual/thermal/%s/type", entry->d_name) >= sizeof(type_path)) {
                                    logging_warning("Thermal zone type path truncated, skipping: %s", entry->d_name);
                                    continue;
                                }
                                if (snprintf(temp_path, sizeof(temp_path), "/sys/devices/virtual/thermal/%s/temp", entry->d_name) >= sizeof(temp_path)) {
                                    logging_warning("Thermal zone temp path truncated, skipping: %s", entry->d_name);
                                    continue;
                                }
                                
                                // Check if this thermal zone is for our modem
                                FILE *type_fp = fopen(type_path, "r");
                                if (type_fp) {
                                    char zone_type[DEVICE_NAME_LEN];
                                    if (fgets(zone_type, sizeof(zone_type), type_fp) != NULL) {
                                        zone_type[strcspn(zone_type, "\n")] = '\0';
                                        
                                        // Log all thermal zones for debugging
                                        logging_debug("Checking thermal zone: %s (type: %s)", entry->d_name, zone_type);
                                        
                                        // Safety check: Never write to CPU, GPU, or system thermal zones
                                        if (strstr(zone_type, "cpu") != NULL || 
                                            strstr(zone_type, "gpu") != NULL ||
                                            strstr(zone_type, "soc") != NULL ||
                                            strstr(zone_type, "board") != NULL) {
                                            logging_debug("Skipping system thermal zone: %s (type: %s)", entry->d_name, zone_type);
                                            fclose(type_fp);
                                            continue;
                                        }
                                        
                                        // Only write to thermal zones that are specifically for modem/Quectel
                                        // Avoid CPU, GPU, or other system thermal zones
                                        if (strcmp(zone_type, "quectel_rm520n") == 0 || 
                                            strcmp(zone_type, "modem_thermal") == 0 ||
                                            strcmp(zone_type, "modem-thermal") == 0 ||
                                            strcmp(zone_type, "quectel-thermal") == 0 ||
                                            strcmp(zone_type, "rm520n-thermal") == 0) {
                                            
                                            fclose(type_fp);
                                            logging_debug("Found modem thermal zone: %s (type: %s)", entry->d_name, zone_type);
                                            
                                            // Found our modem thermal zone, write temperature
                                            if (access(temp_path, W_OK) == 0) {
                                                FILE *temp_fp = fopen(temp_path, "w");
                                                if (temp_fp) {
                                                    fprintf(temp_fp, "%d", best_temp_mdeg);
                                                    fclose(temp_fp);
                                                    logging_info("Wrote temperature to modem thermal zone: %s", temp_path);
                                                }
                                            } else {
                                                logging_warning("Modem thermal zone temp file not writable: %s", temp_path);
                                            }
                                            break; // Found our zone, no need to check others
                                        } else {
                                            logging_debug("Skipping non-modem thermal zone: %s (type: %s)", entry->d_name, zone_type);
                                        }
                                    }
                                    fclose(type_fp);
                                }
                            }
                        }
                        closedir(thermal_dir);
                    }
                } else {
                    // Temperature parsing failed
                    g_stats.parse_errors++;
                    logging_warning("Failed to parse temperature from AT response");
                }
            } else {
                // AT command failed
                g_stats.at_command_errors++;
                logging_warning("AT command communication failed");

                // Handle serial communication errors
                if (++serial_reconnect_attempts > 3) {
                    logging_warning("Multiple AT command failures, reopening serial port");
                    close(g_serial_fd);
                    g_serial_fd = -1;
                    serial_reconnect_attempts = 0;
                }
            }
        }

        // Log statistics periodically
        if (g_stats.total_iterations % STATS_LOG_INTERVAL == 0) {
            double success_rate = g_stats.total_iterations > 0
                ? (100.0 * g_stats.successful_reads / g_stats.total_iterations)
                : 0.0;
            logging_info("Daemon statistics: iterations=%lu, successful=%lu (%.1f%%), "
                        "serial_errors=%lu, at_errors=%lu, parse_errors=%lu",
                        g_stats.total_iterations, g_stats.successful_reads, success_rate,
                        g_stats.serial_errors, g_stats.at_command_errors, g_stats.parse_errors);
        }

        // Wait for next interval (use config instead of loop_config which is out of scope)
        sleep(config.interval);
    }

    // Cleanup
    if (g_serial_fd >= 0) {
        close(g_serial_fd);
        g_serial_fd = -1;
    }

    release_daemon_lock();
    logging_info("Daemon shutdown complete");
    return 0;
}
