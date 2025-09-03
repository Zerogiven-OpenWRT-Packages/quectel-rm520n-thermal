/**
 * @file temperature.c
 * @brief Temperature processing and parsing functions for Quectel RM520N thermal management
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This file contains temperature-related functions including AT response parsing,
 * temperature extraction, validation, and processing. These functions are
 * self-contained and provide robust temperature handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "logging.h"

/* ============================================================================
 * TEMPERATURE PROCESSING FUNCTIONS
 * ============================================================================ */

/**
 * extract_single_temperature - Helper function to extract temperature from pattern
 * @param response: The full AT response string
 * @param pattern: The pattern to search for
 * @param pattern_len: Length of the pattern string
 * @param temp_value: Pointer to store the extracted temperature
 * 
 * Extracts a single temperature value from the response using the specified pattern.
 * Handles the actual response format: +QTEMP:"pattern","value"
 * 
 * @return 1 on success, 0 on failure
 */
static int extract_single_temperature(const char *response, const char *pattern, 
                                    size_t pattern_len, int *temp_value)
{
    const char *temp_ptr;
    
    logging_debug("extract_single_temperature: pattern='%s', response_len=%zu", pattern, strlen(response));
    
    if (!response || !pattern || !temp_value) {
        logging_debug("extract_single_temperature: invalid parameters");
        return 0;
    }
    
    // Search for the pattern in the response
    temp_ptr = strstr(response, pattern);
    if (!temp_ptr) {
        logging_debug("extract_single_temperature: pattern '%s' not found", pattern);
        return 0;
    }
    
    // Look backwards to find the start of the +QTEMP line
    const char *line_start = temp_ptr;
    while (line_start > response && *line_start != '\n' && *line_start != '\r') {
        line_start--;
    }
    if (*line_start == '\n' || *line_start == '\r') {
        line_start++;
    }
    
    // Check if this line starts with +QTEMP:
    if (strncmp(line_start, "+QTEMP:", 7) != 0) {
        logging_debug("extract_single_temperature: line does not start with +QTEMP:");
        return 0;
    }
    
    // Find the temperature value after the pattern
    temp_ptr += pattern_len;
    
    // Skip any whitespace, commas, and tabs
    while (*temp_ptr && (isspace(*temp_ptr) || *temp_ptr == ',')) {
        temp_ptr++;
    }
    
    // Skip the opening quote if present
    if (*temp_ptr == '"') {
        temp_ptr++;
    }
    
    // Check if we have a valid temperature value
    if (*temp_ptr && *temp_ptr != '"' && isdigit(*temp_ptr)) {
        *temp_value = atoi(temp_ptr);
        logging_debug("extract_single_temperature: extracted %d°C for pattern '%s'", *temp_value, pattern);
        return 1;
    }
    
    logging_debug("extract_single_temperature: no valid temperature found for pattern '%s'", pattern);
    return 0;
}

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
                       const char *modem_prefix, const char *ap_prefix, const char *pa_prefix)
{
    logging_debug("extract_temp_values: parsing AT+QTEMP response (len=%zu)", response ? strlen(response) : 0);
    
    /* Initialize output values */
    if (modem_temp) *modem_temp = 0;
    if (ap_temp) *ap_temp = 0;
    if (pa_temp) *pa_temp = 0;
    
    /* Validate response format */
    if (!response) {
        logging_warning("AT+QTEMP response invalid: null pointer");
        return 0;
    }
    
    if (!strstr(response, "+QTEMP:")) {
        logging_warning("AT+QTEMP response invalid: missing +QTEMP prefix");
        return 0;
    }
    
    /* Check for alternative response formats */
    if (strstr(response, "ERROR")) {
        logging_warning("Modem response: ERROR returned");
        return 0;
    }
    
    if (strstr(response, "OK") && !strstr(response, "modem")) {
        logging_warning("Modem response: OK but no temperature data");
        return 0;
    }
    
    /* Extract temperatures using configurable prefixes */
    if (modem_temp && modem_prefix) {
        char pattern[128];
        snprintf(pattern, sizeof(pattern), "\"%s\"", modem_prefix);
        if (extract_single_temperature(response, pattern, strlen(pattern), modem_temp)) {
            logging_debug("Extracted modem temperature: %d°C", *modem_temp);
        }
    }
    
    if (ap_temp && ap_prefix) {
        char pattern[128];
        snprintf(pattern, sizeof(pattern), "\"%s\"", ap_prefix);
        if (extract_single_temperature(response, pattern, strlen(pattern), ap_temp)) {
            logging_debug("Extracted AP temperature: %d°C", *ap_temp);
        }
    }
    
    if (pa_temp && pa_prefix) {
        char pattern[128];
        snprintf(pattern, sizeof(pattern), "\"%s\"", pa_prefix);
        if (extract_single_temperature(response, pattern, strlen(pattern), pa_temp)) {
            logging_debug("Extracted PA temperature: %d°C", *pa_temp);
        }
    }
    
    /* Check if all temperatures are zero (potential parsing issue) */
    if (modem_temp && *modem_temp == 0 && ap_temp && *ap_temp == 0 && pa_temp && *pa_temp == 0) {
        logging_warning("Temperature parsing issue: all values are 0°C");
    }
    
    /* Validate extracted temperatures (basic sanity check) */
    if (modem_temp && (*modem_temp < -40 || *modem_temp > 120)) {
        logging_warning("Modem temperature out of range: %d°C", *modem_temp);
        return 0;
    }
    
    if (ap_temp && (*ap_temp < -40 || *ap_temp > 120)) {
        logging_warning("AP temperature out of range: %d°C", *ap_temp);
        return 0;
    }
    
    if (pa_temp && (*pa_temp < -40 || *pa_temp > 120)) {
        logging_warning("PA temperature out of range: %d°C", *pa_temp);
        return 0;
    }
    
    logging_debug("extract_temp_values: success - modem:%d°C, AP:%d°C, PA:%d°C", 
                 modem_temp ? *modem_temp : -999, ap_temp ? *ap_temp : -999, pa_temp ? *pa_temp : -999);
    
    return 1; /* Success */
}
