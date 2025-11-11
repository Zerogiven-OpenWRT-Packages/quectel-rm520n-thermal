/**
 * @file prometheus.h
 * @brief Prometheus metrics exporter for Quectel RM520N thermal daemon
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 *
 * This module provides a lightweight HTTP server that exports temperature
 * and daemon statistics in Prometheus text format for monitoring integration.
 */

#ifndef PROMETHEUS_H
#define PROMETHEUS_H

#include <stdbool.h>

/**
 * Prometheus metrics configuration
 */
typedef struct {
    bool enabled;           /* Enable/disable Prometheus export */
    int port;               /* HTTP port for metrics endpoint (default 9101) */
    int server_fd;          /* Server socket file descriptor */
} prometheus_config_t;

/**
 * Metrics data structure
 * Contains all metrics to be exported in Prometheus format
 */
typedef struct {
    int temperature_celsius;        /* Current temperature in °C */
    int temp_min_celsius;           /* Minimum threshold in °C */
    int temp_max_celsius;           /* Maximum threshold in °C */
    int temp_crit_celsius;          /* Critical threshold in °C */
    unsigned long iterations_total; /* Total daemon iterations */
    unsigned long reads_success;    /* Successful temperature reads */
    unsigned long errors_serial;    /* Serial port errors */
    unsigned long errors_at_cmd;    /* AT command errors */
    unsigned long errors_parse;     /* Parse errors */
    unsigned long uptime_seconds;   /* Daemon uptime */
    bool alert_active;              /* Temperature alert status */
} prometheus_metrics_t;

/* Function declarations */
int prometheus_init(prometheus_config_t *config);
void prometheus_shutdown(prometheus_config_t *config);
int prometheus_handle_request(prometheus_config_t *config, prometheus_metrics_t *metrics);
void prometheus_format_metrics(prometheus_metrics_t *metrics, char *buffer, size_t buffer_size);

#endif /* PROMETHEUS_H */
