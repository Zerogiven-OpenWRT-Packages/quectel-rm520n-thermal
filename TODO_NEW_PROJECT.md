# Multi-Modem Support Implementation Plan

> **Status**: Planning Phase - No Implementation Yet
>
> **Decision Pending**: Whether to create new project or major refactor of existing one

---

## Table of Contents

- [Project Naming](#project-naming)
- [Overview](#overview)
- [Architecture Decisions](#architecture-decisions)
- [Implementation Phases](#implementation-phases)
  - [Phase 1: Multi-Instance Support](#phase-1-multi-instance-support)
  - [Phase 2: Parser Abstraction](#phase-2-parser-abstraction)
  - [Phase 3: Second Built-in Parser](#phase-3-second-built-in-parser)
  - [Phase 4: Declarative UCI Parser](#phase-4-declarative-uci-parser)
  - [Phase 5: Documentation & Testing](#phase-5-documentation--testing)
- [File Structure](#file-structure)
- [Effort Estimate](#effort-estimate)
- [Testing Strategy](#testing-strategy)
- [Future Parsers](#future-parsers)

---

## Project Naming

**Current Name**: `quectel-rm520n-thermal`

**Proposed Names** (in order of preference):

1. **`at-modem-thermal`**
   - ✅ Clear: Indicates AT command-based modems
   - ✅ Concise: Short and memorable
   - ✅ Accurate: Scope is AT command modems
   - ❌ Doesn't indicate serial connection explicitly

2. **`serial-modem-thermal`**
   - ✅ Clear: Indicates serial connection method
   - ✅ Generic: Works for all serial modems
   - ❌ Less specific: Could include non-AT modems theoretically

3. **`at-serial-thermal`**
   - ✅ Very concise
   - ✅ Indicates both AT and serial
   - ❌ Missing "modem" makes purpose less clear

4. **`modem-thermal-monitor`**
   - ✅ Clear purpose
   - ✅ Generic
   - ❌ Longer
   - ❌ Doesn't indicate AT/serial

**Recommendation**: `at-modem-thermal` - best balance of clarity and conciseness

**Package names after rename**:
- Main: `at-modem-thermal`
- Kernel: `kmod-at-modem-thermal`
- Prometheus: `prometheus-node-exporter-ucode-at-modem-thermal`
- Binary: `/usr/bin/at-modem-temp` or `/usr/bin/atmodem`

---

## Overview

This plan adds support for multiple modems from different manufacturers to the current Quectel-only implementation. The implementation is split into two major features that complement each other:

1. **Multi-Instance Support**: Run multiple daemon instances for multiple physical modems
2. **Multi-Modem Parser Support**: Support different modem types (Quectel, SIMCom, Telit, etc.)

These features are **independent but synergistic**:
- Multi-instance allows monitoring multiple modems simultaneously
- Multi-parser allows different modem brands in the same system
- Combined: Monitor 3 different modems from 3 different manufacturers

**Implementation Order**: Multi-instance first, then parser support

---

## Architecture Decisions

### 1. Multi-Instance Approach

**Decision**: Multiple daemon instances (one per modem)

**Reasoning**:
- Isolation: One modem failure doesn't affect others
- Simplicity: Each instance is independent
- Flexibility: Different configurations per modem
- OpenWRT-friendly: Fits procd multi-instance model

### 2. Parser Architecture

**Three-tier system**:
1. **Built-in native parsers** (C) - Fast, reliable, for popular modems
2. **Declarative UCI config** - Simple custom modems without code
3. **ucode scripts** (future) - Maximum flexibility if needed

### 3. Temperature Selection Logic

**When multiple temperature sources exist** (e.g., modem, CPU, PA):
- **Always use HIGHEST temperature** (current Quectel behavior)
- Applies to all parsers (built-in, declarative, custom)
- Ensures critical temperature is never missed

### 4. Project Organization

**Option A**: Major refactor of existing project
- Rename everything
- Keep git history
- Migration path for existing users

**Option B**: New project from scratch
- Clean slate
- Copy/refactor code
- Old project stays for legacy users

**Decision pending**: Discuss with maintainer

---

## Implementation Phases

## Phase 1: Multi-Instance Support (4-6 hours)

**Goal**: Support multiple physical modems of the same type (Quectel for now)

**Prerequisites**: None (can implement immediately)

### 1.1 Kernel Module Multi-Instance Support

**Add instance parameter** to kernel modules:

**Changes to `src/kmod/main.c`**:
```c
static int instance = 0;
module_param(instance, int, 0444);
MODULE_PARM_DESC(instance, "Modem instance number (default: 0)");

// In init function:
snprintf(kobj_name, sizeof(kobj_name), "quectel_rm520n_thermal%d", instance);
temp_kobj = kobject_create_and_add(kobj_name, kernel_kobj);
```

**Result**:
- `/sys/kernel/quectel_rm520n_thermal0/temp`
- `/sys/kernel/quectel_rm520n_thermal1/temp`
- `/sys/kernel/quectel_rm520n_thermal2/temp`

**Changes to `src/kmod/hwmon.c`**:
```c
extern int instance;  // From main.c

// Use instance-specific device name
snprintf(hwmon_name, sizeof(hwmon_name), "quectel_rm520n_thermal%d", instance);
```

**Changes to `src/kmod/sensor.c`**:
```c
extern int instance;  // From main.c

// Use instance-specific thermal zone name
```

**Loading multiple instances**:
```bash
insmod quectel_rm520n_temp.ko instance=0
insmod quectel_rm520n_temp.ko instance=1
insmod quectel_rm520n_temp.ko instance=2
```

### 1.2 UCI Configuration Changes

**Current** (`/etc/config/quectel_rm520n_thermal`):
```ini
config settings 'config'
    option serial_port '/dev/ttyUSB2'
    option interval '30'
    ...
```

**New** (multi-instance format):
```ini
config modem 'modem0'
    option enabled '1'
    option label 'Primary'
    option serial_port '/dev/ttyUSB2'
    option interval '30'
    option baud_rate '115200'
    option instance '0'              # Kernel module instance
    ...

config modem 'modem1'
    option enabled '1'
    option label 'Secondary'
    option serial_port '/dev/ttyUSB5'
    option interval '30'
    option baud_rate '115200'
    option instance '1'
    ...

config modem 'modem2'
    option enabled '0'
    option label 'Tertiary'
    option serial_port '/dev/ttyUSB8'
    option interval '60'
    option baud_rate '115200'
    option instance '2'
    ...
```

**Migration strategy**:
- Auto-detect old format on first boot
- Convert to `config modem 'modem0'` with same settings
- Backward compatible

### 1.3 Daemon Instance Support

**Add `--instance` parameter** to daemon:

**Command line**:
```bash
quectel_rm520n_temp daemon --instance modem0
quectel_rm520n_temp daemon --instance modem1
```

**Changes to `src/main.c`**:
```c
// Add instance parameter parsing
static char *instance_name = NULL;

// In main():
case 'i':
    instance_name = optarg;
    break;
```

**Changes to `src/daemon.c`**:
```c
// Read UCI config for specific instance
config_read_uci_instance(&config, instance_name);

// Use instance-specific paths
snprintf(sysfs_path, sizeof(sysfs_path),
         "/sys/kernel/quectel_rm520n_thermal%d/temp", config.instance);

// Use instance-specific PID file
snprintf(pid_file, sizeof(pid_file),
         "/var/run/quectel_rm520n_temp_%s.pid", instance_name);
```

**Changes to `src/uci_config.c`**:
```c
// New function to read specific modem section
int config_read_uci_instance(config_t *config, const char *instance) {
    // Open UCI context
    // Load 'quectel_rm520n_thermal' config
    // Find section matching instance name
    // Read all options
}
```

### 1.4 Init Script Multi-Instance

**Changes to `files/etc/init.d/quectel_rm520n_thermal.init`**:

```bash
#!/bin/sh /etc/rc.common

START=99
STOP=10

USE_PROCD=1

start_service() {
    # Iterate through all modem sections
    config_load 'quectel_rm520n_thermal'
    config_foreach start_modem_instance 'modem'
}

start_modem_instance() {
    local cfg="$1"
    local enabled
    local label

    config_get_bool enabled "$cfg" 'enabled' '0'
    [ "$enabled" -eq 0 ] && return

    config_get label "$cfg" 'label' "$cfg"

    # Start instance
    procd_open_instance "$cfg"
    procd_set_param command /usr/bin/quectel_rm520n_temp daemon --instance "$cfg"
    procd_set_param respawn 3600 5 5
    procd_set_param stdout 1
    procd_set_param stderr 1
    procd_close_instance
}

service_triggers() {
    procd_add_reload_trigger "quectel_rm520n_thermal"
}

reload_service() {
    stop
    start
}
```

### 1.5 CLI Multi-Instance Support

**Add `--instance` and `--all` flags**:

```bash
# Read from specific instance
quectel_rm520n_temp read --instance modem0

# Read from all enabled instances
quectel_rm520n_temp read --all

# Status of all instances
quectel_rm520n_temp status --all
```

**Changes to `src/cli.c`**:
```c
// Add --instance and --all flags
// If --all, iterate through UCI modem sections
// Read from each enabled instance's sysfs path
```

### 1.6 Prometheus Collector Multi-Instance

**Changes to `files/extra/quectel_modem.uc`**:

```javascript
// Iterate through all modem instances from UCI
config_load('quectel_rm520n_thermal');
let modems = [];

uci.foreach('quectel_rm520n_thermal', 'modem', (s) => {
    if (s.enabled === '1') {
        push(modems, {
            name: s['.name'],
            label: s.label,
            instance: s.instance
        });
    }
});

// Export metrics for each instance with labels
for (let modem in modems) {
    let temp = read_sysfs(`/sys/kernel/quectel_rm520n_thermal${modem.instance}/temp`);

    m_temp({
        instance: modem.name,
        label: modem.label
    }, int(temp) / 1000);
}
```

**Metrics with labels**:
```prometheus
quectel_modem_temperature_celsius{instance="modem0",label="Primary"} 45.0
quectel_modem_temperature_celsius{instance="modem1",label="Secondary"} 42.0
```

### Phase 1 Deliverables

- ✅ Kernel modules support multiple instances
- ✅ UCI config supports multiple modem sections
- ✅ Init script starts multiple daemon instances
- ✅ CLI can query specific or all instances
- ✅ Prometheus exports metrics for all instances
- ✅ Each instance completely independent

**Files Modified**:
- `src/kmod/main.c` - Add instance parameter
- `src/kmod/hwmon.c` - Instance-specific names
- `src/kmod/sensor.c` - Instance-specific names
- `src/main.c` - Add --instance parameter
- `src/daemon.c` - Instance-specific paths
- `src/cli.c` - Add --instance and --all
- `src/uci_config.c` - Read instance-specific config
- `files/etc/init.d/*.init` - Multi-instance startup
- `files/extra/quectel_modem.uc` - Multi-instance metrics

**Testing** (can be done with single modem):
- Configure modem0 only: should work like before
- Configure modem0 and modem1 (same physical modem, different serial devices): both should work
- Disable modem1: only modem0 should run

**Estimated Effort**: 4-6 hours

---

## Phase 2: Parser Abstraction (4-6 days)

**Goal**: Create extensible parser system (Quectel only for now)

**Prerequisites**: Phase 1 completed (so parser works with multi-instance)

### 2.1 Parser Interface Definition

**Create `src/include/modem_parser.h`**:

```c
#ifndef MODEM_PARSER_H
#define MODEM_PARSER_H

#include "serial.h"
#include <time.h>

#define MAX_TEMP_SOURCES 8
#define PARSER_NAME_MAX 32

/**
 * Temperature reading from a single source
 */
typedef struct temp_reading {
    int temp_mdeg;              // Temperature in millidegrees Celsius
    char source[32];            // "modem", "cpu", "pa", etc.
    time_t timestamp;           // When reading was taken
} temp_reading_t;

/**
 * Modem parser interface
 * All parsers (built-in or custom) must implement this interface
 */
typedef struct modem_parser {
    char name[PARSER_NAME_MAX];     // "quectel", "simcom", "custom", etc.
    char description[128];          // Human-readable description

    /**
     * Initialize parser (optional)
     * Called once when daemon starts
     *
     * @param ctx Serial context
     * @return 0 on success, negative on error
     */
    int (*init)(serial_context_t *ctx);

    /**
     * Read and parse temperature (required)
     *
     * @param ctx Serial context
     * @param readings Array to store readings (at least MAX_TEMP_SOURCES)
     * @param max_readings Size of readings array
     * @return Number of readings on success, negative on error
     */
    int (*read_temp)(serial_context_t *ctx, temp_reading_t *readings, int max_readings);

    /**
     * Cleanup (optional)
     * Called when daemon exits
     */
    void (*cleanup)(void);

    void *private_data;  // Parser-specific data
} modem_parser_t;

#endif /* MODEM_PARSER_H */
```

### 2.2 Parser Registry

**Create `src/parser_registry.c` and `src/include/parser_registry.h`**:

```c
// parser_registry.h
#ifndef PARSER_REGISTRY_H
#define PARSER_REGISTRY_H

#include "modem_parser.h"

#define MAX_PARSERS 16

/**
 * Register a parser with the registry
 * @return 0 on success, negative on error
 */
int parser_register(modem_parser_t *parser);

/**
 * Get parser by name
 * @return Parser pointer or NULL if not found
 */
modem_parser_t *parser_get(const char *name);

/**
 * List all registered parsers
 * @param names Array to store parser names
 * @param max_count Size of names array
 * @return Number of parsers
 */
int parser_list(char names[][PARSER_NAME_MAX], int max_count);

/**
 * Initialize registry and register built-in parsers
 */
void parser_registry_init(void);

#endif /* PARSER_REGISTRY_H */
```

```c
// parser_registry.c
#include "parser_registry.h"
#include <string.h>
#include <stdio.h>

static modem_parser_t *parsers[MAX_PARSERS];
static int parser_count = 0;

int parser_register(modem_parser_t *parser) {
    if (parser_count >= MAX_PARSERS) {
        return -1;
    }

    // Check for duplicate names
    for (int i = 0; i < parser_count; i++) {
        if (strcmp(parsers[i]->name, parser->name) == 0) {
            return -2;  // Already registered
        }
    }

    parsers[parser_count++] = parser;
    return 0;
}

modem_parser_t *parser_get(const char *name) {
    for (int i = 0; i < parser_count; i++) {
        if (strcmp(parsers[i]->name, name) == 0) {
            return parsers[i];
        }
    }
    return NULL;
}

int parser_list(char names[][PARSER_NAME_MAX], int max_count) {
    int count = parser_count < max_count ? parser_count : max_count;
    for (int i = 0; i < count; i++) {
        strncpy(names[i], parsers[i]->name, PARSER_NAME_MAX - 1);
        names[i][PARSER_NAME_MAX - 1] = '\0';
    }
    return count;
}

void parser_registry_init(void) {
    // Register built-in parsers
    extern modem_parser_t quectel_parser;
    parser_register(&quectel_parser);
}
```

### 2.3 Refactor Quectel Code to Parser

**Create `src/parsers/quectel.c`**:

Move all Quectel-specific parsing logic from `src/temperature.c` here:

```c
#include "modem_parser.h"
#include "serial.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>

// Quectel-specific private data
typedef struct {
    char modem_prefix[64];
    char ap_prefix[64];
    char pa_prefix[64];
} quectel_data_t;

static int quectel_init(serial_context_t *ctx) {
    // No special initialization needed for Quectel
    return 0;
}

static int quectel_read_temp(serial_context_t *ctx, temp_reading_t *readings, int max_readings) {
    // Send AT+QTEMP command
    // Parse response for modem/AP/PA temperatures
    // Fill readings array
    // Return number of readings (typically 3)

    // This is the existing logic from temperature.c
    // Just refactored to match the interface

    int count = 0;

    // ... existing Quectel parsing code ...

    if (modem_temp_valid) {
        readings[count].temp_mdeg = modem_temp * 1000;
        strncpy(readings[count].source, "modem", 32);
        readings[count].timestamp = time(NULL);
        count++;
    }

    if (ap_temp_valid) {
        readings[count].temp_mdeg = ap_temp * 1000;
        strncpy(readings[count].source, "cpu", 32);
        readings[count].timestamp = time(NULL);
        count++;
    }

    if (pa_temp_valid) {
        readings[count].temp_mdeg = pa_temp * 1000;
        strncpy(readings[count].source, "pa", 32);
        readings[count].timestamp = time(NULL);
        count++;
    }

    return count;
}

static void quectel_cleanup(void) {
    // No cleanup needed
}

modem_parser_t quectel_parser = {
    .name = "quectel",
    .description = "Quectel modems (RM520N, RM500Q, etc.) using AT+QTEMP",
    .init = quectel_init,
    .read_temp = quectel_read_temp,
    .cleanup = quectel_cleanup,
    .private_data = NULL
};
```

### 2.4 Update Daemon to Use Parser Interface

**Changes to `src/daemon.c`**:

```c
#include "parser_registry.h"

// In daemon_mode():
parser_registry_init();

// Read modem_type from UCI config
modem_parser_t *parser = parser_get(config.modem_type);
if (!parser) {
    logging_error("Unknown modem type: %s", config.modem_type);
    return -1;
}

// Initialize parser
if (parser->init) {
    if (parser->init(&serial_ctx) != 0) {
        logging_error("Parser initialization failed");
        return -1;
    }
}

// In main loop:
temp_reading_t readings[MAX_TEMP_SOURCES];
int num_readings = parser->read_temp(&serial_ctx, readings, MAX_TEMP_SOURCES);

if (num_readings > 0) {
    // Select highest temperature (existing behavior)
    int best_temp_mdeg = readings[0].temp_mdeg;
    const char *best_source = readings[0].source;

    for (int i = 1; i < num_readings; i++) {
        if (readings[i].temp_mdeg > best_temp_mdeg) {
            best_temp_mdeg = readings[i].temp_mdeg;
            best_source = readings[i].source;
        }
    }

    logging_info("Best temperature: %d°C from %s",
                 best_temp_mdeg / 1000, best_source);

    // Write to sysfs, update kernel, etc.
}

// Cleanup
if (parser->cleanup) {
    parser->cleanup();
}
```

### 2.5 UCI Configuration Extension

**Add `modem_type` option**:

```ini
config modem 'modem0'
    option enabled '1'
    option label 'Primary'
    option modem_type 'quectel'        # NEW: Parser type
    option serial_port '/dev/ttyUSB2'
    option instance '0'
    ...

    # Quectel-specific options (backward compatible)
    option temp_modem_prefix 'modem-ambient-usr'
    option temp_ap_prefix 'cpuss-0-usr'
    option temp_pa_prefix 'modem-lte-sub6-pa1'
```

**Default value**: If `modem_type` not specified, default to `'quectel'` for backward compatibility

**Changes to `src/include/config.h`**:

```c
typedef struct {
    char modem_type[32];           // NEW: "quectel", "simcom", "custom", etc.
    char serial_port[CONFIG_STRING_LEN];
    int interval;
    int instance;                  // From Phase 1
    // ... existing fields ...
} config_t;
```

### Phase 2 Deliverables

- ✅ Parser interface defined
- ✅ Parser registry working
- ✅ Quectel code refactored to parser
- ✅ Daemon uses parser interface
- ✅ All existing functionality preserved
- ✅ Ready for additional parsers

**Files Created**:
- `src/include/modem_parser.h`
- `src/include/parser_registry.h`
- `src/parser_registry.c`
- `src/parsers/quectel.c`

**Files Modified**:
- `src/daemon.c` - Use parser interface
- `src/include/config.h` - Add modem_type
- `src/uci_config.c` - Read modem_type
- `Makefile` - Add parser sources

**Testing**:
- Existing Quectel functionality still works
- Multi-instance + parser works
- Different instances can use same parser

**Estimated Effort**: 4-6 days

---

## Phase 3: Second Built-in Parser (3-4 days)

**Goal**: Prove parser abstraction works with a real second modem

**Challenge**: No hardware available for testing

### 3.1 Choose Target: SIMCom (Theory Only)

**Why SIMCom**:
- Most popular alternative to Quectel
- Used in many IoT devices (SIM800, SIM900, SIM7600)
- Similar multi-value response format

**AT Command**: `AT+CPTEMP`

**Expected Response**:
```
AT+CPTEMP
+CPTEMP: "CPU",45
+CPTEMP: "PA",50
OK
```

### 3.2 Implement SIMCom Parser (Theoretical)

**Create `src/parsers/simcom.c`**:

```c
#include "modem_parser.h"
#include "serial.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>

static int simcom_init(serial_context_t *ctx) {
    // Test modem responds to AT
    // Could send AT+CPTEMP? to verify command exists
    return 0;
}

static int simcom_read_temp(serial_context_t *ctx, temp_reading_t *readings, int max_readings) {
    char command[] = "AT+CPTEMP\r";
    char response[512];

    // Send command
    if (serial_send_command(ctx, command) != 0) {
        logging_error("Failed to send AT+CPTEMP");
        return -1;
    }

    // Read response
    if (serial_read_response(ctx, response, sizeof(response), 2000) <= 0) {
        logging_error("Failed to read AT+CPTEMP response");
        return -2;
    }

    // Parse response: +CPTEMP: "CPU",45
    //                 +CPTEMP: "PA",50
    int count = 0;
    char *line = strtok(response, "\r\n");

    while (line && count < max_readings) {
        if (strncmp(line, "+CPTEMP:", 8) == 0) {
            char source[32];
            int temp;

            // Parse: +CPTEMP: "CPU",45
            if (sscanf(line, "+CPTEMP: \"%[^\"]\", %d", source, &temp) == 2) {
                readings[count].temp_mdeg = temp * 1000;  // Assume celsius
                strncpy(readings[count].source, source, 31);
                readings[count].source[31] = '\0';
                readings[count].timestamp = time(NULL);
                count++;
            }
        }
        line = strtok(NULL, "\r\n");
    }

    if (count == 0) {
        logging_error("No temperature values found in response");
        return -3;
    }

    return count;
}

static void simcom_cleanup(void) {
    // No cleanup needed
}

modem_parser_t simcom_parser = {
    .name = "simcom",
    .description = "SIMCom modems (SIM800, SIM900, SIM7600) using AT+CPTEMP",
    .init = simcom_init,
    .read_temp = simcom_read_temp,
    .cleanup = simcom_cleanup,
    .private_data = NULL
};
```

### 3.3 Register SIMCom Parser

**Changes to `src/parser_registry.c`**:

```c
void parser_registry_init(void) {
    extern modem_parser_t quectel_parser;
    extern modem_parser_t simcom_parser;

    parser_register(&quectel_parser);
    parser_register(&simcom_parser);
}
```

### 3.4 Makefile Changes

```makefile
PARSER_SOURCES := \
    $(PKG_BUILD_DIR)/parsers/quectel.c \
    $(PKG_BUILD_DIR)/parsers/simcom.c

$(TARGET_CC) ... $(PARSER_SOURCES) ...
```

### 3.5 UCI Configuration Example

```ini
config modem 'modem0'
    option enabled '1'
    option label 'Quectel_Primary'
    option modem_type 'quectel'
    option serial_port '/dev/ttyUSB2'
    option instance '0'

config modem 'modem1'
    option enabled '1'
    option label 'SIMCom_Backup'
    option modem_type 'simcom'
    option serial_port '/dev/ttyUSB5'
    option instance '1'
```

### Phase 3 Deliverables

- ✅ SIMCom parser implemented (untested - no hardware)
- ✅ Different parsers can coexist
- ✅ Multi-instance + multi-parser proven
- ⚠️ **Cannot test without hardware**

**Testing Strategy Without Hardware**:
1. Code review for correctness
2. Mock testing with simulated responses
3. Community testing when project released
4. Document as "theoretical implementation"

**Files Created**:
- `src/parsers/simcom.c`

**Files Modified**:
- `src/parser_registry.c` - Register SIMCom
- `Makefile` - Add SIMCom sources

**Estimated Effort**: 3-4 days (implementation + mock testing)

---

## Phase 4: Declarative UCI Parser (3-4 days)

**Goal**: Allow custom modems without C code

### 4.1 Declarative Parser Implementation

**Create `src/parsers/declarative.c`**:

```c
#include "modem_parser.h"
#include "config.h"
#include <regex.h>
#include <string.h>

typedef struct {
    char at_command[128];
    regex_t response_regex;
    int temp_field;              // Which capture group
    char temp_unit[32];          // celsius, millicelsius, kelvin
    int adc_offset;              // For ADC conversion
    int adc_divisor;
    bool multi_line;             // Parse multiple lines
} declarative_config_t;

static declarative_config_t decl_config;

static int declarative_init(serial_context_t *ctx) {
    // Read declarative config from UCI
    // Compile regex pattern
    // Validate configuration

    // Example:
    // at_command = "AT#TEMP"
    // response_pattern = "^#TEMP: ([0-9]+)$"
    // temp_field = 1
    // temp_unit = "celsius"

    if (regcomp(&decl_config.response_regex,
                config.custom_response_pattern,
                REG_EXTENDED) != 0) {
        logging_error("Invalid response pattern regex");
        return -1;
    }

    return 0;
}

static int declarative_read_temp(serial_context_t *ctx, temp_reading_t *readings, int max_readings) {
    char response[512];

    // Send AT command
    if (serial_send_command(ctx, decl_config.at_command) != 0) {
        return -1;
    }

    // Read response
    if (serial_read_response(ctx, response, sizeof(response), 2000) <= 0) {
        return -2;
    }

    // Parse with regex
    regmatch_t matches[10];
    int count = 0;

    if (decl_config.multi_line) {
        // Parse each line
        char *line = strtok(response, "\r\n");
        while (line && count < max_readings) {
            if (regexec(&decl_config.response_regex, line, 10, matches, 0) == 0) {
                // Extract temperature from capture group
                char temp_str[32];
                int len = matches[decl_config.temp_field].rm_eo -
                         matches[decl_config.temp_field].rm_so;
                strncpy(temp_str, line + matches[decl_config.temp_field].rm_so, len);
                temp_str[len] = '\0';

                int temp = atoi(temp_str);

                // Convert units
                if (strcmp(decl_config.temp_unit, "celsius") == 0) {
                    readings[count].temp_mdeg = temp * 1000;
                } else if (strcmp(decl_config.temp_unit, "millicelsius") == 0) {
                    readings[count].temp_mdeg = temp;
                } else if (strcmp(decl_config.temp_unit, "adc") == 0) {
                    // ADC formula: (value - offset) / divisor
                    readings[count].temp_mdeg =
                        ((temp - decl_config.adc_offset) * 1000) / decl_config.adc_divisor;
                }

                strcpy(readings[count].source, "modem");
                readings[count].timestamp = time(NULL);
                count++;
            }
            line = strtok(NULL, "\r\n");
        }
    } else {
        // Single line parsing
        if (regexec(&decl_config.response_regex, response, 10, matches, 0) == 0) {
            // ... same extraction logic ...
            count = 1;
        }
    }

    return count;
}

static void declarative_cleanup(void) {
    regfree(&decl_config.response_regex);
}

modem_parser_t declarative_parser = {
    .name = "custom",
    .description = "Custom modem using declarative UCI configuration",
    .init = declarative_init,
    .read_temp = declarative_read_temp,
    .cleanup = declarative_cleanup,
    .private_data = NULL
};
```

### 4.2 UCI Configuration for Declarative Parser

**Add custom_* options to UCI schema**:

```ini
config modem 'modem2'
    option enabled '1'
    option label 'Custom_Telit'
    option modem_type 'custom'
    option serial_port '/dev/ttyUSB8'
    option instance '2'

    # Declarative parser config
    option custom_at_command 'AT#TEMP'
    option custom_response_pattern '^#TEMP: ([0-9]+)$'
    option custom_temp_field '1'
    option custom_temp_unit 'celsius'

    # Optional: Multi-line responses
    option custom_multi_line '0'

    # Optional: ADC conversion
    #option custom_temp_unit 'adc'
    #option custom_adc_offset '1024'
    #option custom_adc_divisor '10'
```

**Example: Telit AT#TEMP**:
```ini
option modem_type 'custom'
option custom_at_command 'AT#TEMP'
option custom_response_pattern '^#TEMP: ([0-9]+)$'
option custom_temp_field '1'
option custom_temp_unit 'celsius'
```

**Example: u-blox AT+UTEMP**:
```ini
option modem_type 'custom'
option custom_at_command 'AT+UTEMP'
option custom_response_pattern '^\+UTEMP: ([0-9]+)$'
option custom_temp_field '1'
option custom_temp_unit 'millicelsius'
```

### 4.3 Configuration Validation

**On daemon startup**:
- Validate regex pattern compiles
- Check all required custom_* options present
- Test AT command format
- Log configuration details

### Phase 4 Deliverables

- ✅ Declarative parser working
- ✅ UCI configuration flexible
- ✅ Covers simple custom modems (~70%)
- ✅ No code changes needed for new modems

**Features**:
- Single or multi-line response parsing
- POSIX regex with capture groups
- Unit conversion (celsius, millicelsius, ADC)
- Validation on startup

**Limitations**:
- Single AT command only
- No initialization sequences
- No complex state machines
- Line-based parsing only

**Files Created**:
- `src/parsers/declarative.c`

**Files Modified**:
- `src/parser_registry.c` - Register declarative parser
- `src/include/config.h` - Add custom_* fields
- `src/uci_config.c` - Read custom_* options
- `Makefile` - Add declarative parser

**Estimated Effort**: 3-4 days

---

## Phase 5: Documentation & Testing (2-3 days)

**Goal**: Complete documentation and testing plan

### 5.1 Documentation Updates

**README.md**:
- Add "Multi-Instance Support" section
- Add "Multi-Modem Parser Support" section
- Update all command examples
- Document built-in parsers (Quectel, SIMCom)
- Document declarative parser configuration
- Add examples for mixed configurations

**Create docs/PARSERS.md**:
```markdown
# Modem Parser Reference

## Built-in Parsers

### Quectel (RM520N, RM500Q, etc.)
- AT Command: AT+QTEMP
- Response: Multi-value (modem/CPU/PA temps)
- Unit: Celsius

### SIMCom (SIM800, SIM900, SIM7600)
- AT Command: AT+CPTEMP
- Response: Multi-value (CPU/PA temps)
- Unit: Celsius
- Status: Untested (no hardware)

## Custom Parser Examples

### Telit AT#TEMP
[Configuration example]

### u-blox AT+UTEMP
[Configuration example]

## Creating Custom Parsers

[Declarative UCI guide]
```

**Create docs/MULTI_INSTANCE.md**:
```markdown
# Multi-Instance Configuration Guide

## Overview
Run multiple daemon instances for multiple modems...

## Configuration Examples
[Examples]

## Prometheus Metrics
[How metrics work with multiple instances]
```

**Update UPGRADING.md** (if renamed):
- Migration from quectel-rm520n-thermal
- Config file changes
- Binary/path name changes

### 5.2 Testing Strategy

**Without hardware for other modems**:

1. **Unit Testing** (not required per user request):
   - Skip for now

2. **Manual Testing Scenarios**:

**Scenario 1: Single Instance Quectel** (can test now)
- Configure modem0 only
- Verify works like before Phase 1

**Scenario 2: Multi-Instance Quectel** (can test with USB hub + multiple ports)
- Configure modem0 and modem1
- Point to same physical modem but different virtual ports
- Verify both instances run independently

**Scenario 3: Parser Abstraction** (can test now)
- Ensure Quectel parser works
- Verify parser selection from UCI

**Scenario 4: SIMCom Parser** (cannot test - no hardware)
- Code review only
- Mark as "community testing needed"

**Scenario 5: Declarative Parser** (can test theoretically)
- Configure for Quectel using declarative parser
- Should match built-in Quectel results

**Scenario 6: Mixed Configuration** (theoretical)
- modem0: Quectel (built-in parser)
- modem1: SIMCom (built-in parser)
- modem2: Custom (declarative parser)
- All should work independently

3. **Community Testing Program**:
   - Release as beta
   - Request testing from users with different modems
   - Collect feedback and fix issues
   - Document tested modem models

### 5.3 CLI Help Updates

**Add parser listing command**:
```bash
# List available parsers
at-modem-temp parsers

Output:
Available modem parsers:
  quectel   - Quectel modems (RM520N, RM500Q, etc.) using AT+QTEMP
  simcom    - SIMCom modems (SIM800, SIM900, SIM7600) using AT+CPTEMP
  custom    - Custom modem using declarative UCI configuration
```

### Phase 5 Deliverables

- ✅ Complete documentation
- ✅ Testing scenarios defined
- ✅ Community testing plan
- ✅ Updated CLI help

**Files Created**:
- `docs/PARSERS.md`
- `docs/MULTI_INSTANCE.md`

**Files Modified**:
- `README.md` - Comprehensive updates
- `src/cli.c` - Add `parsers` command

**Estimated Effort**: 2-3 days

---

## File Structure After Completion

```
at-modem-thermal/                    # Renamed from quectel-rm520n-thermal
├── src/
│   ├── include/
│   │   ├── modem_parser.h           # NEW: Parser interface
│   │   ├── parser_registry.h        # NEW: Registry API
│   │   └── config.h                 # Modified: Add modem_type, custom_* fields
│   ├── parsers/                     # NEW: Parser directory
│   │   ├── quectel.c                # Built-in: Quectel parser
│   │   ├── simcom.c                 # Built-in: SIMCom parser
│   │   └── declarative.c            # Declarative UCI parser
│   ├── parser_registry.c            # NEW: Registry implementation
│   ├── kmod/                        # Modified for multi-instance
│   │   ├── main.c                   # Add instance parameter
│   │   ├── hwmon.c                  # Instance-specific names
│   │   └── sensor.c                 # Instance-specific names
│   ├── main.c                       # Modified: Add --instance parameter
│   ├── daemon.c                     # Modified: Use parser interface
│   ├── cli.c                        # Modified: Add --instance, --all, parsers
│   ├── uci_config.c                 # Modified: Read instance + modem_type
│   └── ...
├── files/
│   ├── etc/
│   │   ├── config/
│   │   │   └── at_modem_thermal     # Renamed, new format
│   │   └── init.d/
│   │       └── at_modem_thermal.init # Renamed, multi-instance support
│   └── extra/
│       └── quectel_modem.uc         # Modified: Multi-instance metrics
├── docs/                            # NEW: Extended documentation
│   ├── PARSERS.md                   # Parser reference
│   └── MULTI_INSTANCE.md            # Multi-instance guide
├── README.md                        # Major updates
├── UPGRADING.md                     # NEW: Migration guide
├── TODO_NEW_PROJECT.md              # This file
└── Makefile                         # Updated: New name, parser sources

Binary: /usr/bin/at-modem-temp       # Renamed from quectel_rm520n_temp
```

---

## Configuration Examples

### Example 1: Single Quectel Modem (Backward Compatible)

```ini
config modem 'modem0'
    option enabled '1'
    option modem_type 'quectel'
    option serial_port '/dev/ttyUSB2'
    option instance '0'
    option interval '30'
    # All existing Quectel options work
```

### Example 2: Multiple Quectel Modems

```ini
config modem 'modem0'
    option enabled '1'
    option label 'Quectel_Front'
    option modem_type 'quectel'
    option serial_port '/dev/ttyUSB2'
    option instance '0'
    option interval '30'

config modem 'modem1'
    option enabled '1'
    option label 'Quectel_Back'
    option modem_type 'quectel'
    option serial_port '/dev/ttyUSB5'
    option instance '1'
    option interval '30'
```

### Example 3: Mixed Modem Types

```ini
config modem 'modem0'
    option enabled '1'
    option label 'Primary_Quectel'
    option modem_type 'quectel'
    option serial_port '/dev/ttyUSB2'
    option instance '0'

config modem 'modem1'
    option enabled '1'
    option label 'Backup_SIMCom'
    option modem_type 'simcom'
    option serial_port '/dev/ttyUSB5'
    option instance '1'

config modem 'modem2'
    option enabled '0'
    option label 'Test_Telit'
    option modem_type 'custom'
    option serial_port '/dev/ttyUSB8'
    option instance '2'
    option custom_at_command 'AT#TEMP'
    option custom_response_pattern '^#TEMP: ([0-9]+)$'
    option custom_temp_field '1'
    option custom_temp_unit 'celsius'
```

### Example 4: Custom Modem (u-blox)

```ini
config modem 'modem0'
    option enabled '1'
    option label 'ublox_TOBY'
    option modem_type 'custom'
    option serial_port '/dev/ttyUSB2'
    option instance '0'
    option custom_at_command 'AT+UTEMP'
    option custom_response_pattern '^\+UTEMP: ([0-9]+)$'
    option custom_temp_field '1'
    option custom_temp_unit 'millicelsius'
```

---

## Effort Estimate

| Phase | Days | Lines of Code | Priority |
|-------|------|---------------|----------|
| Phase 1: Multi-Instance | 0.5-0.75 days (4-6 hours) | ~400 LOC | **CRITICAL** |
| Phase 2: Parser Abstraction | 4-6 days | ~800 LOC | **HIGH** |
| Phase 3: SIMCom Parser | 3-4 days | ~200 LOC | **MEDIUM** (untested) |
| Phase 4: Declarative Parser | 3-4 days | ~400 LOC | **MEDIUM** |
| Phase 5: Documentation | 2-3 days | ~1000 lines docs | **HIGH** |

**Total Minimum**: 12.5-17.75 days (~3-4 weeks)
**Total Maximum**: 20+ days with thorough testing

**Critical Path**:
1. Multi-instance (enables everything else)
2. Parser abstraction (enables multiple parsers)
3. Documentation (required for release)

**Optional/Can Wait**:
- Additional built-in parsers (add as hardware becomes available)
- Declarative parser refinements
- Community feedback integration

---

## Testing Strategy

### What We Can Test Now

1. ✅ **Phase 1 (Multi-Instance)** - Full testing possible
   - Single instance: Works like before
   - Multiple instances: Use USB hub with multiple ports to same modem
   - Verify sysfs paths, PID files, independent operation

2. ✅ **Phase 2 (Parser Abstraction)** - Full testing possible
   - Quectel parser through new interface
   - Regression testing: All existing functionality works
   - Multi-instance + parser combination

3. ⚠️ **Phase 3 (SIMCom)** - Cannot fully test
   - Code review only
   - Mock testing with simulated responses
   - Mark as "beta - community testing needed"

4. ✅ **Phase 4 (Declarative)** - Partial testing possible
   - Configure Quectel using declarative parser
   - Should match built-in parser results
   - Validates declarative engine works

### Community Testing Program

**Release Strategy**:
1. Release Phase 1 + 2 as stable (tested with Quectel)
2. Release Phase 3 + 4 as beta (community testing)
3. Clearly mark untested parsers in documentation
4. Request feedback from users with different modems
5. Update parsers based on real-world testing

**Documentation Requirements**:
- Each parser marked as "Tested" or "Community Testing"
- List of confirmed working modem models
- Troubleshooting section for common issues
- How to report parser bugs

---

## Future Parsers (Post-Release)

Add more built-in parsers as hardware becomes available or community contributes:

### Tier 1: Popular (When hardware available)
- **Telit** - `AT#TEMP`, `AT#TAD`
- **u-blox** - `AT+UTEMP`
- **Sierra Wireless** - `AT!TEMP`, `AT!DieTemp`

### Tier 2: Less Common (Community contributions)
- **Huawei** - Various AT commands
- **ZTE** - Various AT commands
- **Other manufacturers** - As requested

**Each new parser**: ~1-2 days development + testing

---

## Migration Path

### For Existing Users (if renamed)

**Option 1: Automatic Migration Script**

Create `/usr/bin/migrate-to-at-modem-thermal`:
```bash
#!/bin/sh
# Migrate from quectel-rm520n-thermal to at-modem-thermal

# Check if old config exists
if [ -f /etc/config/quectel_rm520n_thermal ]; then
    # Convert old config to new format
    # Create modem0 section from old settings section
    # Set modem_type='quectel'
    # Backup old config
    # Load new config
fi
```

**Option 2: Keep Both Projects**

- `quectel-rm520n-thermal` - Legacy, maintenance mode only
- `at-modem-thermal` - New project, active development
- Users migrate when ready

**Recommendation**: Option 2 (separate projects) to avoid breaking existing users

---

## Key Decisions Pending

1. **Project Name**:
   - Preferred: `at-modem-thermal`
   - Alternative: `serial-modem-thermal`
   - Final decision?

2. **Migration Strategy**:
   - New project or rename existing?
   - Full rename or partial backward compatibility?

3. **Multi-Instance vs Parser Priority**:
   - Confirmed: Multi-instance first
   - Then parser abstraction

4. **Testing Without Hardware**:
   - Release untested parsers as beta?
   - Community testing program?

5. **Release Strategy**:
   - Big bang (all phases at once)?
   - Incremental (Phase 1-2, then 3-4)?

---

## Risk Assessment

### High Risks

1. **No Hardware for Testing Other Modems** 🔴
   - **Mitigation**: Mark as beta, community testing, good code review
   - **Impact**: Parsers may not work on first release

2. **Complex Multi-Instance Changes** 🟡
   - **Mitigation**: Extensive testing with Phase 1, keep simple
   - **Impact**: Could break existing single-instance users if not careful

3. **Project Rename Breaking Users** 🟡
   - **Mitigation**: Separate new project, clear migration docs
   - **Impact**: Users confused, need clear communication

### Medium Risks

4. **Parser Abstraction Overhead** 🟡
   - **Mitigation**: Keep interface simple, minimal overhead
   - **Impact**: Slight performance impact (negligible for 30s polling)

5. **Declarative Parser Complexity** 🟢
   - **Mitigation**: Good examples, validation on startup
   - **Impact**: Users might struggle with regex patterns

### Low Risks

6. **UCI Config Changes** 🟢
   - **Mitigation**: Backward compatible defaults
   - **Impact**: Minimal, well-tested in OpenWRT

---

## Success Criteria

**Must Have**:
- ✅ Phase 1 (multi-instance) fully working with Quectel
- ✅ Phase 2 (parser abstraction) maintains all existing functionality
- ✅ Single Quectel modem still works (regression test passes)
- ✅ Documentation complete and clear

**Should Have**:
- ✅ Phase 4 (declarative parser) working
- ✅ SIMCom parser code complete (even if untested)
- ✅ Multi-instance + multi-parser tested
- ✅ Community testing program in place

**Nice to Have**:
- ✅ Real hardware testing for SIMCom
- ✅ More built-in parsers
- ✅ ucode script support (future)

---

## Next Steps

1. **Decide on project name**: `at-modem-thermal` vs alternatives
2. **Decide on migration**: New project vs refactor
3. **Begin Phase 1**: Multi-instance support (4-6 hours)
4. **Test Phase 1**: Verify with single Quectel modem
5. **Continue to Phase 2**: Parser abstraction

**Hold point after Phase 1**: Verify multi-instance works before continuing

---

## Notes

- This is a **major feature addition** - plan for 3-4 weeks of development
- **Testing limited** by lack of hardware - accept this limitation
- **Community involvement critical** for testing other modems
- **Documentation is key** - users need clear examples
- **Backward compatibility important** - don't break existing users
- Consider this a **new project** rather than a major version bump

---

*Last Updated: [Current Date]*
*Status: Planning Phase*
*Author: Planning discussion with user*
