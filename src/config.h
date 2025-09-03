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

/* Configuration structure */
typedef struct {
    char serial_port[64];
    int interval;
    speed_t baud_rate;
    char error_value[64];
    int debug;
    char temp_modem_prefix[64];
    char temp_ap_prefix[64];
    char temp_pa_prefix[64];
} config_t;

/* Function declarations */
int config_read_uci(config_t *config);
void config_set_defaults(config_t *config);
int config_parse_baud_rate(const char *baud_str, speed_t *baud_rate);

#endif /* CONFIG_H */
