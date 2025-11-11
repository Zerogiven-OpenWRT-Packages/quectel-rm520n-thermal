/**
 * @file ui.c
 * @brief User interface functions for Quectel RM520N thermal management
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This file contains user interface functions including help text, version
 * information, and environment variable handling. These functions are
 * self-contained and provide the CLI user experience.
 */

#include <stdio.h>
#include <stdlib.h>
#include "include/logging.h"
#include "include/common.h"

/* External variables from main.c */
extern bool verbose_output;

/* ============================================================================
 * USER INTERFACE FUNCTIONS
 * ============================================================================ */

/**
 * Print version information
 */
void print_version(void)
{
    printf("%s version %s\n", BINARY_NAME, PKG_TAG);
    printf("Copyright (C) %s %s\n", PKG_COPYRIGHT_YEAR, PKG_MAINTAINER);
    printf("License %s\n", PKG_LICENSE);
}

/**
 * Print usage information
 */
void print_usage(const char *progname)
{
    printf("Usage: %s [OPTIONS] <COMMAND>\n\n", progname);
    printf("Quectel RM520N thermal management tool\n\n");
    printf("DESCRIPTION\n");
    printf("  This tool provides both daemon and CLI functionality for monitoring\n");
    printf("  Quectel RM520N modem temperatures.\n\n");
    	printf("COMMANDS\n");
	printf("  read               Read current temperature (CLI mode) [default]\n");
	printf("  daemon             Start daemon mode (background monitoring)\n");
	printf("  config             Update kernel module thresholds from UCI config\n");
	printf("  status             Show daemon status and system information\n\n");
    printf("Options:\n");
    printf("  -p, --port PORT    Serial port (default: /dev/ttyUSB2)\n");
    printf("  -b, --baud RATE    Baud rate (default: 115200)\n");
    printf("  -j, --json         JSON output format (CLI mode only)\n");
    printf("  -c, --celsius      Return temperature in degrees Celsius (CLI mode only)\n");
    printf("  -w, --watch        Continuously monitor temperature (CLI mode only, respects UCI interval)\n");
    printf("  -d, --debug        Enable debug output\n");
    printf("  -V, --version      Show version information\n");
    printf("  -h, --help         Show this help message\n\n");
    	printf("Examples:\n");
	printf("  %s                    # Read temperature (default CLI mode)\n", progname);
	printf("  %s read               # Read temperature (explicit CLI mode)\n", progname);
	printf("  %s daemon             # Start daemon mode\n", progname);
	printf("  %s config             # Update kernel module thresholds\n", progname);
	printf("  %s status             # Check daemon status\n", progname);
    printf("  %s --json             # Read temperature in JSON format\n", progname);
    printf("  %s --celsius          # Return temperature in degrees Celsius\n", progname);
    printf("  %s --watch            # Continuously monitor temperature\n", progname);
    printf("  %s --watch --celsius  # Monitor temperature in degrees Celsius\n", progname);
    printf("  %s --watch --json     # Monitor temperature in JSON format\n", progname);
    printf("  %s --port /dev/ttyUSB3 # Read from specific port\n", progname);
    printf("  %s --debug            # Enable debug output\n", progname);
    printf("\n");
    printf("Exit codes:\n");
    printf("  0  Success\n");
    printf("  1  Error (serial communication, parsing, etc.)\n");
    printf("  2  Invalid arguments or usage error\n");
    printf("  3  Daemon already running or lock error\n");
    printf("\n");
    printf("CONFIGURATION\n");
    printf("  UCI Config: /etc/config/quectel_rm520n_thermal\n\n");
    printf("ENVIRONMENT VARIABLES\n");
    printf("  DEBUG           Enable debug output (same as --debug flag)\n\n");
    printf("LOGS\n");
    printf("  Daemon: /var/log/messages (filter: quectel_rm520n_temp)\n");
    printf("  CLI: stderr (use --debug for more details)\n");
}

/**
 * Check environment variables for CLI guidelines compliance
 * 
 * This function checks for standard environment variables that control CLI behavior:
 * - DEBUG: Enables debug output (same as --debug flag)
 * 
 * Following clig.dev guidelines for environment variable usage.
 */
void check_environment_variables(void)
{
    // Check for DEBUG environment variable (CLI guidelines compliance)
    if (getenv("DEBUG") != NULL) {
        verbose_output = true;
        logging_debug("DEBUG environment variable detected, enabling verbose output");
    }
}
