/**
 * @file logging.h
 * @brief Shared logging functions for Quectel RM520N
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This header provides consistent logging functions used by
 * both the daemon and CLI tools for standardized output.
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <syslog.h>
#include <stdbool.h>

/* Log levels */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} log_level_t;

/* Logging configuration */
typedef struct {
    log_level_t level;
    bool use_syslog;
    bool use_stderr;
    const char *ident;
} logging_config_t;

/* Function declarations */
void logging_init(logging_config_t *config);
void logging_cleanup(void);
void logging_log(log_level_t level, const char *format, ...);

/* Convenience macros */
#define logging_debug(...)   logging_log(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define logging_info(...)    logging_log(LOG_LEVEL_INFO, __VA_ARGS__)
#define logging_warning(...) logging_log(LOG_LEVEL_WARNING, __VA_ARGS__)
#define logging_error(...)   logging_log(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif /* LOGGING_H */
