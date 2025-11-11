/**
 * @file uci_config.c
 * @brief UCI configuration functions for kernel module temperature thresholds
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This module provides UCI configuration functions for updating kernel module
 * temperature thresholds via sysfs interfaces. Integrated into the main binary
 * as a subcommand.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include "logging.h"

/* ============================================================================
 * CONSTANTS & CONFIGURATION
 * ============================================================================ */

#define SYSFS_BASE "/sys/kernel/quectel_rm520n"
#define HWMON_BASE "/sys/class/hwmon"
#define UCI_CONFIG "quectel_rm520n_thermal"
#define UCI_SECTION "settings"

/* Temperature threshold options in UCI */
#define UCI_TEMP_MIN "temp_min"
#define UCI_TEMP_MAX "temp_max"
#define UCI_TEMP_CRIT "temp_crit"
#define UCI_TEMP_DEFAULT "temp_default"

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

/**
 * Extract hwmon device number from directory name
 * 
 * @param dir_name Directory name (e.g., "hwmon3")
 * @return Device number on success, -1 on error
 */
static int extract_hwmon_number(const char *dir_name)
{
    if (!dir_name) return -1;
    
    // Extract numeric part from "hwmon3" -> "3"
    const char *num_start = strpbrk(dir_name, "0123456789");
    if (num_start) {
        int num = atoi(num_start);
        if (num >= 0) {
            return num;
        }
    }
    return -1;
}

/* ============================================================================
 * UCI CONFIGURATION FUNCTIONS
 * ============================================================================ */

/**
 * Read a UCI configuration value
 * 
 * @param option Option name to read
 * @param buffer Buffer to store the value
 * @param buffer_size Size of the buffer
 * @return 0 on success, -1 on error
 */
static int read_uci_option(const char *option, char *buffer, size_t buffer_size)
{
    char command[256];
    FILE *fp;
    
    // Build uci get command
    if (snprintf(command, sizeof(command), "uci -q get %s.%s.%s", 
                 UCI_CONFIG, UCI_SECTION, option) >= sizeof(command)) {
        fprintf(stderr, "Error: UCI command too long\n");
        return -1;
    }
    
    // Execute uci command
    fp = popen(command, "r");
    if (!fp) {
        fprintf(stderr, "Error: Failed to execute uci command\n");
        return -1;
    }
    
    // Read the result
    if (fgets(buffer, buffer_size, fp) == NULL) {
        pclose(fp);
        return -1; // Option not found or empty
    }
    
    pclose(fp);
    
    // Remove trailing newline
    buffer[strcspn(buffer, "\n")] = '\0';
    
    return 0;
}

/**
 * Convert Celsius to millidegrees Celsius
 * 
 * @param celsius Temperature in Celsius
 * @return Temperature in millidegrees Celsius
 */
static int celsius_to_millidegrees(const char *celsius_str)
{
    float celsius = atof(celsius_str);
    return (int)(celsius * 1000.0f);
}

/* ============================================================================
 * SYSFS INTERFACE FUNCTIONS
 * ============================================================================ */

/**
 * Find the Quectel hwmon device
 * 
 * @return hwmon device number on success, -1 on error
 */
static int find_quectel_hwmon_device(void)
{
    DIR *hwmon_dir;
    struct dirent *entry;
    char name_path[256];
    char dev_name[64];
    FILE *name_fp;
    int hwmon_num = -1;
    
    hwmon_dir = opendir(HWMON_BASE);
    if (!hwmon_dir) {
        return -1;
    }
    
    while ((entry = readdir(hwmon_dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
            
        if (snprintf(name_path, sizeof(name_path), "%s/%s/name", HWMON_BASE, entry->d_name) >= sizeof(name_path)) {
            continue;
        }
        
        name_fp = fopen(name_path, "r");
        if (name_fp) {
            if (fgets(dev_name, sizeof(dev_name), name_fp) != NULL) {
                dev_name[strcspn(dev_name, "\n")] = '\0';
                logging_debug("Found hwmon device: %s -> %s", entry->d_name, dev_name);
                if (strcmp(dev_name, "quectel_rm520n") == 0) {
                    // Verify this device actually has Quectel attributes
                    char verify_path[256];
                    if (snprintf(verify_path, sizeof(verify_path), "%s/%s/temp1_input", HWMON_BASE, entry->d_name) < sizeof(verify_path)) {
                        if (access(verify_path, R_OK) == 0) {
                            int selected_num = extract_hwmon_number(entry->d_name);
                            if (selected_num >= 0) {
                                logging_info("Selected Quectel hwmon device: %s -> extracted number %d (hwmon%d) - verified", 
                                           entry->d_name, selected_num, selected_num);
                                hwmon_num = selected_num;
                                fclose(name_fp);
                                break;
                            } else {
                                logging_warning("Failed to extract hwmon number from: %s", entry->d_name);
                            }
                        } else {
                            logging_debug("Found hwmon device with name 'quectel_rm520n' but no temp1_input: %s", entry->d_name);
                        }
                    }
                }
            }
            fclose(name_fp);
        }
    }
    
    closedir(hwmon_dir);
    
    // If no device found by name, try to find by attributes
    if (hwmon_num == -1) {
        logging_info("No hwmon device found by name, trying attribute-based detection...");
        DIR *attr_hwmon_dir = opendir(HWMON_BASE);
        if (attr_hwmon_dir) {
            struct dirent *attr_entry;
            while ((attr_entry = readdir(attr_hwmon_dir)) != NULL) {
                if (strcmp(attr_entry->d_name, ".") == 0 || strcmp(attr_entry->d_name, "..") == 0)
                    continue;
                    
                char attr_check_path[256];
                if (snprintf(attr_check_path, sizeof(attr_check_path), "%s/%s/temp1_input", HWMON_BASE, attr_entry->d_name) < sizeof(attr_check_path)) {
                    if (access(attr_check_path, R_OK) == 0) {
                        // Check if this device has Quectel-like attributes
                        char attr_name_path[256];
                        if (snprintf(attr_name_path, sizeof(attr_name_path), "%s/%s/name", HWMON_BASE, attr_entry->d_name) < sizeof(attr_name_path)) {
                            char attr_dev_name[64];
                            FILE *attr_name_fp = fopen(attr_name_path, "r");
                            if (attr_name_fp) {
                                if (fgets(attr_dev_name, sizeof(attr_dev_name), attr_name_fp) != NULL) {
                                    attr_dev_name[strcspn(attr_dev_name, "\n")] = '\0';
                                    if (strstr(attr_dev_name, "quectel") != NULL || strstr(attr_dev_name, "rm520n") != NULL) {
                                        int attr_selected_num = extract_hwmon_number(attr_entry->d_name);
                                        if (attr_selected_num >= 0) {
                                            logging_info("Found Quectel-like device by attributes: %s -> extracted number %d (hwmon%d)", 
                                                       attr_entry->d_name, attr_selected_num, attr_selected_num);
                                            hwmon_num = attr_selected_num;
                                            fclose(attr_name_fp);
                                            break;
                                        } else {
                                            logging_warning("Failed to extract hwmon number from: %s", attr_entry->d_name);
                                        }
                                    }
                                }
                                fclose(attr_name_fp);
                            }
                        }
                    }
                }
            }
            closedir(attr_hwmon_dir);
        }
    }
    
    logging_info("find_quectel_hwmon_device() returning: %d", hwmon_num);
    return hwmon_num;
}

/**
 * Write value to sysfs file
 * 
 * @param filename Sysfs filename
 * @param value Value to write
 * @return 0 on success, -1 on error
 */
static int write_sysfs_value(const char *filename, int value)
{
    char path[256];
    FILE *fp;
    
    // Build full path
    if (snprintf(path, sizeof(path), "%s/%s", SYSFS_BASE, filename) >= sizeof(path)) {
        logging_warning("Sysfs path truncated, write skipped");
        return -1;
    }
    
    // Check if file exists
    if (access(path, W_OK) != 0) {
        logging_debug("Sysfs file not writable: %s", path);
        return -1;
    }
    
    // Write value
    fp = fopen(path, "w");
    if (!fp) {
        logging_error("Failed to open sysfs file: %s", path);
        return -1;
    }
    
    if (fprintf(fp, "%d", value) < 0) {
        logging_error("Failed to write to sysfs file: %s", path);
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    logging_info("Updated %s to %d m°C (%0.1f°C)", filename, value, value / 1000.0f);
    return 0;
}

/**
 * Read current value from sysfs file
 * 
 * @param filename Sysfs filename
 * @return Current value on success, -1 on error
 */
static int read_sysfs_value(const char *filename)
{
    char path[256];
    FILE *fp;
    int value;
    
    // Build full path
    if (snprintf(path, sizeof(path), "%s/%s", SYSFS_BASE, filename) >= sizeof(path)) {
        logging_warning("Sysfs path truncated, read skipped");
        return -1;
    }
    
    // Check if file exists
    if (access(path, R_OK) != 0) {
        logging_debug("Sysfs file not readable: %s", path);
        return -1;
    }
    
    // Read value
    fp = fopen(path, "r");
    if (!fp) {
        logging_error("Failed to open sysfs file: %s", path);
        return -1;
    }
    
    if (fscanf(fp, "%d", &value) != 1) {
        logging_error("Failed to read from sysfs file: %s", path);
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return value;
}

/* ============================================================================
 * MAIN UCI CONFIG FUNCTION
 * ============================================================================ */

/**
 * UCI configuration mode - Update kernel module thresholds from UCI config
 * 
 * Reads UCI configuration and updates kernel module temperature thresholds
 * via sysfs interfaces. Provides a bridge between OpenWRT UCI configuration
 * and kernel module parameters.
 * 
 * @return 0 on success, 1 on error
 */
int uci_config_mode(void)
{
    char uci_value[64];
    int temp_min, temp_max, temp_crit, temp_default;
    int current_min, current_max, current_crit, current_default;
    int updated = 0;
    
    logging_info("Updating kernel module thresholds from UCI config");
    
    // Check if kernel module is loaded
    if (access(SYSFS_BASE, F_OK) != 0) {
        logging_error("Kernel module not loaded or sysfs not available: %s", SYSFS_BASE);
        logging_error("Please load the quectel_rm520n_temp kernel module first");
        return 1;
    }
    
    // Read current sysfs values
    current_min = read_sysfs_value("temp_min");
    current_max = read_sysfs_value("temp_max");
    current_crit = read_sysfs_value("temp_crit");
    current_default = read_sysfs_value("temp_default");
    
    logging_info("Current kernel module thresholds:");
    logging_info("  temp_min: %d m°C (%0.1f°C)", current_min, current_min / 1000.0f);
    logging_info("  temp_max: %d m°C (%0.1f°C)", current_max, current_max / 1000.0f);
    logging_info("  temp_crit: %d m°C (%0.1f°C)", current_crit, current_crit / 1000.0f);
    logging_info("  temp_default: %d m°C (%0.1f°C)", current_default, current_default / 1000.0f);
    
    // Read UCI configuration and update if different
    logging_info("Reading UCI configuration...");

    // Read all temperature thresholds first for validation
    int uci_temp_min = current_min;
    int uci_temp_max = current_max;
    int uci_temp_crit = current_crit;
    int uci_temp_default = current_default;

    if (read_uci_option(UCI_TEMP_MIN, uci_value, sizeof(uci_value)) == 0) {
        uci_temp_min = celsius_to_millidegrees(uci_value);
        logging_info("UCI temp_min: %s°C -> %d m°C", uci_value, uci_temp_min);
    }

    if (read_uci_option(UCI_TEMP_MAX, uci_value, sizeof(uci_value)) == 0) {
        uci_temp_max = celsius_to_millidegrees(uci_value);
        logging_info("UCI temp_max: %s°C -> %d m°C", uci_value, uci_temp_max);
    }

    if (read_uci_option(UCI_TEMP_CRIT, uci_value, sizeof(uci_value)) == 0) {
        uci_temp_crit = celsius_to_millidegrees(uci_value);
        logging_info("UCI temp_crit: %s°C -> %d m°C", uci_value, uci_temp_crit);
    }

    if (read_uci_option(UCI_TEMP_DEFAULT, uci_value, sizeof(uci_value)) == 0) {
        uci_temp_default = celsius_to_millidegrees(uci_value);
        logging_info("UCI temp_default: %s°C -> %d m°C", uci_value, uci_temp_default);
    }

    // Validate temperature thresholds
    if (uci_temp_min >= uci_temp_max) {
        logging_error("Invalid UCI config: temp_min (%d m°C) must be less than temp_max (%d m°C)",
                     uci_temp_min, uci_temp_max);
        logging_error("Keeping current thresholds unchanged");
        return 1;
    }

    if (uci_temp_max >= uci_temp_crit) {
        logging_error("Invalid UCI config: temp_max (%d m°C) must be less than temp_crit (%d m°C)",
                     uci_temp_max, uci_temp_crit);
        logging_error("Keeping current thresholds unchanged");
        return 1;
    }

    // Thresholds are valid, now update if different
    temp_min = uci_temp_min;
    temp_max = uci_temp_max;
    temp_crit = uci_temp_crit;
    temp_default = uci_temp_default;

    if (temp_min != current_min) {
        if (write_sysfs_value("temp_min", temp_min) == 0) {
            updated++;
        }
    }

    if (temp_max != current_max) {
        if (write_sysfs_value("temp_max", temp_max) == 0) {
            updated++;
        }
    }

    if (temp_crit != current_crit) {
        if (write_sysfs_value("temp_crit", temp_crit) == 0) {
            updated++;
        }
        logging_info("Updating temp_crit from %d to %d m°C", current_crit, temp_crit);
    }

    if (temp_default != current_default) {
        if (write_sysfs_value("temp_default", temp_default) == 0) {
            updated++;
        }
    }
    
    if (updated > 0) {
        logging_info("Updated %d threshold(s) from UCI configuration", updated);
    } else {
        logging_info("All thresholds are already up-to-date");
    }
    
    // Also update hwmon device if available
    logging_info("Checking hwmon device...");
    
    // List all available hwmon devices for debugging (debug level only)
    logging_debug("Available hwmon devices:");
    DIR *debug_hwmon_dir = opendir(HWMON_BASE);
    if (debug_hwmon_dir) {
        struct dirent *debug_entry;
        while ((debug_entry = readdir(debug_hwmon_dir)) != NULL) {
            if (strcmp(debug_entry->d_name, ".") == 0 || strcmp(debug_entry->d_name, "..") == 0)
                continue;
                
            char debug_name_path[256];
            if (snprintf(debug_name_path, sizeof(debug_name_path), "%s/%s/name", HWMON_BASE, debug_entry->d_name) < sizeof(debug_name_path)) {
                char debug_dev_name[64];
                FILE *debug_name_fp = fopen(debug_name_path, "r");
                if (debug_name_fp) {
                    if (fgets(debug_dev_name, sizeof(debug_dev_name), debug_name_fp) != NULL) {
                        debug_dev_name[strcspn(debug_dev_name, "\n")] = '\0';
                        logging_debug("  %s -> %s", debug_entry->d_name, debug_dev_name);
                    }
                    fclose(debug_name_fp);
                }
            }
        }
        closedir(debug_hwmon_dir);
    }
    
            int hwmon_num = find_quectel_hwmon_device();
        logging_debug("find_quectel_hwmon_device() returned: %d", hwmon_num);
        
        // Check for manual override via environment variable
        const char *manual_hwmon = getenv("QUECTEL_HWMON_OVERRIDE");
        if (manual_hwmon) {
            int manual_num = atoi(manual_hwmon);
            logging_info("Manual hwmon override: using hwmon%d (from QUECTEL_HWMON_OVERRIDE=%s)", manual_num, manual_hwmon);
            hwmon_num = manual_num;
        }
        
        logging_debug("Final hwmon_num value: %d", hwmon_num);
    
            if (hwmon_num >= 0) {
            logging_info("Found Quectel hwmon device: hwmon%d", hwmon_num);
            logging_debug("Hwmon base path: %s", HWMON_BASE);
        
        // Verify this device has Quectel attributes and is writable
        char verify_path[256];
        if (snprintf(verify_path, sizeof(verify_path), "%s/hwmon%d/temp1_input", HWMON_BASE, hwmon_num) < sizeof(verify_path)) {
            if (access(verify_path, R_OK) == 0) {
                logging_info("Verified: hwmon%d has Quectel attributes", hwmon_num);
                
                        // Check if the device is writable
        char write_test_path[256];
        if (snprintf(write_test_path, sizeof(write_test_path), "%s/hwmon%d/temp1_crit", HWMON_BASE, hwmon_num) < sizeof(write_test_path)) {
            if (access(write_test_path, W_OK) == 0) {
                logging_info("Verified: hwmon%d is writable", hwmon_num);
                
                // Check actual file permissions
                struct stat st;
                if (stat(write_test_path, &st) == 0) {
                    logging_info("File permissions: %o (owner: %d, group: %d)", 
                               st.st_mode & 0777, st.st_uid, st.st_gid);
                }
            } else {
                logging_warning("Warning: hwmon%d is not writable", hwmon_num);
                logging_warning("File not writable: %s", write_test_path);
            }
        }
            } else {
                logging_warning("Warning: hwmon%d does not have Quectel attributes", hwmon_num);
                logging_warning("Expected file not found: %s", verify_path);
            }
        }
        
        // Update hwmon thresholds
        char hwmon_path[256];
        int hwmon_updated = 0;
        
        // Read current hwmon values for comparison (debug level)
        logging_debug("Current hwmon hwmon%d values:", hwmon_num);
        if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_min", HWMON_BASE, hwmon_num) < sizeof(hwmon_path)) {
            FILE *fp = fopen(hwmon_path, "r");
            if (fp) {
                int current_val;
                if (fscanf(fp, "%d", &current_val) == 1) {
                    logging_debug("  temp1_min: %d m°C (%0.1f°C)", current_val, current_val / 1000.0f);
                }
                fclose(fp);
            }
        }
        
        if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_max", HWMON_BASE, hwmon_num) < sizeof(hwmon_path)) {
            FILE *fp = fopen(hwmon_path, "r");
            if (fp) {
                int current_val;
                if (fscanf(fp, "%d", &current_val) == 1) {
                    logging_info("  temp1_max: %d m°C (%0.1f°C)", current_val, current_val / 1000.0f);
                }
                fclose(fp);
            }
        }
        
        if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_crit", HWMON_BASE, hwmon_num) < sizeof(hwmon_path)) {
            FILE *fp = fopen(hwmon_path, "r");
            if (fp) {
                int current_val;
                if (fscanf(fp, "%d", &current_val) == 1) {
                    logging_info("  temp1_crit: %d m°C (%0.1f°C)", current_val, current_val / 1000.0f);
                }
                fclose(fp);
            }
        }
        
        // Update temp1_min
        if (read_uci_option(UCI_TEMP_MIN, uci_value, sizeof(uci_value)) == 0) {
            temp_min = celsius_to_millidegrees(uci_value);
            if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_min", HWMON_BASE, hwmon_num) < sizeof(hwmon_path)) {
                logging_info("Attempting to update hwmon temp1_min at: %s", hwmon_path);
                if (access(hwmon_path, W_OK) == 0) {
                    logging_info("File is writable, attempting to open: %s", hwmon_path);
                    FILE *fp = fopen(hwmon_path, "w");
                    if (fp) {
                        int write_result = fprintf(fp, "%d", temp_min);
                        if (write_result > 0) {
                            fclose(fp);
                            logging_info("Successfully updated hwmon temp1_min to %d m°C (%0.1f°C)", temp_min, temp_min / 1000.0f);
                            hwmon_updated++;
                        } else {
                            logging_error("Failed to write to file: %s (fprintf returned %d)", hwmon_path, write_result);
                            fclose(fp);
                        }
                    } else {
                        logging_error("Failed to open file for writing: %s (errno: %d)", hwmon_path, errno);
                        
                        // Try alternative method: shell command
                        logging_info("Attempting alternative update method using shell command...");
                        char shell_cmd[512];
                        if (snprintf(shell_cmd, sizeof(shell_cmd), "echo %d > %s", temp_min, hwmon_path) < sizeof(shell_cmd)) {
                            int cmd_result = system(shell_cmd);
                            if (cmd_result == 0) {
                                logging_info("Successfully updated hwmon temp1_min to %d m°C (%0.1f°C) via shell command", temp_min, temp_min / 1000.0f);
                                hwmon_updated++;
                            } else {
                                logging_error("Shell command failed with exit code %d", cmd_result);
                            }
                        }
                    }
                } else {
                    logging_warning("Hwmon temp1_min file not writable: %s (errno: %d)", hwmon_path, errno);
                }
            }
        }
        
        // Update temp1_max
        if (read_uci_option(UCI_TEMP_MAX, uci_value, sizeof(uci_value)) == 0) {
            temp_max = celsius_to_millidegrees(uci_value);
            if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_max", HWMON_BASE, hwmon_num) < sizeof(hwmon_path)) {
                logging_info("Attempting to update hwmon temp1_max at: %s", hwmon_path);
                if (access(hwmon_path, W_OK) == 0) {
                    logging_info("File is writable, attempting to open: %s", hwmon_path);
                    FILE *fp = fopen(hwmon_path, "w");
                    if (fp) {
                        int write_result = fprintf(fp, "%d", temp_max);
                        if (write_result > 0) {
                            fclose(fp);
                            logging_info("Successfully updated hwmon temp1_max to %d m°C (%0.1f°C)", temp_max, temp_max / 1000.0f);
                            hwmon_updated++;
                        } else {
                            logging_error("Failed to write to file: %s (fprintf returned %d)", hwmon_path, write_result);
                            fclose(fp);
                        }
                    } else {
                        logging_error("Failed to open file for writing: %s (errno: %d)", hwmon_path, errno);
                        
                        // Try alternative method: shell command
                        logging_info("Attempting alternative update method using shell command...");
                        char shell_cmd[512];
                        if (snprintf(shell_cmd, sizeof(shell_cmd), "echo %d > %s", temp_max, hwmon_path) < sizeof(shell_cmd)) {
                            int cmd_result = system(shell_cmd);
                            if (cmd_result == 0) {
                                logging_info("Successfully updated hwmon temp1_max to %d m°C (%0.1f°C) via shell command", temp_max, temp_max / 1000.0f);
                                hwmon_updated++;
                            } else {
                                logging_error("Shell command failed with exit code %d", cmd_result);
                            }
                        }
                    }
                } else {
                    logging_warning("Hwmon temp1_max file not writable: %s (errno: %d)", hwmon_path, errno);
                }
            }
        }
        
        // Update temp1_crit
        if (read_uci_option(UCI_TEMP_CRIT, uci_value, sizeof(uci_value)) == 0) {
            temp_crit = celsius_to_millidegrees(uci_value);
            if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_crit", HWMON_BASE, hwmon_num) < sizeof(hwmon_path)) {
                logging_info("Attempting to update hwmon temp1_crit at: %s", hwmon_path);
                if (access(hwmon_path, W_OK) == 0) {
                    logging_info("File is writable, attempting to open: %s", hwmon_path);
                    FILE *fp = fopen(hwmon_path, "w");
                    if (fp) {
                        int write_result = fprintf(fp, "%d", temp_crit);
                        if (write_result > 0) {
                            fclose(fp);
                            logging_info("Successfully updated hwmon temp1_crit to %d m°C (%0.1f°C)", temp_crit, temp_crit / 1000.0f);
                            hwmon_updated++;
                        } else {
                            logging_error("Failed to write to file: %s (fprintf returned %d)", hwmon_path, write_result);
                            fclose(fp);
                        }
                    } else {
                        logging_error("Failed to open file for writing: %s (errno: %d)", hwmon_path, errno);
                        
                        // Try alternative method: shell command
                        logging_info("Attempting alternative update method using shell command...");
                        char shell_cmd[512];
                        if (snprintf(shell_cmd, sizeof(shell_cmd), "echo %d > %s", temp_crit, hwmon_path) < sizeof(shell_cmd)) {
                            int cmd_result = system(shell_cmd);
                            if (cmd_result == 0) {
                                logging_info("Successfully updated hwmon temp1_crit to %d m°C (%0.1f°C) via shell command", temp_crit, temp_crit / 1000.0f);
                                hwmon_updated++;
                            } else {
                                logging_error("Shell command failed with exit code %d", cmd_result);
                            }
                        }
                    }
                } else {
                    logging_warning("Hwmon temp1_crit file not writable: %s (errno: %d)", hwmon_path, errno);
                }
            }
        }
        
        // Summary of hwmon updates
        if (hwmon_updated > 0) {
            logging_info("Successfully updated %d hwmon threshold(s) in hwmon%d", hwmon_updated, hwmon_num);
        } else {
            logging_warning("No hwmon thresholds were updated in hwmon%d", hwmon_num);
        }
                    // Try updating via main sysfs interface as fallback
        logging_debug("Attempting fallback update via main sysfs interface...");
        int fallback_updated = 0;
        
        // Check main sysfs permissions and accessibility first
        char main_sysfs_path[256];
        if (snprintf(main_sysfs_path, sizeof(main_sysfs_path), "%s/temp_crit", SYSFS_BASE) < sizeof(main_sysfs_path)) {
            struct stat main_st;
            if (stat(main_sysfs_path, &main_st) == 0) {
                logging_debug("Main sysfs temp_crit permissions: %o (owner: %d, group: %d)", 
                           (int)(main_st.st_mode & 0777), (int)main_st.st_uid, (int)main_st.st_gid);
                
                // Check if the file is actually writable
                if (access(main_sysfs_path, W_OK) == 0) {
                    logging_debug("Main sysfs temp_crit is writable");
                } else {
                    logging_warning("Main sysfs temp_crit is not writable (errno: %d)", errno);
                }
            } else {
                logging_error("Failed to stat main sysfs temp_crit: %s", main_sysfs_path);
            }
        }
        
        // Try to update all thresholds via main sysfs interface
        if (read_uci_option(UCI_TEMP_MIN, uci_value, sizeof(uci_value)) == 0) {
            temp_min = celsius_to_millidegrees(uci_value);
            if (write_sysfs_value("temp_min", temp_min) == 0) {
                logging_info("Fallback: Updated main sysfs temp_min to %d m°C (%0.1f°C)", temp_min, temp_min / 1000.0f);
                fallback_updated++;
            } else {
                logging_error("Fallback: Failed to update main sysfs temp_min");
            }
        }
        
        if (read_uci_option(UCI_TEMP_MAX, uci_value, sizeof(uci_value)) == 0) {
            temp_max = celsius_to_millidegrees(uci_value);
            if (write_sysfs_value("temp_max", temp_max) == 0) {
                logging_info("Fallback: Updated main sysfs temp_max to %d m°C (%0.1f°C)", temp_max, temp_max / 1000.0f);
                fallback_updated++;
            } else {
                logging_error("Fallback: Failed to update main sysfs temp_max");
            }
        }
        
        if (read_uci_option(UCI_TEMP_CRIT, uci_value, sizeof(uci_value)) == 0) {
            temp_crit = celsius_to_millidegrees(uci_value);
            if (write_sysfs_value("temp_crit", temp_crit) == 0) {
                logging_info("Fallback: Updated main sysfs temp_crit to %d m°C (%0.1f°C)", temp_crit, temp_crit / 1000.0f);
                fallback_updated++;
            } else {
                logging_error("Fallback: Failed to update main sysfs temp_crit");
            }
        }
        
        if (read_uci_option(UCI_TEMP_DEFAULT, uci_value, sizeof(uci_value)) == 0) {
            temp_default = celsius_to_millidegrees(uci_value);
            if (write_sysfs_value("temp_default", temp_default) == 0) {
                logging_info("Fallback: Updated main sysfs temp_default to %d m°C (%0.1f°C)", temp_default, temp_default / 1000.0f);
                fallback_updated++;
            } else {
                logging_error("Fallback: Failed to update main sysfs temp_default");
            }
        }
        
        if (fallback_updated > 0) {
            logging_info("Fallback: Successfully updated %d threshold(s) via main sysfs interface", fallback_updated);
            
            // Try to reload the kernel module to apply new thresholds to hwmon
            logging_debug("Attempting kernel module reload to apply new thresholds to hwmon...");
            char reload_cmd[512];
            if (snprintf(reload_cmd, sizeof(reload_cmd), "rmmod quectel_rm520n_temp_sensor_hwmon && insmod quectel_rm520n_temp_sensor_hwmon") < sizeof(reload_cmd)) {
                logging_debug("Executing: %s", reload_cmd);
                int reload_result = system(reload_cmd);
                if (reload_result == 0) {
                    logging_info("Successfully reloaded kernel module");
                    
                    // Wait a moment for the module to initialize
                    sleep(1);
                    
                    // Check if the new values are now visible in hwmon
                    if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_crit", HWMON_BASE, hwmon_num) < sizeof(hwmon_path)) {
                        FILE *fp = fopen(hwmon_path, "r");
                        if (fp) {
                            int new_val;
                            if (fscanf(fp, "%d", &new_val) == 1) {
                                logging_debug("After reload: hwmon temp1_crit = %d m°C (%0.1f°C)", new_val, new_val / 1000.0f);
                            }
                            fclose(fp);
                        }
                    }
                } else {
                    logging_error("Failed to reload kernel module (exit code: %d)", reload_result);
                }
            }
        } else {
            logging_warning("Fallback: No thresholds could be updated via any method");
        }
        
        // Final summary
        logging_info("=== UCI Configuration Update Summary ===");
        if (hwmon_updated > 0) {
            logging_info("✓ Hwmon interface: %d threshold(s) updated directly", hwmon_updated);
        } else {
            logging_info("✗ Hwmon interface: No thresholds updated (files are read-only hardware limits)");
        }
        
        if (fallback_updated > 0) {
            logging_info("✓ Main sysfs interface: %d threshold(s) updated", fallback_updated);
            logging_info("✓ Kernel module reload: Attempted to propagate changes to hwmon");
        } else {
            logging_info("✗ Main sysfs interface: No thresholds updated");
        }
        
        logging_info("Note: Hwmon temp1_* files are hardware-defined limits and cannot be written directly");
        logging_info("      Updates must go through the kernel module's main sysfs interface");
        logging_info("================================================");
    } else {
        logging_info("Quectel hwmon device not found");
        
        // Try alternative approach: look for any hwmon device with Quectel attributes
        logging_info("Trying alternative hwmon device detection...");
        DIR *alt_hwmon_dir = opendir(HWMON_BASE);
        if (alt_hwmon_dir) {
            struct dirent *alt_entry;
            while ((alt_entry = readdir(alt_hwmon_dir)) != NULL) {
                if (strcmp(alt_entry->d_name, ".") == 0 || strcmp(alt_entry->d_name, "..") == 0)
                    continue;
                    
                char alt_name_path[256];
                char alt_dev_name[64];
                if (snprintf(alt_name_path, sizeof(alt_name_path), "%s/%s/name", HWMON_BASE, alt_entry->d_name) < sizeof(alt_name_path)) {
                    FILE *alt_name_fp = fopen(alt_name_path, "r");
                    if (alt_name_fp) {
                        if (fgets(alt_dev_name, sizeof(alt_dev_name), alt_name_fp) != NULL) {
                            alt_dev_name[strcspn(alt_dev_name, "\n")] = '\0';
                            if (strcmp(alt_dev_name, "quectel_rm520n") == 0) {
                                int alt_hwmon_num = atoi(alt_entry->d_name);
                                logging_info("Alternative detection found: hwmon%d", alt_hwmon_num);
                                
                                // Try to update this alternative device
                                char alt_hwmon_path[256];
                                if (snprintf(alt_hwmon_path, sizeof(alt_hwmon_path), "%s/hwmon%d/temp1_crit", HWMON_BASE, alt_hwmon_num) < sizeof(alt_hwmon_path)) {
                                    if (access(alt_hwmon_path, W_OK) == 0) {
                                        logging_info("Alternative hwmon%d is writable, attempting update...", alt_hwmon_num);
                                        // Try to update temp1_crit as a test
                                        if (read_uci_option(UCI_TEMP_CRIT, uci_value, sizeof(uci_value)) == 0) {
                                            temp_crit = celsius_to_millidegrees(uci_value);
                                            FILE *alt_fp = fopen(alt_hwmon_path, "w");
                                            if (alt_fp) {
                                                fprintf(alt_fp, "%d", temp_crit);
                                                fclose(alt_fp);
                                                logging_info("Successfully updated alternative hwmon%d temp1_crit to %d m°C", alt_hwmon_num, temp_crit);
                                            }
                                        }
                                    }
                                }
                                break;
                            }
                        }
                        fclose(alt_name_fp);
                    }
                }
            }
            closedir(alt_hwmon_dir);
        }
    }
    
    return 0;
}
