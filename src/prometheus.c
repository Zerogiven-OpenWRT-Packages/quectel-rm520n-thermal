/**
 * @file prometheus.c
 * @brief Prometheus metrics exporter implementation
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 *
 * Implements a lightweight HTTP server for Prometheus metrics export.
 * Uses non-blocking sockets to integrate with the main daemon loop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include "include/prometheus.h"
#include "include/logging.h"

#define METRICS_BUFFER_SIZE 4096
#define REQUEST_BUFFER_SIZE 1024

/**
 * prometheus_init - Initialize Prometheus HTTP server
 * @config: Prometheus configuration
 *
 * Creates a non-blocking TCP socket and binds it to the configured port.
 *
 * Return: 0 on success, -1 on failure
 */
int prometheus_init(prometheus_config_t *config)
{
    struct sockaddr_in addr;
    int opt = 1;

    if (!config || !config->enabled) {
        return 0;  /* Not enabled, skip initialization */
    }

    /* Create socket */
    config->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (config->server_fd < 0) {
        logging_error("Prometheus: Failed to create socket: %s", strerror(errno));
        return -1;
    }

    /* Set socket options */
    if (setsockopt(config->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logging_warning("Prometheus: Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    /* Set non-blocking mode */
    int flags = fcntl(config->server_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(config->server_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        logging_error("Prometheus: Failed to set non-blocking mode: %s", strerror(errno));
        close(config->server_fd);
        return -1;
    }

    /* Bind to port */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config->port);

    if (bind(config->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        logging_error("Prometheus: Failed to bind to port %d: %s", config->port, strerror(errno));
        close(config->server_fd);
        return -1;
    }

    /* Listen for connections */
    if (listen(config->server_fd, 5) < 0) {
        logging_error("Prometheus: Failed to listen: %s", strerror(errno));
        close(config->server_fd);
        return -1;
    }

    logging_info("Prometheus metrics server listening on port %d", config->port);
    return 0;
}

/**
 * prometheus_shutdown - Shutdown Prometheus HTTP server
 * @config: Prometheus configuration
 *
 * Closes the server socket and cleans up resources.
 */
void prometheus_shutdown(prometheus_config_t *config)
{
    if (config && config->server_fd >= 0) {
        close(config->server_fd);
        config->server_fd = -1;
        logging_info("Prometheus metrics server shutdown");
    }
}

/**
 * prometheus_format_metrics - Format metrics in Prometheus text format
 * @metrics: Metrics data to export
 * @buffer: Output buffer for formatted metrics
 * @buffer_size: Size of output buffer
 *
 * Formats all metrics according to Prometheus text exposition format.
 */
void prometheus_format_metrics(prometheus_metrics_t *metrics, char *buffer, size_t buffer_size)
{
    int offset = 0;

    /* Temperature metrics */
    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_modem_temperature_celsius Current modem temperature in Celsius\n"
        "# TYPE quectel_modem_temperature_celsius gauge\n"
        "quectel_modem_temperature_celsius %d\n\n",
        metrics->temperature_celsius);

    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_modem_temp_min_celsius Minimum temperature threshold in Celsius\n"
        "# TYPE quectel_modem_temp_min_celsius gauge\n"
        "quectel_modem_temp_min_celsius %d\n\n",
        metrics->temp_min_celsius);

    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_modem_temp_max_celsius Maximum temperature threshold in Celsius\n"
        "# TYPE quectel_modem_temp_max_celsius gauge\n"
        "quectel_modem_temp_max_celsius %d\n\n",
        metrics->temp_max_celsius);

    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_modem_temp_crit_celsius Critical temperature threshold in Celsius\n"
        "# TYPE quectel_modem_temp_crit_celsius gauge\n"
        "quectel_modem_temp_crit_celsius %d\n\n",
        metrics->temp_crit_celsius);

    /* Daemon statistics */
    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_daemon_iterations_total Total number of monitoring iterations\n"
        "# TYPE quectel_daemon_iterations_total counter\n"
        "quectel_daemon_iterations_total %lu\n\n",
        metrics->iterations_total);

    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_daemon_reads_success_total Successful temperature reads\n"
        "# TYPE quectel_daemon_reads_success_total counter\n"
        "quectel_daemon_reads_success_total %lu\n\n",
        metrics->reads_success);

    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_daemon_errors_serial_total Serial port errors\n"
        "# TYPE quectel_daemon_errors_serial_total counter\n"
        "quectel_daemon_errors_serial_total %lu\n\n",
        metrics->errors_serial);

    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_daemon_errors_at_command_total AT command errors\n"
        "# TYPE quectel_daemon_errors_at_command_total counter\n"
        "quectel_daemon_errors_at_command_total %lu\n\n",
        metrics->errors_at_cmd);

    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_daemon_errors_parse_total Temperature parsing errors\n"
        "# TYPE quectel_daemon_errors_parse_total counter\n"
        "quectel_daemon_errors_parse_total %lu\n\n",
        metrics->errors_parse);

    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_daemon_uptime_seconds Daemon uptime in seconds\n"
        "# TYPE quectel_daemon_uptime_seconds counter\n"
        "quectel_daemon_uptime_seconds %lu\n\n",
        metrics->uptime_seconds);

    /* Alert status */
    offset += snprintf(buffer + offset, buffer_size - offset,
        "# HELP quectel_modem_alert_active Temperature alert active (1=active, 0=normal)\n"
        "# TYPE quectel_modem_alert_active gauge\n"
        "quectel_modem_alert_active %d\n\n",
        metrics->alert_active ? 1 : 0);
}

/**
 * prometheus_handle_request - Handle HTTP requests for metrics
 * @config: Prometheus configuration
 * @metrics: Current metrics data
 *
 * Accepts incoming connections and serves metrics in response to GET /metrics requests.
 * Uses non-blocking I/O to avoid blocking the daemon loop.
 *
 * Return: 0 on success, -1 on error
 */
int prometheus_handle_request(prometheus_config_t *config, prometheus_metrics_t *metrics)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;
    char request[REQUEST_BUFFER_SIZE];
    char response[METRICS_BUFFER_SIZE];
    char metrics_buffer[METRICS_BUFFER_SIZE];
    ssize_t n;

    if (!config || !config->enabled || config->server_fd < 0) {
        return 0;
    }

    /* Accept connection (non-blocking) */
    client_fd = accept(config->server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;  /* No pending connections */
        }
        logging_debug("Prometheus: accept() failed: %s", strerror(errno));
        return -1;
    }

    /* Read HTTP request */
    n = recv(client_fd, request, sizeof(request) - 1, 0);
    if (n > 0) {
        request[n] = '\0';

        /* Check for GET /metrics */
        if (strstr(request, "GET /metrics") != NULL) {
            /* Format metrics */
            prometheus_format_metrics(metrics, metrics_buffer, sizeof(metrics_buffer));

            /* Build HTTP response */
            snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain; version=0.0.4\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                metrics_buffer);

            /* Send response */
            send(client_fd, response, strlen(response), 0);
            logging_debug("Prometheus: Served metrics to client");
        } else {
            /* Send 404 for other paths */
            const char *not_found =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "\r\n"
                "404 Not Found. Try GET /metrics\n";
            send(client_fd, not_found, strlen(not_found), 0);
        }
    }

    close(client_fd);
    return 0;
}
