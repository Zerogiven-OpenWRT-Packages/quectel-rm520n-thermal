/**
 * @file common.h
 * @brief Common definitions and fallbacks for Quectel RM520N thermal kernel modules
 * @author Christopher Sollinger
 * @date 2025
 * @license GPL
 * 
 * This header file provides common fallback definitions for package information
 * that should be consistent across all kernel modules. It ensures that if the
 * build system doesn't provide these macros, sensible defaults are used.
 */

#ifndef COMMON_H
#define COMMON_H

/* ============================================================================
 * PACKAGE INFORMATION FALLBACKS
 * ============================================================================ */

/* Package name fallback */
#ifndef PKG_NAME
#define PKG_NAME "quectel-rm520n-thermal"
#endif

/* Binary name fallback */
#ifndef BINARY_NAME
#define BINARY_NAME "quectel_rm520n_temp"
#endif

/* Package version tag fallback */
#ifndef PKG_TAG
#define PKG_TAG "2.0.0-r0"
#endif

/* Package maintainer fallback */
#ifndef PKG_MAINTAINER
#define PKG_MAINTAINER "Christopher Sollinger"
#endif

/* Package license fallback */
#ifndef PKG_LICENSE
#define PKG_LICENSE "GPL"
#endif

/* Package copyright year fallback */
#ifndef PKG_COPYRIGHT_YEAR
#define PKG_COPYRIGHT_YEAR "2025"
#endif

/* ============================================================================
 * KERNEL MODULE COMPATIBILITY
 * ============================================================================ */

/* Ensure these macros are available for all kernel modules */
#define KMOD_NAME PKG_NAME
#define KMOD_VERSION PKG_TAG
#define KMOD_AUTHOR PKG_MAINTAINER
#define KMOD_LICENSE PKG_LICENSE
#define KMOD_COPYRIGHT_YEAR PKG_COPYRIGHT_YEAR

/* ============================================================================
 * TEMPERATURE THRESHOLDS (m°C)
 * ============================================================================ */

/* Default temperature thresholds - can be overridden by UCI config */
#define DEFAULT_TEMP_MIN     -30000   /* -30°C in m°C */
#define DEFAULT_TEMP_MAX     75000    /* 75°C in m°C */
#define DEFAULT_TEMP_CRIT    85000    /* 85°C in m°C */
#define DEFAULT_TEMP_DEFAULT 40000    /* 40°C in m°C */

/* Temperature range validation */
#define TEMP_ABSOLUTE_MIN    -40000   /* -40°C in m°C (hardware limit) */
#define TEMP_ABSOLUTE_MAX    125000   /* 125°C in m°C (hardware limit) */

#endif /* KMOD_COMMON_H */
