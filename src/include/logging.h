/**
 * @file logging.h
 * @brief Logging wrapper for OpenWRT ulog (libubox)
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 *
 * This header provides a lightweight wrapper around OpenWRT's ulog
 * library for consistent logging across daemon and CLI tools.
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <libubox/ulog.h>
#include <syslog.h>
#include <stdbool.h>

/**
 * Initialize logging system
 *
 * @param use_syslog Enable syslog output
 * @param use_stderr Enable stderr output
 * @param debug Enable debug level logging
 * @param ident Program identifier string
 */
static inline void logging_init(bool use_syslog, bool use_stderr, bool debug, const char *ident)
{
    int channels = 0;

    if (use_syslog) {
        channels |= ULOG_SYSLOG;
    }
    if (use_stderr) {
        channels |= ULOG_STDIO;
    }

    ulog_open(channels, LOG_DAEMON, ident);

    if (debug) {
        ulog_threshold(LOG_DEBUG);
    } else {
        ulog_threshold(LOG_INFO);
    }
}

/**
 * Clean up logging system
 */
static inline void logging_cleanup(void)
{
    ulog_close();
}

/* Convenience macros mapping to ulog */
#define logging_debug(...)   ulog(LOG_DEBUG, __VA_ARGS__)
#define logging_info(...)    ULOG_INFO(__VA_ARGS__)
#define logging_warning(...) ULOG_WARN(__VA_ARGS__)
#define logging_error(...)   ULOG_ERR(__VA_ARGS__)

#endif /* LOGGING_H */
