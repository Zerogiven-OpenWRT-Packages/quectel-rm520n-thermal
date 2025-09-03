/**
 * @file cli.c
 * @brief CLI mode implementation for Quectel RM520N thermal management
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This file contains the CLI mode implementation for reading temperatures
 * either from the daemon's output files or directly via AT commands.
 * Implements smart fallback logic and proper error handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "logging.h"
#include "config.h"
#include "serial.h"
#include "temperature.h"
#include "system.h"

/* Helper macro for safe string copying with null termination */
#define SAFE_STRNCPY(dst, src, size) do { \
    strncpy(dst, src, size - 1); \
    dst[size - 1] = '\0'; \
} while(0)

/* External variables from main.c */
extern config_t config;

/* ============================================================================
 * CONSTANTS & CONFIGURATION
 * ============================================================================ */

#define MAX_RESPONSE 1024
#define AT_COMMAND "AT+QTEMP\r"

/* ============================================================================
 * CLI MODE IMPLEMENTATION
 * ============================================================================ */

/**
 * CLI mode - read temperature with smart fallback strategy
 * 
 * Implements intelligent temperature reading that first attempts to read
 * from the daemon's hwmon interface, then falls back to direct AT commands
 * if the daemon is not available.
 * 
 * SMART READING STRATEGY:
 * 1. Check if daemon is running
 * 2. If running: read from hwmon interface (fast, reliable)
 * 3. If not running: fall back to direct AT commands (slower but always available)
 * 
 * Includes comprehensive error handling, logging, and JSON output support.
 * Following clig.dev guidelines for robust CLI behavior and user feedback.
 * 
 * @param temp_str Output buffer for temperature string
 * @param temp_size Size of temp_str buffer
 * @return 0 on success, 1 on error
 */
int cli_mode(char *temp_str, size_t temp_size)
{
    const char *status = "ok";
    int fd = -1;
    
    // Reload UCI configuration to get current settings
    if (config_read_uci(&config) == 0) {
        logging_debug("UCI configuration loaded: port=%s, baud=%d", config.serial_port, (int)config.baud_rate);
    } else {
        logging_warning("Failed to load UCI configuration, using defaults");
    }
    
    // Initialize temp_str with default value
    strncpy(temp_str, "N/A", temp_size - 1);
    temp_str[temp_size - 1] = '\0';

    // First, try to read temperature from daemon's output files
    logging_debug("Attempting to read temperature from daemon output...");
    
    // Check if daemon is running first
    if (check_daemon_running()) {
        logging_debug("Daemon is running, attempting to read from daemon interfaces...");
        
        // Try to read from main sysfs interface first (primary interface)
        if (access("/sys/kernel/quectel_rm520n/temp", R_OK) == 0) {
            FILE *temp_fp = fopen("/sys/kernel/quectel_rm520n/temp", "r");
            if (temp_fp) {
                if (fgets(temp_str, sizeof(temp_str), temp_fp) != NULL) {
                    temp_str[strcspn(temp_str, "\n")] = '\0';
                    
                    if (strcmp(temp_str, "N/A") != 0 && strcmp(temp_str, "0") != 0) {
                        status = "ok";
                        logging_debug("Temperature read from main sysfs interface: '%s'", temp_str);
                        logging_debug("Using temperature from daemon");
                        fclose(temp_fp);
                        goto output_result;
                    }
                }
                fclose(temp_fp);
            }
        } else {
            logging_debug("Main sysfs interface not available: /sys/kernel/quectel_rm520n/temp");
        }
        
        // Fall back to hwmon interface if main interface not available
        logging_debug("Main interface not available, trying hwmon...");
        // Try reading from hwmon interface
        DIR *hwmon_dir = opendir("/sys/class/hwmon");
        if (hwmon_dir) {
            struct dirent *entry;
            while ((entry = readdir(hwmon_dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;
                    
                char name_path[256];  // Must accommodate entry->d_name (up to 255 chars) + path prefix
                char temp_path[256];  // Must accommodate entry->d_name (up to 255 chars) + path prefix
                if (snprintf(name_path, sizeof(name_path), "/sys/class/hwmon/%s/name", entry->d_name) >= sizeof(name_path)) {
                    logging_warning("HWMON name path truncated, device skipped: %s", entry->d_name);
                    continue;
                }
                if (snprintf(temp_path, sizeof(temp_path), "/sys/class/hwmon/%s/temp1_input", entry->d_name) >= sizeof(temp_path)) {
                    logging_warning("HWMON temp path truncated, device skipped: %s", entry->d_name);
                    continue;
                }
                
                FILE *name_fp = fopen(name_path, "r");
                if (name_fp) {
                    char dev_name[32];  // Reduced from 64 - device names are typically short
                    if (fgets(dev_name, sizeof(dev_name), name_fp) != NULL) {
                        dev_name[strcspn(dev_name, "\n")] = '\0';
                        logging_debug("Checking hwmon device: %s (name: '%s')", entry->d_name, dev_name);
                        if (strcmp(dev_name, "quectel_rm520n") == 0 || strcmp(dev_name, "quectel_rm520n_hwmon") == 0) {
                            fclose(name_fp);
                            logging_debug("Found quectel_rm520n hwmon device: %s", entry->d_name);
                            
                            // Found the hwmon device, read temperature
                            FILE *temp_fp = fopen(temp_path, "r");
                            if (temp_fp) {
                                if (fgets(temp_str, sizeof(temp_str), temp_fp) != NULL) {
                                    temp_str[strcspn(temp_str, "\n")] = '\0';
                                    
                                    if (strcmp(temp_str, "N/A") != 0 && strcmp(temp_str, "0") != 0) {
                                        status = "ok";
                                        logging_debug("Temperature read from hwmon: '%s'", temp_str);
                                        logging_debug("Using temperature from daemon");
                                        fclose(temp_fp);
                                        closedir(hwmon_dir);
                                        goto output_result;
                                    }
                                }
                                fclose(temp_fp);
                            }
                            break;
                        }
                    }
                    fclose(name_fp);
                }
            }
            closedir(hwmon_dir);
        }
    }
    
    // If we get here, daemon is not running or has no valid temperature
    // Fall back to direct AT command method
    logging_debug("Daemon not available, falling back to direct AT command...");
    
    // Read temperature via AT command
    fd = init_serial_port(config.serial_port, config.baud_rate);
    if (fd < 0) {
        status = "Error: Serial port not available";
        logging_debug("Serial port open failed: %s", config.serial_port);
        goto output_result;
    }

    logging_debug("Serial port opened successfully, fd=%d", fd);
    logging_debug("Sending AT command: %s", AT_COMMAND);

    char response[MAX_RESPONSE];
    if (send_at_command(fd, AT_COMMAND, response, sizeof(response)) > 0) {
        logging_debug("AT command sent successfully, response length: %zu", strlen(response));
        int modem_temp, ap_temp, pa_temp;
                        if (extract_temp_values(response, &modem_temp, &ap_temp, &pa_temp,
                                       config.temp_modem_prefix, config.temp_ap_prefix, config.temp_pa_prefix) == 1) {
            // extract_temp_values returns 1 on success
            // Use the highest temperature value (same logic as daemon)
            int best_temp = modem_temp;
            if (ap_temp > best_temp) best_temp = ap_temp;
            if (pa_temp > best_temp) best_temp = pa_temp;
            
            // Convert to millidegrees (same format as daemon output)
            int best_temp_mdeg = best_temp * 1000;
            if (snprintf(temp_str, sizeof(temp_str), "%d", best_temp_mdeg) >= sizeof(temp_str)) {
                logging_warning("Temperature string truncated, using fallback: N/A");
                SAFE_STRNCPY(temp_str, "N/A", sizeof(temp_str));
            }
            logging_debug("Temperature parsed successfully: %s°C (modem: %d°C, AP: %d°C, PA: %d°C)", 
                         temp_str, modem_temp, ap_temp, pa_temp);
        } else {
            status = "Error: Invalid response format";
            SAFE_STRNCPY(temp_str, "N/A", sizeof(temp_str));
            logging_debug("Temperature parsing failed: invalid response format");
        }
    } else {
        status = "Error: Communication failed";
        logging_debug("AT command communication failed: no response received");
    }

    close_serial_port(fd);
    logging_debug("Serial port closed");

output_result:
    // Note: json_output is handled in main.c, this function focuses on CLI logic
    if (strcmp(status, "ok") == 0) {
        return 0;  // Success
    } else {
        return 1;  // Error
        }
    }
