/**
 * @file temperature.h
 * @brief Temperature processing function declarations
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * Header file for temperature processing functions including AT response parsing,
 * temperature extraction, validation, and processing.
 */

#ifndef TEMPERATURE_H
#define TEMPERATURE_H

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

/**
 * Extracts temperature values from the AT+QTEMP response
 * 
 * Parses the modem AT+QTEMP response to extract three temperature values:
 * - Modem temperature from configurable prefix (default: "modem-ambient-usr")
 * - AP temperature from configurable prefix (default: "cpuss-0-usr") 
 * - PA temperature from configurable prefix (default: "modem-lte-sub6-pa1")
 * 
 * PERFORMANCE OPTIMIZATIONS:
 * - Uses hardcoded string lengths instead of strlen() calls
 * - Single pass parsing with minimal memory allocation
 * - Efficient string pattern matching with strstr()
 * 
 * Includes validation and debug logging for robust temperature extraction.
 * Following clig.dev guidelines for robust parsing and error handling.
 * 
 * @param response AT command response string to parse
 * @param modem_temp Pointer to store extracted modem temperature (°C)
 * @param ap_temp Pointer to store extracted AP temperature (°C)
 * @param pa_temp Pointer to store extracted PA temperature (°C)
 * @param modem_prefix Modem temperature prefix (e.g., "modem-ambient-usr")
 * @param ap_prefix AP temperature prefix (e.g., "cpuss-0-usr")
 * @param pa_prefix PA temperature prefix (e.g., "modem-lte-sub6-pa1")
 * @return 1 on success, 0 on failure (invalid response format)
 */
int extract_temp_values(const char *response, int *modem_temp, int *ap_temp, int *pa_temp,
                       const char *modem_prefix, const char *ap_prefix, const char *pa_prefix);

#endif /* TEMPERATURE_H */
