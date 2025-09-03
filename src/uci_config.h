/**
 * @file uci_config.h
 * @brief UCI configuration function declarations
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * Header file for UCI configuration functions including reading UCI config
 * and updating kernel module temperature thresholds.
 */

#ifndef UCI_CONFIG_H
#define UCI_CONFIG_H

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

/**
 * UCI configuration mode - update kernel module thresholds from UCI config
 * 
 * Reads UCI configuration and updates kernel module temperature thresholds
 * via sysfs interfaces. Provides a bridge between OpenWRT UCI configuration
 * and kernel module parameters.
 * 
 * @return 0 on success, 1 on error
 */
int uci_config_mode(void);

#endif /* UCI_CONFIG_H */
