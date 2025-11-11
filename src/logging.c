/**
 * @file logging.c
 * @brief Shared logging functions for Quectel RM520N
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This module provides consistent logging functions used by
 * both the daemon and CLI tools for standardized output.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>
#include "include/logging.h"

/* Global logging configuration */
static logging_config_t g_log_config = {
    .level = LOG_LEVEL_INFO,
    .use_syslog = false,
    .use_stderr = true,
    .ident = "quectel_rm520n_thermal"
};

/* Convert our log levels to syslog priorities */
static int level_to_syslog_priority(log_level_t level)
{
    switch (level) {
        case LOG_LEVEL_DEBUG:   return LOG_DEBUG;
        case LOG_LEVEL_INFO:    return LOG_INFO;
        case LOG_LEVEL_WARNING: return LOG_WARNING;
        case LOG_LEVEL_ERROR:   return LOG_ERR;
        default:                return LOG_INFO;
    }
}

/* Get log level string for stderr output */
static const char* level_to_string(log_level_t level)
{
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "DEBUG";
        case LOG_LEVEL_INFO:    return "INFO";
        case LOG_LEVEL_WARNING: return "WARNING";
        case LOG_LEVEL_ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

/**
 * Initialize logging system
 * @param config Logging configuration
 */
void logging_init(logging_config_t *config)
{
    if (config) {
        g_log_config = *config;
    }
    
    if (g_log_config.use_syslog) {
        openlog(g_log_config.ident, LOG_PID | LOG_CONS, LOG_DAEMON);
    }
}

/**
 * Clean up logging system
 */
void logging_cleanup(void)
{
    if (g_log_config.use_syslog) {
        closelog();
    }
}

/**
 * Main logging function
 * @param level Log level
 * @param format Format string
 * @param ... Variable arguments
 */
void logging_log(log_level_t level, const char *format, ...)
{
    /* Check if we should log at this level */
    if (level < g_log_config.level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    /* Log to syslog if enabled */
    if (g_log_config.use_syslog) {
        vsyslog(level_to_syslog_priority(level), format, args);
    }
    
    /* Log to stderr if enabled */
    if (g_log_config.use_stderr) {
        fprintf(stderr, "[%s] %s: ", g_log_config.ident, level_to_string(level));
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
    }
    
    va_end(args);
}
