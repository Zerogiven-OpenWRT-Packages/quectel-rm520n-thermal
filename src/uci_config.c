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
 *
 * ENVIRONMENT VARIABLES:
 *   QUECTEL_HWMON_OVERRIDE - Override automatic hwmon device detection.
 *     Set to the hwmon device number (e.g., "3" for hwmon3) to force
 *     the tool to use a specific hwmon device instead of auto-detecting.
 *     This is a SECURITY-SENSITIVE setting: only set this in trusted
 *     environments where you control the hwmon device numbering.
 *     Valid range: 0-255. Invalid values are logged and ignored.
 *     Example: QUECTEL_HWMON_OVERRIDE=3 quectel_rm520n_temp config
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <uci.h>
#include "include/logging.h"
#include "include/common.h"

/* ============================================================================
 * CONSTANTS & CONFIGURATION
 * ============================================================================ */

#define SYSFS_BASE "/sys/kernel/quectel_rm520n_thermal"
#define HWMON_BASE "/sys/class/hwmon"
#define HWMON_NUM_MAX 255   /* Maximum valid hwmon device number */
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
    char *endptr;
    long num;

    if (!dir_name) return -1;

    // Extract numeric part from "hwmon3" -> "3"
    const char *num_start = strpbrk(dir_name, "0123456789");
    if (num_start) {
        errno = 0;
        num = strtol(num_start, &endptr, 10);
        // Validate: no error, consumed digits, and within valid range
        if (errno == 0 && endptr != num_start && num >= 0 && num <= HWMON_NUM_MAX) {
            return (int)num;
        }
    }
    return -1;
}

/* ============================================================================
 * UCI CONFIGURATION FUNCTIONS
 * ============================================================================ */

/**
 * Read a UCI configuration value using UCI C library
 *
 * Uses the UCI C library directly instead of popen() to avoid
 * command injection vulnerabilities.
 *
 * @param option Option name to read
 * @param buffer Buffer to store the value
 * @param buffer_size Size of the buffer
 * @return 0 on success, -1 on error
 */
static int read_uci_option(const char *option, char *buffer, size_t buffer_size)
{
    struct uci_context *ctx;
    struct uci_package *pkg;
    struct uci_section *section;
    const char *value;

    if (!option || !buffer || buffer_size == 0) {
        return -1;
    }

    ctx = uci_alloc_context();
    if (!ctx) {
        logging_error("Failed to allocate UCI context");
        return -1;
    }

    if (uci_load(ctx, UCI_CONFIG, &pkg) != UCI_OK) {
        logging_debug("Failed to load UCI package '%s'", UCI_CONFIG);
        uci_free_context(ctx);
        return -1;
    }

    section = uci_lookup_section(ctx, pkg, UCI_SECTION);
    if (!section) {
        logging_debug("UCI section '%s' not found", UCI_SECTION);
        uci_free_context(ctx);
        return -1;
    }

    value = uci_lookup_option_string(ctx, section, option);
    if (!value) {
        logging_debug("UCI option '%s' not found", option);
        uci_free_context(ctx);
        return -1;
    }

    /* Copy value to buffer safely */
    strncpy(buffer, value, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';

    uci_free_context(ctx);
    return 0;
}

/**
 * Convert Celsius to millidegrees Celsius
 *
 * @param celsius_str Temperature string in Celsius
 * @return Temperature in millidegrees Celsius, or DEFAULT_TEMP_DEFAULT on error
 */
static int celsius_to_millidegrees(const char *celsius_str)
{
    if (!celsius_str || *celsius_str == '\0') {
        logging_warning("celsius_to_millidegrees: empty input, using default");
        return DEFAULT_TEMP_DEFAULT;
    }

    char *endptr;
    errno = 0;
    double celsius = strtod(celsius_str, &endptr);

    /* Check for conversion errors */
    if (errno != 0 || endptr == celsius_str || (*endptr != '\0' && !isspace(*endptr))) {
        logging_warning("celsius_to_millidegrees: invalid input '%s', using default", celsius_str);
        return DEFAULT_TEMP_DEFAULT;
    }

    /* Validate range to prevent overflow (use float limits for *1000) */
    if (celsius < (TEMP_ABSOLUTE_MIN / 1000) || celsius > (TEMP_ABSOLUTE_MAX / 1000)) {
        logging_warning("celsius_to_millidegrees: value %.1f out of range, using default", celsius);
        return DEFAULT_TEMP_DEFAULT;
    }

    return (int)(celsius * 1000.0);
}

/* ============================================================================
 * SYSFS INTERFACE FUNCTIONS
 * ============================================================================ */

/**
 * Find the Quectel hwmon device (single-pass optimized)
 *
 * Scans hwmon devices once, preferring exact name match over partial match.
 *
 * @return hwmon device number on success, -1 on error
 */
static int find_quectel_hwmon_device(void)
{
    DIR *hwmon_dir;
    struct dirent *entry;
    int hwmon_num = -1;
    int fallback_num = -1;  /* Partial match fallback */

    hwmon_dir = opendir(HWMON_BASE);
    if (!hwmon_dir) {
        return -1;
    }

    while ((entry = readdir(hwmon_dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* Build paths for this device */
        char name_path[256];
        char verify_path[256];
        if (snprintf(name_path, sizeof(name_path), "%s/%s/name", HWMON_BASE, entry->d_name) >= (int)sizeof(name_path))
            continue;
        if (snprintf(verify_path, sizeof(verify_path), "%s/%s/temp1_input", HWMON_BASE, entry->d_name) >= (int)sizeof(verify_path))
            continue;

        /* Read device name */
        FILE *name_fp = fopen(name_path, "r");
        if (!name_fp)
            continue;

        char dev_name[64];
        if (fgets(dev_name, sizeof(dev_name), name_fp) == NULL) {
            fclose(name_fp);
            continue;
        }
        fclose(name_fp);
        dev_name[strcspn(dev_name, "\n")] = '\0';

        logging_debug("Found hwmon device: %s -> %s", entry->d_name, dev_name);

        /* Check for exact match first (highest priority) */
        if (strcmp(dev_name, "quectel_rm520n_thermal") == 0) {
            /* Verify device has temp1_input */
            if (access(verify_path, R_OK) == 0) {
                int num = extract_hwmon_number(entry->d_name);
                if (num >= 0) {
                    logging_info("Selected Quectel hwmon device (exact match): hwmon%d", num);
                    hwmon_num = num;
                    break;  /* Exact match found, stop searching */
                }
            }
        }
        /* Check for partial match (fallback) */
        else if (fallback_num < 0 &&
                 (strstr(dev_name, "quectel") != NULL || strstr(dev_name, "rm520n") != NULL)) {
            if (access(verify_path, R_OK) == 0) {
                int num = extract_hwmon_number(entry->d_name);
                if (num >= 0) {
                    logging_debug("Found Quectel-like device (partial match): hwmon%d", num);
                    fallback_num = num;
                    /* Continue searching for exact match */
                }
            }
        }
    }

    closedir(hwmon_dir);

    /* Use fallback if no exact match found */
    if (hwmon_num < 0 && fallback_num >= 0) {
        logging_info("Using Quectel-like device (partial match): hwmon%d", fallback_num);
        hwmon_num = fallback_num;
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
    
    // Open file for writing (avoid TOCTOU race with access() check)
    fp = fopen(path, "w");
    if (!fp) {
        logging_debug("Sysfs file not writable: %s", path);
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
    
    // Open file for reading (avoid TOCTOU race with access() check)
    fp = fopen(path, "r");
    if (!fp) {
        logging_debug("Sysfs file not readable: %s", path);
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
            char *endptr;
            errno = 0;
            long manual_num = strtol(manual_hwmon, &endptr, 10);
            if (errno != 0 || endptr == manual_hwmon || *endptr != '\0') {
                logging_warning("Invalid QUECTEL_HWMON_OVERRIDE value '%s', ignoring", manual_hwmon);
            } else if (manual_num < 0 || manual_num > HWMON_NUM_MAX) {
                logging_warning("QUECTEL_HWMON_OVERRIDE value %ld out of range [0-%d], ignoring",
                               manual_num, HWMON_NUM_MAX);
            } else {
                logging_info("Manual hwmon override: using hwmon%ld (from QUECTEL_HWMON_OVERRIDE=%s)",
                            manual_num, manual_hwmon);
                hwmon_num = (int)manual_num;
            }
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
        
        // Update temp1_min (direct fopen to avoid TOCTOU race)
        if (read_uci_option(UCI_TEMP_MIN, uci_value, sizeof(uci_value)) == 0) {
            temp_min = celsius_to_millidegrees(uci_value);
            if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_min", HWMON_BASE, hwmon_num) < (int)sizeof(hwmon_path)) {
                logging_info("Attempting to update hwmon temp1_min at: %s", hwmon_path);
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
                    logging_warning("Hwmon temp1_min file not writable: %s (errno: %d)", hwmon_path, errno);
                }
            }
        }

        // Update temp1_max (direct fopen to avoid TOCTOU race)
        if (read_uci_option(UCI_TEMP_MAX, uci_value, sizeof(uci_value)) == 0) {
            temp_max = celsius_to_millidegrees(uci_value);
            if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_max", HWMON_BASE, hwmon_num) < (int)sizeof(hwmon_path)) {
                logging_info("Attempting to update hwmon temp1_max at: %s", hwmon_path);
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
                    logging_warning("Hwmon temp1_max file not writable: %s (errno: %d)", hwmon_path, errno);
                }
            }
        }

        // Update temp1_crit (direct fopen to avoid TOCTOU race)
        if (read_uci_option(UCI_TEMP_CRIT, uci_value, sizeof(uci_value)) == 0) {
            temp_crit = celsius_to_millidegrees(uci_value);
            if (snprintf(hwmon_path, sizeof(hwmon_path), "%s/hwmon%d/temp1_crit", HWMON_BASE, hwmon_num) < (int)sizeof(hwmon_path)) {
                logging_info("Attempting to update hwmon temp1_crit at: %s", hwmon_path);
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
            logging_info("Successfully updated %d threshold(s) via main sysfs interface", fallback_updated);
        } else {
            logging_warning("No thresholds could be updated via main sysfs interface");
        }

        // Final summary
        logging_info("=== UCI Configuration Update Summary ===");
        if (hwmon_updated > 0) {
            logging_info("Hwmon interface: %d threshold(s) updated directly", hwmon_updated);
        } else {
            logging_info("Hwmon interface: No thresholds updated (files may be read-only)");
        }

        if (fallback_updated > 0) {
            logging_info("Main sysfs interface: %d threshold(s) updated", fallback_updated);
        } else {
            logging_info("Main sysfs interface: No thresholds updated");
        }

        logging_info("Note: Hwmon files are typically read-only; main sysfs updates are authoritative");
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
                            if (strcmp(alt_dev_name, "quectel_rm520n_thermal") == 0) {
                                int alt_hwmon_num = extract_hwmon_number(alt_entry->d_name);
                                if (alt_hwmon_num < 0) {
                                    logging_warning("Failed to extract hwmon number from '%s'", alt_entry->d_name);
                                    fclose(alt_name_fp);
                                    continue;
                                }
                                logging_info("Alternative detection found: hwmon%d", alt_hwmon_num);
                                
                                // Try to update this alternative device
                                char alt_hwmon_path[256];
                                if (snprintf(alt_hwmon_path, sizeof(alt_hwmon_path), "%s/hwmon%d/temp1_crit", HWMON_BASE, alt_hwmon_num) < sizeof(alt_hwmon_path)) {
                                    // Try to update temp1_crit (avoid TOCTOU race with access() check)
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
