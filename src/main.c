/**
 * @file main.c
 * @brief Combined daemon and CLI tool for Quectel RM520N thermal management
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This module provides both daemon and CLI functionality in a single binary.
 * Use 'daemon' subcommand to start the daemon, otherwise runs in CLI mode.
 * 
 * CODE ORGANIZATION:
 * - User Interface Functions: Help, version, usage display
 * - Environment & Configuration: Environment variable handling
 * - Daemon Management: Process control, locking, status checking
 * - Temperature Processing: AT command parsing and temperature extraction
 * - Application Modes: CLI and daemon mode implementations
 * - Signal & Control: Signal handling and graceful shutdown
 * 
 * PERFORMANCE FEATURES:
 * - Optimized string parsing with hardcoded lengths
 * - Efficient buffer management with appropriate sizes
 * - Single-pass temperature extraction
 * - Minimal memory allocation
 * 
 * MEMORY SAFETY FEATURES:
 * - Buffer overflow protection with snprintf return value checks
 * - Appropriate buffer sizes for filesystem paths (256 bytes)
 * - Safe string operations with bounds checking
 * - Graceful handling of path truncation scenarios
 */

/* ============================================================================
 * INCLUDES & HEADERS
 * ============================================================================ */

/* Standard C library includes */
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>

#include <termios.h>
#include <syslog.h>
#include <time.h>

#include <errno.h>
#include <signal.h>

#include <stdbool.h>



#include <sys/file.h>
#include <getopt.h>

/* Project-specific includes */
#include "include/serial.h"
#include "include/config.h"
#include "include/logging.h"
#include "include/common.h"
#include "include/temperature.h"
#include "include/ui.h"
#include "include/system.h"
#include "include/cli.h"
#include "include/daemon.h"

#include "include/uci_config.h"

/* ============================================================================
 * CONSTANTS & CONFIGURATION
 * ============================================================================ */

/* Configuration constants */
#define MAX_RESPONSE 1024
#define AT_COMMAND "AT+QTEMP\r"
#define PID_FILE "/var/run/quectel_rm520n_temp.pid"
#define LOCK_FILE "/var/run/quectel_rm520n_temp.lock"

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

/* Global variables */
config_t config;  /* Shared with CLI and other modules */
static bool json_output = false;
bool verbose_output = false;  /* Shared with UI module */
static bool celsius_output = false;
static bool watch_mode = false;
volatile sig_atomic_t shutdown_requested = 0;

/* ============================================================================
 * FUNCTION PROTOTYPES
 * ============================================================================ */





/* ============================================================================
 * USER INTERFACE FUNCTIONS
 * ============================================================================ */



/* ============================================================================
 * ENVIRONMENT & CONFIGURATION FUNCTIONS
 * ============================================================================ */



/* ============================================================================
 * DAEMON MANAGEMENT FUNCTIONS
 * ============================================================================ */







/* ============================================================================
 * MAIN FUNCTION
 * ============================================================================ */

/**
 * Main function
 */
int main(int argc, char *argv[])
{
    int opt;
    const struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"baud", required_argument, 0, 'b'},
        {"json", no_argument, 0, 'j'},
        {"debug", no_argument, 0, 'd'},
        {"celsius", no_argument, 0, 'c'},
        {"watch", no_argument, 0, 'w'},
        {"version", no_argument, 0, 'V'},

        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "p:b:jhdVcwh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                if (optarg) {
                SAFE_STRNCPY(config.serial_port, optarg, sizeof(config.serial_port));
                } else {
                    fprintf(stderr, "Error: --port requires an argument. Example: --port /dev/ttyUSB2\n");
                    fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
                    return 2;
                }
                break;
            case 'b':
                if (optarg) {
                    if (config_parse_baud_rate(optarg, &config.baud_rate) != 0) {
                        fprintf(stderr, "Error: Invalid baud rate '%s'. Supported values: 9600, 19200, 38400, 57600, 115200\n", optarg);
                        fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
                        return 2;
                    }
                } else {
                    fprintf(stderr, "Error: --baud requires an argument. Example: --baud 115200\n");
                    fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
                    return 2;
                }
                break;
            case 'j':
                json_output = true;
                break;
            case 'd':
                verbose_output = true; // --debug enables verbose output
                break;
            case 'c':
                celsius_output = true; // --celsius returns temperature in degrees instead of millidegrees
                break;
            case 'w':
                watch_mode = true; // --watch continuously monitors temperature
                break;
            case 'V':
                print_version();
                return 0;

            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                fprintf(stderr, "Error: Unknown option '%c'. Use --help to see valid options\n", optopt);
                fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
                return 2;
        }
    }

    // Initialize logging system early (with default threshold)
    // CLI mode: use stderr output, no syslog
    // Start with INFO level, will update after reading config
    logging_init(false, true, false, BINARY_NAME);

    // Read UCI configuration (this may generate debug logs)
    config_read_uci(&config);

    // Update logging threshold based on config log_level
    // --debug flag overrides config to set debug level
    int log_threshold;
    if (verbose_output) {
        log_threshold = LOG_DEBUG;
    } else {
        log_threshold = config_parse_log_level(config.log_level);
    }
    ulog_threshold(log_threshold);

    // Check environment variables for CLI guidelines compliance
    check_environment_variables();
    
    // Set up signal handling for graceful shutdown
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    logging_debug("Configuration loaded: port=%s, baud=%d", 
                  config.serial_port, config.baud_rate);
    
    // Parse subcommand (default to 'read' if none given)
    const char *command;
    if (optind >= argc) {
        command = "read"; // Default to read mode
        logging_debug("No subcommand specified, defaulting to 'read'");
    } else {
        command = argv[optind];
    }
    
    // Validate mode configuration
    if (strcmp(command, "daemon") == 0 && verbose_output) {
        logging_warning("Debug output enabled in daemon mode, consider UCI debug setting instead");
    }
    
    // Validate CLI arguments (CLI guidelines compliance)
    if (strcmp(command, "daemon") == 0 && json_output) {
        fprintf(stderr, "Error: --json is not valid in daemon mode. Use 'read --json' for JSON output\n");
        fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
        return 2;
    }
    
    if (strcmp(command, "daemon") == 0 && celsius_output) {
        fprintf(stderr, "Error: --celsius is not valid in daemon mode. Use 'read --celsius' for celsius output\n");
        fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
        return 2;
    }
    
    if (strcmp(command, "daemon") == 0 && watch_mode) {
        fprintf(stderr, "Error: --watch is not valid in daemon mode. Use 'read --watch' for continuous monitoring\n");
        fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
        return 2;
    }
    
    // Run in appropriate mode
    if (strcmp(command, "daemon") == 0) {
        return daemon_mode(&shutdown_requested);
    } else if (strcmp(command, "read") == 0) {
        if (watch_mode) {
            // Watch mode: continuously monitor temperature
            char temp_str[64];
            int consecutive_failures = 0;
            const int max_failures = 3;

            if (!json_output) {
                printf("Monitoring temperature at %d seconds interval (press Ctrl+C to exit)...\n", config.interval);
            }

            while (!shutdown_requested) {
                int result = cli_mode(temp_str, sizeof(temp_str));

                // Track consecutive serial failures (for instant retry)
                if (result == CLI_ERR_SERIAL) {
                    consecutive_failures++;
                    if (consecutive_failures >= max_failures) {
                        if (!json_output) {
                            fprintf(stderr, "\nError: %d consecutive serial failures. "
                                    "Check serial port configuration.\n", max_failures);
                        } else {
                            printf("{\n");
                            printf("  \"error\": \"serial_failure\",\n");
                            printf("  \"message\": \"%d consecutive serial failures\",\n", max_failures);
                            printf("  \"timestamp\": \"%ld\"\n", (long)time(NULL));
                            printf("}\n");
                        }
                        return 1;
                    }
                    // Instant retry for serial failures - no sleep
                    continue;
                } else if (result != CLI_SUCCESS) {
                    // Other errors - reset counter but wait before retry
                    consecutive_failures = 0;
                } else {
                    consecutive_failures = 0;  // Reset on success
                }

                // Convert temperature format if needed
                if (result == CLI_SUCCESS && celsius_output && strcmp(temp_str, "N/A") != 0) {
                    int temp_mdeg = atoi(temp_str);
                    int temp_celsius = temp_mdeg / 1000;
                    snprintf(temp_str, sizeof(temp_str), "%d", temp_celsius);
                }

                // Output the temperature value
                if (json_output) {
                    printf("{\n");
                    printf("  \"temperature\": \"%s\",\n", temp_str);
                    printf("  \"status\": \"%s\",\n", result == CLI_SUCCESS ? "ok" : "error");
                    printf("  \"timestamp\": \"%ld\"\n", (long)time(NULL));
                    printf("}\n");
                } else {
                    // Clear line and print temperature with timestamp
                    printf("\r\033[K"); // Clear current line
                    time_t now = time(NULL);
                    struct tm *tm_info = localtime(&now);
                    char time_str[32];
                    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
                    printf("[%s] Temperature: %s", time_str, temp_str);
                    fflush(stdout);
                }

                // Sleep for configured interval (following CLI guidelines for watch mode)
                sleep(config.interval);
            }

            if (!json_output) {
                printf("\n"); // New line after Ctrl+C
            }

            return 0;
        } else {
            // Single read mode
            char temp_str[64];
            int result = cli_mode(temp_str, sizeof(temp_str));

            // Convert temperature format if needed
            if (result == CLI_SUCCESS && celsius_output && strcmp(temp_str, "N/A") != 0) {
                int temp_mdeg = atoi(temp_str);
                int temp_celsius = temp_mdeg / 1000;
                snprintf(temp_str, sizeof(temp_str), "%d", temp_celsius);
            }

            // Output the temperature value
            if (json_output) {
                printf("{\n");
                printf("  \"temperature\": \"%s\",\n", temp_str);
                printf("  \"status\": \"%s\",\n", result == CLI_SUCCESS ? "ok" : "error");
                printf("  \"timestamp\": \"%ld\"\n", (long)time(NULL));
                printf("}\n");
            } else {
                printf("%s\n", temp_str);
            }

            return (result == CLI_SUCCESS) ? 0 : 1;
        }
    } else if (strcmp(command, "config") == 0) {
        return uci_config_mode();
    } else if (strcmp(command, "status") == 0) {
        // Status command - check daemon running state and show system info
        int daemon_status = check_daemon_running();

        if (daemon_status == 1) {
            printf("Status: running\n");

            // Try to read PID
            FILE *pid_file = fopen("/var/run/quectel_rm520n_temp.pid", "r");
            if (pid_file) {
                int pid;
                if (fscanf(pid_file, "%d", &pid) == 1) {
                    printf("PID: %d\n", pid);
                }
                fclose(pid_file);
            }

            // Show current temperature if available
            if (access("/sys/kernel/quectel_rm520n_thermal/temp", R_OK) == 0) {
                FILE *temp_fp = fopen("/sys/kernel/quectel_rm520n_thermal/temp", "r");
                if (temp_fp) {
                    char temp[SMALL_BUFFER_LEN];
                    if (fgets(temp, sizeof(temp), temp_fp) != NULL) {
                        STRIP_NEWLINE(temp);
                        int temp_mdeg = atoi(temp);
                        printf("Temperature: %d m°C (%.1f°C)\n", temp_mdeg, temp_mdeg / 1000.0);
                    }
                    fclose(temp_fp);
                }
            }

            // Show statistics from kernel module
            if (access("/sys/kernel/quectel_rm520n_thermal/stats", R_OK) == 0) {
                FILE *stats_fp = fopen("/sys/kernel/quectel_rm520n_thermal/stats", "r");
                if (stats_fp) {
                    char stats_line[256];
                    printf("\nKernel module statistics:\n");
                    while (fgets(stats_line, sizeof(stats_line), stats_fp) != NULL) {
                        STRIP_NEWLINE(stats_line);
                        printf("  %s\n", stats_line);
                    }
                    fclose(stats_fp);
                }
            }

            // Show kernel modules status
            FILE *modules = fopen("/proc/modules", "r");
            if (modules) {
                char line[PATH_MAX_LEN];
                int module_count = 0;
                while (fgets(line, sizeof(line), modules)) {
                    if (strstr(line, "quectel_rm520n_temp")) {
                        module_count++;
                    }
                }
                printf("Kernel modules: %d loaded\n", module_count);
                fclose(modules);
            }

            return 0;
        } else if (daemon_status == 0) {
            printf("Status: stopped\n");
            printf("Daemon is not running\n");
            return 1;
        } else {
            printf("Status: error\n");
            printf("Unable to determine daemon status\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Error: Unknown command '%s'. Valid commands: 'read' (default), 'daemon', 'config', or 'status'\n", command);
        fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
        return 2;
    }
}
