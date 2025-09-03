/**
 * @file ui.h
 * @brief User interface function declarations
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * Header file for user interface functions including help text, version
 * information, and environment variable handling.
 */

#ifndef UI_H
#define UI_H

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

/**
 * Print version information
 */
void print_version(void);

/**
 * Print usage information
 * 
 * @param progname Program name for usage examples
 */
void print_usage(const char *progname);

/**
 * Check environment variables for CLI guidelines compliance
 * 
 * This function checks for standard environment variables that control CLI behavior:
 * - DEBUG: Enables debug output (same as --debug flag)
 * 
 * Following clig.dev guidelines for environment variable usage.
 */
void check_environment_variables(void);

#endif /* UI_H */
