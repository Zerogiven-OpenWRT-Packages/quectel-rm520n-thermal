# Quectel RM520N Thermal Management Tools

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Quick Start](#quick-start)
- [Installation](#installation)
- [Configuration](#configuration)
  - [UCI Configuration Options](#uci-configuration-options)
- [Usage](#usage)
- [Troubleshooting](#troubleshooting)
- [Support](#support)

## Overview

This project provides a comprehensive set of tools and kernel modules for monitoring and managing the temperature of Quectel modems (primarily RM520N-GL, but compatible with other modems supporting AT+QTEMP). It is designed as an OpenWRT package and can be integrated into custom OpenWRT builds. The package includes support for dynamic Device Tree Overlays (DTO), a configurable daemon, CLI tools, and various kernel modules for thermal management.

**Note**: This package is designed to work with any modem that supports AT+QTEMP command and provides temperature data in a similar format to the Quectel RM520N. The temperature parsing prefixes are configurable via UCI to support different modem models and response formats.

## Features

- **OpenWRT Integration**: Fully compatible with OpenWRT build systems for seamless integration into custom firmware builds.
- **Dynamic Device Tree Overlays (DTO)**: Supports dynamic registration of virtual sensors via DTOs.
- **Configurable Daemon**: A userspace daemon reads modem temperature via AT commands and updates sysfs, virtual sensors, and hwmon nodes.
- **CLI Tool**: Command-line interface for manual temperature reading with JSON output support.
- **Kernel Modules**: Includes kernel modules for sysfs-based temperature reporting, hwmon integration, and virtual thermal sensors.
- **Fallback Mechanisms**: Provides fallback options when Device Tree overlay is not supported.
- **Open Source**: Licensed under the GNU General Public License for maximum flexibility.

## Components

- **Kernel Modules**: Three specialized modules for sysfs, thermal sensors, and hwmon integration
- **Userspace Tool**: Combined daemon and CLI with subcommands (`read`, `daemon`, `config`)
- **Configuration**: UCI-based configuration with automatic service reload
- **Service Management**: OpenWrt procd integration with auto-restart

## Quick Start

```bash
# Read temperature (uses daemon data if available)
quectel_rm520n_temp

# Start daemon service
/etc/init.d/quectel_rm520n_thermal start

# Check status
/etc/init.d/quectel_rm520n_thermal status
```

## Installation

### Build Environment
```bash
# Clone to OpenWrt package directory
git clone https://github.com/your-repo/quectel-rm520n-thermal.git package/quectel-rm520n-thermal

# Add to build config: Utilities → Quectel RM520N Thermal Management Tools
make menuconfig

# Build
make
```

### Existing System
```bash
make package/quectel-rm520n-thermal/compile V=s
opkg install bin/packages/.../kmod-quectel-rm520n-thermal.ipk
opkg install bin/packages/.../quectel-rm520n-thermal.ipk
```

**Note**: Service starts automatically after installation.

## Configuration

The thermal management system is configured through the UCI (Unified Configuration Interface) system. The main configuration file is located at `/etc/config/quectel_rm520n_thermal`.

### UCI Configuration Options

#### Basic Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `serial_port` | string | `/dev/ttyUSB2` | Serial port device for modem communication |
| `baud_rate` | integer | `115200` | Serial communication baud rate (9600, 19200, 38400, 57600, 115200) |
| `interval` | integer | `10` | Temperature monitoring interval in seconds |
| `enabled` | boolean | `1` | Enable/disable the thermal management service |
| `auto_start` | boolean | `1` | Automatically start service on boot |
| `debug` | boolean | `0` | Enable debug logging for troubleshooting |

#### Temperature Thresholds

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `temp_min` | integer | `-30` | Minimum temperature threshold in °C |
| `temp_max` | integer | `75` | Maximum temperature threshold in °C |
| `temp_crit` | integer | `85` | Critical temperature threshold in °C |
| `temp_default` | integer | `40` | Default temperature value in °C when reading fails |
| `error_value` | string | `N/A` | Value to display when temperature reading fails |
| `fallback_register` | boolean | `1` | Enable fallback to register-based temperature reading |

#### Temperature Parsing Prefixes

These options allow the system to work with different modem models by configuring how temperature values are parsed from AT+QTEMP responses:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `temp_modem_prefix` | string | `modem-ambient-usr` | Prefix for modem ambient temperature |
| `temp_ap_prefix` | string | `cpuss-0-usr` | Prefix for AP/CPU temperature |
| `temp_pa_prefix` | string | `modem-lte-sub6-pa1` | Prefix for power amplifier temperature |

### Example Configuration

```ini
config quectel_rm520n_thermal 'settings'
    # Basic communication settings
    option serial_port '/dev/ttyUSB3'
    option baud_rate '115200'
    option interval '10'
    
    # Service control
    option enabled '1'
    option auto_start '1'
    option debug '0'
    
    # Temperature thresholds (in °C, converted to m°C internally)
    option temp_min '-30'
    option temp_max '75'
    option temp_crit '85'
    option temp_default '40'
    option error_value 'N/A'
    option fallback_register '1'
    
    # Temperature parsing prefixes (configurable for different modem models)
    option temp_modem_prefix 'modem-ambient-usr'
    option temp_ap_prefix 'cpuss-0-usr'
    option temp_pa_prefix 'modem-lte-sub6-pa1'
```

### Configuration Management

**Important**: After modifying the configuration, you must:

1. **Commit changes**: `uci commit quectel_rm520n_thermal`
2. **Reload configuration**: `/sbin/reload_config` (triggers auto-reload)
3. **Or restart service**: `/etc/init.d/quectel_rm520n_thermal restart`

The service automatically detects configuration changes and reloads kernel module thresholds when the UCI config is updated.

### Modem Compatibility

This package works with any modem that:
- Supports AT+QTEMP command
- Returns temperature data in a format similar to Quectel RM520N
- Uses a ttyUSB serial port for communication

**Actual AT+QTEMP Response Format:**
```
AT+QTEMP
+QTEMP:"modem-lte-sub6-pa1","43"
+QTEMP:"cpuss-0-usr","43"
+QTEMP:"modem-ambient-usr","43"
OK
```

To use with different modem models, simply adjust the temperature parsing prefixes in the UCI configuration to match your modem's response format.

## Usage

### CLI Commands
```bash
# Read temperature (default mode, returns millidegrees)
quectel_rm520n_temp

# Return temperature in degrees Celsius
quectel_rm520n_temp --celsius

# JSON output
quectel_rm520n_temp --json

# JSON output in degrees Celsius
quectel_rm520n_temp --json --celsius

# Watch mode - continuously monitor temperature
quectel_rm520n_temp --watch

# Watch mode in degrees Celsius
quectel_rm520n_temp --watch --celsius

# Watch mode with JSON output
quectel_rm520n_temp --watch --json

# Debug mode
quectel_rm520n_temp --debug

# Help
quectel_rm520n_temp --help
```

### Service Management
```bash
# Start/stop service
/etc/init.d/quectel_rm520n_thermal start|stop|restart

# Check status
/etc/init.d/quectel_rm520n_thermal status

# Update config
quectel_rm520n_temp config
```

### Temperature Interfaces
- **Hwmon**: `/sys/class/hwmon/hwmonX/temp1_input` (primary)
- **Kernel**: `/sys/kernel/quectel_rm520n/temp`
- **Thermal**: `/sys/devices/virtual/thermal/thermal_zoneX/temp`

### Temperature Output Format
- **Default**: Returns temperature in millidegrees (e.g., `43000` for 43°C)
- **Celsius Option**: Use `--celsius` flag to return temperature in degrees (e.g., `43`)
- **Watch Mode**: Use `--watch` flag to continuously monitor temperature (interval configurable via UCI)
- **Consistent**: Both daemon and direct AT command modes return the same format

## Troubleshooting

### Common Issues
- **Service not starting**: Check serial port permissions and UCI config
- **Temperature showing 0**: Verify modem connection and AT command responses
- **Hwmon not found**: Ensure kernel modules are loaded
- **Wrong temperature values**: Check temperature parsing prefixes in UCI config

### Debug Mode
```bash
# Enable debug logging
uci set quectel_rm520n_thermal.settings.debug='1'
uci commit quectel_rm520n_thermal
/sbin/reload_config

# Check logs
logread | grep quectel
```

### Finding Hwmon Interface
```bash
# List all hwmon devices
ls /sys/class/hwmon/

# Find Quectel device
for hwmon in /sys/class/hwmon/*; do \
  if [ "$(cat $hwmon/name)" = "quectel_rm520n" ]; then \
    echo "Found: $hwmon"; \
  fi; \
done
```

### Different Modem Models
If using a different modem model, check the AT+QTEMP response format and adjust the temperature prefixes:

```bash
# Test AT command manually
echo "AT+QTEMP" | socat - /dev/ttyUSB3

# Update prefixes in UCI config
uci set quectel_rm520n_thermal.settings.temp_modem_prefix='your-modem-prefix'
uci set quectel_rm520n_thermal.settings.temp_ap_prefix='your-ap-prefix'
uci set quectel_rm520n_thermal.settings.temp_pa_prefix='your-pa-prefix'
uci commit quectel_rm520n_thermal
/sbin/reload_config
```

## License

This project is licensed under the GNU General Public License. See the `LICENSE` file for details.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub.

## Support

For questions or support, please open an issue on GitHub.
