/**
 * @file config.c
 * @brief Shared configuration management for Quectel RM520N
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This module provides common configuration management functions used by
 * both the daemon and CLI tools for reading UCI settings.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <uci.h>
#include "include/config.h"
#include "include/logging.h"

/* Helper macro for safe string copying with null termination */
#define SAFE_STRNCPY(dst, src, size) do { \
    strncpy(dst, src, size - 1); \
    dst[size - 1] = '\0'; \
} while(0)

/**
 * Set default configuration values
 * @param config Configuration structure to initialize
 */
void config_set_defaults(config_t *config)
{
    if (!config) return;

    strcpy(config->serial_port, "/dev/ttyUSB2");
    config->interval = 10;
    config->baud_rate = B115200;
    strcpy(config->error_value, "N/A");
    strcpy(config->log_level, "info");
    strcpy(config->temp_modem_prefix, "modem-ambient-usr");
    strcpy(config->temp_ap_prefix, "cpuss-0-usr");
    strcpy(config->temp_pa_prefix, "modem-lte-sub6-pa1");
}

/**
 * Parse log level string to syslog priority value
 * @param level_str Log level string (e.g., "debug", "info", "warning", "error")
 * @return syslog priority value (defaults to LOG_INFO on invalid input)
 */
int config_parse_log_level(const char *level_str)
{
    if (!level_str) {
        return LOG_INFO;
    }

    if (strcmp(level_str, "debug") == 0) {
        return LOG_DEBUG;
    } else if (strcmp(level_str, "info") == 0) {
        return LOG_INFO;
    } else if (strcmp(level_str, "warning") == 0) {
        return LOG_WARNING;
    } else if (strcmp(level_str, "error") == 0) {
        return LOG_ERR;
    }

    // Default to info for invalid values
    logging_debug("config_parse_log_level: invalid level '%s', using 'info'", level_str);
    return LOG_INFO;
}

/**
 * Parse baud rate string to speed_t value
 * @param baud_str Baud rate string (e.g., "115200")
 * @param baud_rate Output speed_t value
 * @return 0 on success, -1 on failure
 */
int config_parse_baud_rate(const char *baud_str, speed_t *baud_rate)
{
    if (!baud_str || !baud_rate) return -1;
    
    int baud = atoi(baud_str);
    logging_debug("config_parse_baud_rate: input='%s', parsed=%d", baud_str, baud);
    
    switch (baud) {
        case 9600:   *baud_rate = B9600; break;
        case 19200:  *baud_rate = B19200; break;
        case 38400:  *baud_rate = B38400; break;
        case 57600:  *baud_rate = B57600; break;
        case 115200: *baud_rate = B115200; break;
        default: 
            logging_debug("config_parse_baud_rate: unsupported baud rate %d", baud);
            return -1;
    }
    
    logging_debug("config_parse_baud_rate: success, baud_rate=%d", (int)*baud_rate);
    return 0;
}

/**
 * Read configuration from UCI
 * @param config Configuration structure to populate
 * @return 0 on success, -1 on failure
 */
int config_read_uci(config_t *config)
{
    if (!config) return -1;
    
    // Set defaults first
    config_set_defaults(config);
    
    struct uci_context *ctx = uci_alloc_context();
    if (!ctx) {
        logging_debug("Failed to allocate UCI context");
        return -1;
    }
    
    // Load the package explicitly
    struct uci_package *pkg;
    if (uci_load(ctx, "quectel_rm520n_thermal", &pkg) != UCI_OK) {
        logging_debug("Failed to load UCI package 'quectel_rm520n_thermal'");
        uci_free_context(ctx);
        return -1;
    }
    
    pkg = uci_lookup_package(ctx, "quectel_rm520n_thermal");
    if (!pkg) {
        logging_debug("UCI package 'quectel_rm520n_thermal' not found after loading");
        uci_free_context(ctx);
        return -1;
    }
    
    struct uci_section *section = uci_lookup_section(ctx, pkg, "settings");
    if (section) {
        // Read serial port
        const char *port = uci_lookup_option_string(ctx, section, "serial_port");
        if (port) {
            SAFE_STRNCPY(config->serial_port, port, sizeof(config->serial_port));
            logging_debug("UCI serial_port read: '%s'", port);
        } else {
            logging_debug("UCI serial_port not found, using default: '%s'", config->serial_port);
        }
        
        // Read interval
        const char *interval_str = uci_lookup_option_string(ctx, section, "interval");
        if (interval_str) {
            int tmp = atoi(interval_str);
            if (tmp > 0) config->interval = tmp;
            logging_debug("UCI interval read: '%s' -> %d", interval_str, config->interval);
        } else {
            logging_debug("UCI interval not found, using default: %d", config->interval);
        }
        
        // Read baud rate
        const char *baud_str = uci_lookup_option_string(ctx, section, "baud_rate");
        if (baud_str) {
            logging_debug("UCI baud_rate read: '%s'", baud_str);
            if (config_parse_baud_rate(baud_str, &config->baud_rate) == 0) {
                logging_debug("UCI baud_rate parsed successfully");
            } else {
                logging_debug("UCI baud_rate parsing failed for '%s', keeping default", baud_str);
            }
        } else {
            logging_debug("UCI baud_rate not found, using default");
        }
        
        // Read error value
        const char *error_str = uci_lookup_option_string(ctx, section, "error_value");
        if (error_str) {
            SAFE_STRNCPY(config->error_value, error_str, sizeof(config->error_value));
        }

        // Read log level
        const char *log_level_str = uci_lookup_option_string(ctx, section, "log_level");
        if (log_level_str) {
            SAFE_STRNCPY(config->log_level, log_level_str, sizeof(config->log_level));
        }

        // Read temperature prefixes
        const char *modem_prefix = uci_lookup_option_string(ctx, section, "temp_modem_prefix");
        if (modem_prefix) {
            SAFE_STRNCPY(config->temp_modem_prefix, modem_prefix, sizeof(config->temp_modem_prefix));
        }
        
        const char *ap_prefix = uci_lookup_option_string(ctx, section, "temp_ap_prefix");
        if (ap_prefix) {
            SAFE_STRNCPY(config->temp_ap_prefix, ap_prefix, sizeof(config->temp_ap_prefix));
        }
        
        const char *pa_prefix = uci_lookup_option_string(ctx, section, "temp_pa_prefix");
        if (pa_prefix) {
            SAFE_STRNCPY(config->temp_pa_prefix, pa_prefix, sizeof(config->temp_pa_prefix));
        }
    } else {
        logging_debug("UCI section 'settings' not found");
    }
    
    uci_free_context(ctx);
    return 0;
}
