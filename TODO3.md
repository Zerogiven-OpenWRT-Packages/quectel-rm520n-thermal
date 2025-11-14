Multi-modem support would make this a truly universal tool. Let me create a comprehensive plan:

  Architecture Decision: Hybrid Approach

  After analyzing the requirements, I recommend a three-tier parser system:

  1. Built-in native parsers (C code) for popular modems - fast and reliable
  2. Declarative UCI config for simple AT command variations - no code needed
  3. ucode parser scripts (optional) for complex custom modems - maximum flexibility

  Design Overview

  Core Abstraction: Modem Parser Interface

  Every parser (built-in or custom) implements a standard interface:

  typedef struct modem_parser {
      const char *name;                    // "quectel", "telit", etc.
      int (*init)(serial_context_t *);     // Initialize modem
      int (*read_temp)(serial_context_t *, temp_reading_t *);
      int (*parse_response)(const char *, temp_reading_t *);
      const char **(*get_sources)(void);   // Available temp sources
      void (*cleanup)(void);
  } modem_parser_t;

  UCI Configuration Structure

  config modem 'settings'
      # Option 1: Use built-in parser (most users)
      option modem_type 'quectel'       # quectel, telit, ublox, simcom, sierra
      option model 'RM520N'

      # Option 2: Declarative custom parser (simple cases)
      option modem_type 'custom'
      option parser_mode 'declarative'
      option at_command 'AT#TEMP'
      option response_pattern '^#TEMP: ([0-9]+)$'
      option temp_unit 'celsius'        # celsius, millicelsius, adc
      option temp_field '1'             # Which capture group

      # Option 3: ucode script parser (complex cases)
      option modem_type 'custom'
      option parser_mode 'ucode'
      option parser_script '/etc/quectel/parsers/mymodem.uc'

      # Common options for all types
      option serial_port '/dev/ttyUSB2'
      option interval '30'
      ...

  ---
  Implementation Plan

  Phase 1: Foundation & Refactoring (3-5 days)

  Goal: Create parser abstraction without breaking existing functionality

  Tasks:
  1. Create parser interface
    - Define modem_parser_t structure in src/include/modem_parser.h
    - Define temp_reading_t structure (supports multiple temp sources)
  2. Implement parser registry
    - src/parser_registry.c - register and lookup parsers
    - parser_register(), parser_get() functions
  3. Refactor existing Quectel code
    - Move Quectel-specific code to src/parsers/quectel.c
    - Implement parser interface for Quectel
    - Update daemon to use parser interface
  4. Update UCI config
    - Add modem_type option (default: 'quectel' for backward compatibility)
    - Read modem_type and select parser

  Files to create:
  - src/include/modem_parser.h - Interface definition
  - src/parser_registry.c - Registry implementation
  - src/parsers/quectel.c - Quectel parser implementation

  Files to modify:
  - src/daemon.c - Use parser interface
  - src/temperature.c - Refactor to use parser
  - src/include/config.h - Add modem_type field

  Deliverable: Same functionality, but extensible architecture

  ---
  Phase 2: Built-in Parser Library (5-7 days)

  Goal: Add native parsers for popular modems

  Implementation:

  Parser: Telit (src/parsers/telit.c)

  AT#TEMP       → #TEMP: 45
  AT#TAD        → #TAD: 2345  (ADC value, needs conversion)
  - Handle both commands
  - ADC conversion: temp_celsius = (adc_value - 1024) / 10

  Parser: u-blox (src/parsers/ublox.c)

  AT+UTEMP      → +UTEMP: 45000 (millidegrees)
  - Similar to Quectel format
  - Return in millidegrees

  Parser: SIMCom (src/parsers/simcom.c)

  AT+CPTEMP     → +CPTEMP: "CPU",45
                  +CPTEMP: "PA",50
  - Multi-value response parsing
  - Support multiple temperature sources
  - Similar to Quectel (has modem/AP/PA temps)

  Parser: Sierra Wireless (src/parsers/sierra.c)

  AT!TEMP       → Temperature: 45 C
  AT!DieTemp    → Die Temp: 45 C
  - Non-standard format
  - Regex: Temperature: ([0-9]+) C

  Common Challenges:
  1. Error handling - Each modem has different error responses
  2. Multi-source temps - Some modems report CPU, PA, modem separately
  3. Unit variations - °C, m°C, ADC values, Kelvin
  4. Initialization - Some modems need setup commands

  Makefile changes:
  PARSER_SOURCES := \
      $(PKG_BUILD_DIR)/parsers/quectel.c \
      $(PKG_BUILD_DIR)/parsers/telit.c \
      $(PKG_BUILD_DIR)/parsers/ublox.c \
      $(PKG_BUILD_DIR)/parsers/simcom.c \
      $(PKG_BUILD_DIR)/parsers/sierra.c

  Registry initialization (in daemon.c or parser_registry.c):
  void register_all_parsers(void) {
      parser_register(&quectel_parser);
      parser_register(&telit_parser);
      parser_register(&ublox_parser);
      parser_register(&simcom_parser);
      parser_register(&sierra_parser);
  }

  Deliverable: Support for 5 major modem manufacturers out of the box

  ---
  Phase 3: Declarative UCI Parser (3-4 days)

  Goal: Allow simple custom modems without code

  Implementation:

  Create src/parsers/declarative.c that reads UCI config and constructs a parser dynamically:

  Configuration Example:
  config modem 'settings'
      option modem_type 'custom'
      option parser_mode 'declarative'

      # AT command to send
      option at_command 'AT#TEMP'

      # POSIX regex pattern (with capture groups)
      option response_pattern '^#TEMP: ([0-9]+)$'

      # Which capture group contains temperature
      option temp_field '1'

      # Unit conversion
      option temp_unit 'celsius'          # celsius, millicelsius, adc
      option adc_offset '1024'            # For ADC: (value - offset) / divisor
      option adc_divisor '10'

      # Optional: Multiple temperatures
      option multi_temp '0'               # Set to 1 for multi-value responses
      option temp_source_pattern '+"([^"]+)",([0-9]+)'  # Extract name and value

  Features:
  - POSIX regex for response parsing
  - Simple unit conversions (×1000, ÷1000, ADC formula)
  - Support for single or multi-value responses
  - Validation on daemon startup

  Limitations:
  - Can't handle complex initialization sequences
  - Limited to line-based responses
  - No conditional logic

  Covers ~80% of simple AT command modems

  Deliverable: Users can add new simple modems via UCI config

  ---
  Phase 4: ucode Parser Scripts (4-6 days, OPTIONAL)

  Goal: Maximum flexibility for complex modems

  Implementation:

  Allow users to write custom parser in ucode:

  Configuration:
  config modem 'settings'
      option modem_type 'custom'
      option parser_mode 'ucode'
      option parser_script '/etc/quectel/parsers/custom_modem.uc'

  ucode Parser API:
  // /etc/quectel/parsers/custom_modem.uc

  // Called once on daemon startup
  function init() {
      // Send initialization commands if needed
      serial.send("AT");
      let response = serial.read();
      return 0;  // 0 = success
  }

  // Called every polling interval
  function read_temperature() {
      serial.send("AT#TEMP");
      let response = serial.read();

      // Parse response
      let match = match(response, /^#TEMP: ([0-9]+)/);
      if (!match) return null;

      let temp_celsius = int(match[1]);

      // Return temperature reading
      return {
          source: "modem",
          temp_mdeg: temp_celsius * 1000,
          timestamp: time()
      };
  }

  // Export functions
  export { init, read_temperature };

  Provide C API for scripts:
  // In daemon, expose serial functions to ucode
  serial.send(command)
  serial.read(timeout_ms)
  serial.flush()

  Advantages:
  - Full parsing flexibility
  - Can handle complex initialization
  - State machines possible
  - User-modifiable without recompilation

  Challenges:
  - Security (arbitrary code execution)
  - Error handling in scripts
  - Performance overhead (minimal for every 30s polling)

  Deliverable: Advanced users can support any modem

  ---
  File Structure

  src/
  ├── include/
  │   ├── modem_parser.h           # Parser interface
  │   └── parser_registry.h        # Registry functions
  ├── parsers/
  │   ├── quectel.c                # Quectel RM520N/RM500Q/etc
  │   ├── telit.c                  # Telit modules
  │   ├── ublox.c                  # u-blox modules
  │   ├── simcom.c                 # SIMCom SIM800/SIM900
  │   ├── sierra.c                 # Sierra Wireless EM/MC
  │   ├── declarative.c            # UCI-based parser
  │   └── ucode_parser.c           # (optional) ucode script loader
  ├── parser_registry.c            # Parser registration/lookup
  └── ...

  files/
  └── parsers/                     # Example ucode parsers
      ├── README.md
      └── custom_example.uc

  ---
  Configuration Examples

  Example 1: Built-in Telit

  config modem 'settings'
      option modem_type 'telit'
      option serial_port '/dev/ttyUSB2'
      option temp_command 'AT#TEMP'     # or 'AT#TAD' for ADC

  Example 2: Custom Simple Modem (Declarative)

  config modem 'settings'
      option modem_type 'custom'
      option parser_mode 'declarative'
      option at_command 'AT$TEMP?'
      option response_pattern '^\$TEMP: ([0-9]+)$'
      option temp_field '1'
      option temp_unit 'celsius'

  Example 3: Complex Custom Modem (ucode)

  config modem 'settings'
      option modem_type 'custom'
      option parser_mode 'ucode'
      option parser_script '/etc/quectel/parsers/mymodem.uc'

  ---
  Backward Compatibility

  Ensure zero breaking changes:

  # Old config (still works):
  config quectel_rm520n_thermal 'settings'
      option serial_port '/dev/ttyUSB2'
      # No modem_type specified

  # New behavior:
  # - If modem_type not set, default to 'quectel'
  # - Existing Quectel-specific options still work
  # - No migration needed

  ---
  Testing Strategy

  1. Unit tests for each parser with mock AT responses
  2. Integration tests with real hardware (one device per manufacturer)
  3. Regression tests for existing Quectel functionality
  4. Config validation tests for declarative parser

  ---
  Documentation Requirements

  1. README section: "Multi-Modem Support"
  2. Built-in modems table: Command syntax for each
  3. Custom modem guide: How to use declarative config
  4. ucode parser guide (if implemented): API reference and examples
  5. Migration guide: For users with existing setups

  ---
  Effort Estimate

  | Phase                       | Days | Priority | Dependencies |
  |-----------------------------|------|----------|--------------|
  | Phase 1: Foundation         | 3-5  | HIGH     | None         |
  | Phase 2: Built-in Parsers   | 5-7  | HIGH     | Phase 1      |
  | Phase 3: Declarative Config | 3-4  | MEDIUM   | Phase 1      |
  | Phase 4: ucode Scripts      | 4-6  | LOW      | Phase 1,3    |
  | Documentation & Testing     | 3-4  | HIGH     | All          |

  Total (without ucode): 14-20 days
  Total (with ucode): 18-26 days

  ---
  Recommendation

  Implement in this order:
  1. ✅ Phase 1 (foundation) - Required for everything
  2. ✅ Phase 2 (built-in parsers) - Covers 90% of users
  3. ✅ Phase 3 (declarative) - Covers edge cases without code
  4. ⏸️ Phase 4 (ucode) - Only if users request it

  Phase 4 is optional because:
  - Declarative config handles most custom modems
  - ucode adds complexity
  - Can add later if demand exists