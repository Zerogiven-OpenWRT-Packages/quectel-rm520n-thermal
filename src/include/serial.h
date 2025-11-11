/**
 * @file serial.h
 * @brief Shared serial communication functions for Quectel RM520N
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This header provides common serial communication functions used by
 * both the daemon and CLI tools for AT command communication.
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <sys/types.h>
#include <termios.h>

/* Function declarations */
int init_serial_port(const char *port, speed_t baud_rate);
int read_modem_response(int fd, char *buf, size_t buflen);
int send_at_command(int fd, const char *command, char *response, size_t response_len);
int close_serial_port(int fd);

#endif /* SERIAL_H */
