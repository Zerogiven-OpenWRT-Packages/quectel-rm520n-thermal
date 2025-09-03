/**
 * @file serial.c
 * @brief Shared serial communication functions for Quectel RM520N
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This module provides common serial communication functions used by
 * both the daemon and CLI tools for AT command communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include "serial.h"

/* Timeout for AT command responses */
#define AT_TIMEOUT_SEC 5

/* Buffer size constants */
#define MIN_BUFFER_SIZE 64
#define MAX_BUFFER_SIZE 4096

/**
 * validate_serial_params - Helper function to validate serial communication parameters
 * @param fd: File descriptor for serial port
 * @param buf: Buffer pointer
 * @param buflen: Buffer length
 * 
 * Validates that all parameters are within acceptable ranges and not null.
 * 
 * @return 1 if valid, 0 if invalid
 */
static int validate_serial_params(int fd, const char *buf, size_t buflen)
{
    if (fd < 0) {
        return 0;
    }
    
    if (!buf) {
        return 0;
    }
    
    if (buflen < MIN_BUFFER_SIZE || buflen > MAX_BUFFER_SIZE) {
        return 0;
    }
    
    return 1;
}

/**
 * Initializes the serial port and configures it
 * @param port Serial port device path
 * @param baud_rate Baud rate for communication
 * @return File descriptor on success, -1 on failure
 */
int init_serial_port(const char *port, speed_t baud_rate)
{
    int fd;
    struct termios tty;
    
    /* Validate input parameters */
    if (!port) {
        errno = EINVAL;
        return -1;
    }
    
    /* Open serial port with proper flags */
    fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }
    
    /* Initialize termios structure */
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    /* Configure baud rate */
    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);
    
    /* Configure character size and parity */
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~PARENB;  /* No parity */
    tty.c_cflag &= ~CSTOPB;  /* 1 stop bit */
    tty.c_cflag |= CLOCAL;   /* Ignore modem control lines */
    tty.c_cflag |= CREAD;    /* Enable receiver */
    
    /* Configure input flags */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); /* No flow control */
    tty.c_iflag &= ~IGNBRK;
    tty.c_iflag &= ~(ICRNL | IGNCR); /* No CR/NL conversion */
    
    /* Configure local flags */
    tty.c_lflag = 0;
    
    /* Configure output flags */
    tty.c_oflag = 0;
    
    /* Configure special characters */
    tty.c_cc[VMIN] = 1;   /* Minimum characters for read */
    tty.c_cc[VTIME] = 1;  /* Timeout in deciseconds */
    
    /* Apply the configuration */
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    /* Set back to blocking mode for normal operation */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
    
    return fd;
}

/**
 * Reads from the serial port until "OK" appears or timeout is reached
 * @param fd File descriptor for serial port
 * @param buf Buffer to store response
 * @param buflen Size of buffer
 * @return Number of bytes read
 */
int read_modem_response(int fd, char *buf, size_t buflen)
{
    time_t start_time;
    int total = 0;
    int timeout_reached = 0;
    
    /* Validate input parameters */
    if (!validate_serial_params(fd, buf, buflen)) {
        errno = EINVAL;
        return -1;
    }
    
    /* Clear buffer and initialize */
    memset(buf, 0, buflen);
    start_time = time(NULL);
    
    /* Read response with timeout */
    while (!timeout_reached && total < (int)buflen - 1) {
        int n = read(fd, buf + total, buflen - total - 1);
        
        if (n > 0) {
            total += n;
            buf[total] = '\0';
            
            /* Check for OK response */
            if (strstr(buf, "\nOK") || strstr(buf, "\rOK")) {
                break;
            }
            
            /* Check for ERROR response */
            if (strstr(buf, "\nERROR") || strstr(buf, "\rERROR")) {
                break;
            }
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Non-blocking read, check timeout */
                if ((time(NULL) - start_time) >= AT_TIMEOUT_SEC) {
                    timeout_reached = 1;
                    break;
                }
                usleep(10000); /* Wait 10ms */
            } else {
                /* Real error occurred */
                return -1;
            }
        } else {
            /* n == 0, check timeout */
            if ((time(NULL) - start_time) >= AT_TIMEOUT_SEC) {
                timeout_reached = 1;
                break;
            }
            usleep(10000); /* Wait 10ms */
        }
    }
    
    if (timeout_reached) {
        errno = ETIMEDOUT;
    }
    
    return total;
}

/**
 * Sends an AT command and reads the response
 * @param fd File descriptor for serial port
 * @param command AT command to send
 * @param response Buffer to store response
 * @param response_len Size of response buffer
 * @return Number of bytes in response on success, -1 on failure
 */
int send_at_command(int fd, const char *command, char *response, size_t response_len)
{
    ssize_t written;
    int result;
    
    /* Validate input parameters */
    if (fd < 0 || !command || !validate_serial_params(fd, response, response_len)) {
        errno = EINVAL;
        return -1;
    }
    
    /* Flush input buffer before sending command */
    if (tcflush(fd, TCIFLUSH) != 0) {
        /* Log warning but continue */
        fprintf(stderr, "Warning: Failed to flush input buffer: %s\n", strerror(errno));
    }
    
    /* Send the AT command */
    written = write(fd, command, strlen(command));
    if (written < 0) {
        return -1;
    }
    
    /* Ensure command is properly terminated */
    if (write(fd, "\r\n", 2) < 0) {
        return -1;
    }
    
    /* Read the response */
    result = read_modem_response(fd, response, response_len);
    return result;
}

/**
 * Closes the serial port safely
 * @param fd File descriptor for serial port
 * @return 0 on success, -1 on failure
 */
int close_serial_port(int fd)
{
    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }
    
    /* Flush any pending data */
    tcflush(fd, TCIOFLUSH);
    
    /* Close the file descriptor */
    if (close(fd) != 0) {
        return -1;
    }
    
    return 0;
}
