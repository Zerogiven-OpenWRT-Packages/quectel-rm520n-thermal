[![OpenWrt](https://img.shields.io/badge/OpenWrt-24.10.x-darkgreen.svg)](https://openwrt.org/)
[![GitHub Release](https://img.shields.io/github/v/release/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal)](https://github.com/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal/releases)
[![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal/total?color=blue)](https://github.com/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal/releases)
[![GitHub Issues or Pull Requests](https://img.shields.io/github/issues/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal)](https://github.com/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal/issues)

# Quectel RM520N Thermal Management Tools

Comprehensive tools and kernel modules for monitoring and managing Quectel modem temperature on OpenWrt.

<details>

<summary>Navigation</summary>

- [Features](#features)
- [Screenshot](#screenshot)
- [Components](#components)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Thermal Framework Integration](#thermal-framework-integration)
- [Device Tree Configuration](#device-tree-configuration)
- [Troubleshooting](#troubleshooting)

</details>

## Features

- **Linux Thermal Framework Integration**
- **Device Tree Configuration**
    - Trip points, cooling devices, and thermal policies configured via Device Tree for automatic thermal response
- **hwmon Standard Interface**
- **Cooling Device Support**
    - Can bind to cooling devices (fans, throttling) via Device Tree cooling-maps for automatic thermal control
- **Configurable Daemon**
- **CLI Tool**
- **Prometheus Metrics**
    - Lua Script support which works with or without running daemon
- **Fallback Mechanisms**
    - Works without Device Tree for basic monitoring on systems without DT support

**Note**: This package is designed to work with any modem that supports AT+QTEMP command and provides temperature data in a similar format to the Quectel RM520N. The temperature parsing prefixes are configurable via UCI to support different modem models and response formats.

## Screenshot

![Quectel RM520N Thermal Management in Action](Screenshot.png)

## Requirements

- OpenWrt 24.10
- Quectel modem with AT+QTEMP support (RM520N-GL or compatible)
- Serial port access to modem (typically /dev/ttyUSB2 or /dev/ttyUSB3)

## Installation

### From Package Feed

You can setup this package feed to install and update it with opkg:

[https://github.com/Zerogiven-OpenWRT-Packages/package-feed](https://github.com/Zerogiven-OpenWRT-Packages/package-feed)

### From IPK Package

```bash
opkg install kmod-quectel-rm520n-thermal.ipk
opkg install quectel-rm520n-thermal.ipk

# Optional: Install Prometheus metrics collector
opkg install prometheus-node-exporter-ucode-quectel-rm520n-thermal.ipk
```

### From Source

```bash
git clone https://github.com/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal.git package/quectel-rm520n-thermal
make menuconfig  # Navigate to: Utilities → Quectel RM520N Thermal Management Tools
make package/quectel-rm520n-thermal/compile V=s
```

**Note**: Service starts automatically after installation.

## Usage

<details>

<summary>Quick Start</summary>

```bash
# Read temperature (uses daemon data if available)
quectel_rm520n_temp

# Start daemon service
/etc/init.d/quectel_rm520n_thermal start

# Check daemon and service status
quectel_rm520n_temp status
/etc/init.d/quectel_rm520n_thermal status
```

</details>

<details>

<summary>CLI Commands</summary>

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

# Watch mode with JSON output
quectel_rm520n_temp --watch --json

# Debug mode
quectel_rm520n_temp --debug

# Help
quectel_rm520n_temp --help
```

</details>

<details>

<summary>Temperature Interfaces</summary>

- **Hwmon**: `/sys/class/hwmon/hwmonX/temp1_input` (primary)
- **Kernel**: `/sys/kernel/quectel_rm520n_thermal/temp`
- **Thermal**: `/sys/devices/virtual/thermal/thermal_zoneX/temp`

</details>

## Configuration

The thermal management system is configured through the UCI system. The configuration file is located at `/etc/config/quectel_rm520n_thermal`.

### Basic Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `serial_port` | string | `/dev/ttyUSB2` | Serial port device for modem communication |
| `baud_rate` | integer | `115200` | Serial communication baud rate (9600, 19200, 38400, 57600, 115200) |
| `interval` | integer | `10` | Temperature monitoring interval in seconds |
| `enabled` | boolean | `1` | Enable/disable the thermal management service |
| `auto_start` | boolean | `1` | Automatically start service on boot |
| `log_level` | string | `info` | Logging level: `debug`, `info`, `warning`, or `error` |

### Temperature Thresholds

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `temp_min` | integer | `-30` | Minimum temperature threshold in °C |
| `temp_max` | integer | `75` | Maximum temperature threshold in °C |
| `temp_crit` | integer | `85` | Critical temperature threshold in °C |
| `temp_default` | integer | `40` | Default temperature value in °C when reading fails |
| `error_value` | string | `N/A` | Value to display when temperature reading fails |
| `fallback_register` | boolean | `1` | Enable fallback to register-based temperature reading |

### Temperature Parsing Prefixes

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
    option log_level 'info'

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

### Modem Compatibility

This package works with any modem that:

- Supports AT+QTEMP command
- Returns temperature data in a format similar to Quectel RM520N
- Uses a ttyUSB serial port for communication

## Device Tree Configuration

For full thermal framework integration with automatic trip point handling and cooling device support, configure the thermal zone in your Device Tree.

<details>

<summary>Basic Thermal Zone (Monitoring Only)</summary>

```dts
/ {
    thermal-zones {
        modem_thermal: modem-thermal {
            polling-delay-passive = <5000>;   /* Poll every 5s when trip point active */
            polling-delay = <10000>;          /* Poll every 10s normally */
            thermal-sensors = <&quectel_temp_sensor>;

            trips {
                modem_crit: crit {
                    temperature = <85000>;    /* 85°C in millidegrees */
                    hysteresis = <5000>;      /* 5°C hysteresis */
                    type = "critical";        /* Emergency shutdown */
                };

                modem_hot: hot {
                    temperature = <80000>;    /* 80°C */
                    hysteresis = <5000>;
                    type = "hot";             /* High temperature warning */
                };

                modem_active_high: active-high {
                    temperature = <75000>;    /* 75°C */
                    hysteresis = <5000>;
                    type = "active";          /* Active cooling level 3 */
                };

                modem_active_med: active-med {
                    temperature = <70000>;    /* 70°C */
                    hysteresis = <5000>;
                    type = "active";          /* Active cooling level 2 */
                };

                modem_active_low: active-low {
                    temperature = <65000>;    /* 65°C */
                    hysteresis = <5000>;
                    type = "active";          /* Active cooling level 1 */
                };
            };
        };
    };
};
```

</details>

<details>

<summary>With Cooling Device (Fan Control)</summary>

To bind trip points to a cooling device (e.g., PWM fan), add cooling-maps:

```dts
/ {
    thermal-zones {
        modem_thermal: modem-thermal {
            polling-delay-passive = <5000>;
            polling-delay = <10000>;
            thermal-sensors = <&quectel_temp_sensor>;

            trips {
                modem_crit: crit {
                    temperature = <85000>;
                    hysteresis = <5000>;
                    type = "critical";
                };

                modem_active_high: active-high {
                    temperature = <75000>;
                    hysteresis = <5000>;
                    type = "active";
                };

                modem_active_med: active-med {
                    temperature = <70000>;
                    hysteresis = <5000>;
                    type = "active";
                };

                modem_active_low: active-low {
                    temperature = <65000>;
                    hysteresis = <5000>;
                    type = "active";
                };
            };

            cooling-maps {
                /* Map active-low trip (65°C) to fan speed 1 */
                map0 {
                    trip = <&modem_active_low>;
                    cooling-device = <&fan 1 1>;  /* Set fan to speed level 1 */
                };

                /* Map active-med trip (70°C) to fan speed 2 */
                map1 {
                    trip = <&modem_active_med>;
                    cooling-device = <&fan 2 2>;  /* Set fan to speed level 2 */
                };

                /* Map active-high trip (75°C) to fan speed 3 (max) */
                map2 {
                    trip = <&modem_active_high>;
                    cooling-device = <&fan 3 3>;  /* Set fan to maximum speed */
                };
            };
        };
    };
};
```

**Note**: Replace `<&fan>` with your actual cooling device phandle. Common cooling devices: PWM fans (`<&pwm_fan>`), GPIO fans (`<&gpio_fan>`), CPU frequency scaling (`<&cpu0>` for passive cooling/throttling).

</details>

See `quectel_rm520n_thermal_overlay.dts.example` in the project root for a complete Device Tree overlay example.

## Troubleshooting

### Common Issues

| Issue | Solution |
|-------|----------|
| Service not starting | Check serial port permissions and UCI config |
| Temperature showing 0 | Verify modem connection and AT command responses |
| Hwmon not found | Ensure kernel modules are loaded |
| Wrong temperature values | Check temperature parsing prefixes in UCI config |

<details>

<summary>Debug Mode</summary>

```bash
# Enable debug logging
uci set quectel_rm520n_thermal.settings.log_level='debug'
uci commit quectel_rm520n_thermal
/sbin/reload_config

# Check logs
logread | grep quectel

# Disable debug logging (return to info level)
uci set quectel_rm520n_thermal.settings.log_level='info'
uci commit quectel_rm520n_thermal
/sbin/reload_config
```

</details>

<details>

<summary>Finding Hwmon Interface</summary>

```bash
# List all hwmon devices
ls /sys/class/hwmon/

# Find Quectel device
for hwmon in /sys/class/hwmon/*; do \
  if [ "$(cat $hwmon/name)" = "quectel_rm520n_thermal" ]; then \
    echo "Found: $hwmon"; \
  fi; \
done
```

</details>

<details>

<summary>Different Modem Models</summary>

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

</details>
