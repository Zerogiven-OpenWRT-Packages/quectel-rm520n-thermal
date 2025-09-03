/**
 * @file cli.h
 * @brief CLI mode function declarations
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * Header file for CLI mode implementation including temperature reading
 * with smart fallback strategy.
 */

#ifndef CLI_H
#define CLI_H

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

/**
 * CLI mode - read temperature with smart fallback strategy
 * 
 * Implements intelligent temperature reading that first attempts to read
 * from the daemon's hwmon interface, then falls back to direct AT commands
 * if the daemon is not available.
 * 
 * SMART READING STRATEGY:
 * 1. Check if daemon is running
 * 2. If running: read from hwmon interface (fast, reliable)
 * 3. If not running: fall back to direct AT commands (slower but always available)
 * 
 * Includes comprehensive error handling, logging, and JSON output support.
 * Following clig.dev guidelines for robust CLI behavior and user feedback.
 * 
 * @param temp_str Output buffer for temperature string
 * @param temp_size Size of temp_str buffer
 * @return 0 on success, 1 on error
 */
int cli_mode(char *temp_str, size_t temp_size);

#endif /* CLI_H */
