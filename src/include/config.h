/**
 * @file config.h
 * @brief Shared configuration management for Quectel RM520N
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 *
 * This header provides common configuration management functions used by
 * both the daemon and CLI tools for reading UCI settings.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <termios.h>
#include "common.h"

/* Configuration structure */
typedef struct {
    char serial_port[CONFIG_STRING_LEN];
    int interval;
    speed_t baud_rate;
    char error_value[CONFIG_STRING_LEN];
    char log_level[CONFIG_STRING_LEN];
    char temp_modem_prefix[CONFIG_STRING_LEN];
    char temp_ap_prefix[CONFIG_STRING_LEN];
    char temp_pa_prefix[CONFIG_STRING_LEN];
} config_t;

/* Function declarations */
int config_read_uci(config_t *config);
void config_set_defaults(config_t *config);
int config_parse_baud_rate(const char *baud_str, speed_t *baud_rate);
int config_parse_log_level(const char *level_str);

#endif /* CONFIG_H */
