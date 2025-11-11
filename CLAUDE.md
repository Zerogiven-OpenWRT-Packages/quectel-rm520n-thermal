# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Quectel RM520N Thermal Management Tools is an OpenWRT package providing comprehensive temperature monitoring for Quectel modems (primarily RM520N-GL). The package consists of kernel modules, a userspace daemon/CLI tool, and UCI-based configuration. It supports dynamic Device Tree Overlays (DTO) and provides multiple temperature interfaces (sysfs, hwmon, thermal zones).

## Build Commands

### OpenWRT Build Environment
```bash
# Build kernel module package
make package/quectel-rm520n-thermal/compile V=s

# Build with verbose output for debugging
make package/quectel-rm520n-thermal/compile V=sc

# Clean build artifacts
make package/quectel-rm520n-thermal/clean

# Install to running OpenWRT system
opkg install bin/packages/.../kmod-quectel-rm520n-thermal.ipk
opkg install bin/packages/.../quectel-rm520n-thermal.ipk
```

### Local Development Build

For development and testing outside of OpenWRT build environment:

#### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install build-essential libuci-dev libsysfs-dev

# Alpine Linux
apk add build-base uci-dev sysfs-utils-dev

# Note: Kernel module compilation requires kernel headers matching target system
```

#### Building Userspace Tool
```bash
cd src
make

# The binary will be at: src/quectel_rm520n_temp
# Test locally: ./quectel_rm520n_temp --help
```

#### Building Kernel Modules (Advanced)
```bash
# Requires kernel headers for target system
cd src
make modules

# Modules will be at:
# - src/kmod/quectel_rm520n_temp.ko
# - src/kmod/quectel_rm520n_temp_sensor.ko
# - src/kmod/quectel_rm520n_temp_sensor_hwmon.ko
```

#### Cross-Compilation for OpenWRT
```bash
# Set up OpenWRT toolchain
export PATH=/path/to/openwrt/staging_dir/toolchain-*/bin:$PATH
export STAGING_DIR=/path/to/openwrt/staging_dir

# Cross-compile userspace tool
cd src
make CC=aarch64-openwrt-linux-gcc

# Transfer to target device
scp quectel_rm520n_temp root@router:/usr/bin/
```

#### Development Testing
```bash
# Test CLI without daemon
./quectel_rm520n_temp --port /dev/ttyUSB2 --debug

# Test daemon mode (requires root for syslog)
sudo ./quectel_rm520n_temp daemon --debug

# Clean build artifacts
make clean
```

#### Quick Development Cycle
```bash
# Edit code, rebuild, and test
make clean && make && ./quectel_rm520n_temp --version
```

## Architecture Overview

### Package Structure
The project uses a dual-package architecture:
1. **kmod-quectel-rm520n-thermal**: Three kernel modules providing kernel-space temperature interfaces
2. **quectel-rm520n-thermal**: Userspace daemon/CLI tool with UCI configuration

### Kernel Modules (src/kmod/)
Three independent kernel modules that work together:

- **main.c** (`quectel_rm520n_temp.ko`): Core sysfs interface at `/sys/kernel/quectel_rm520n/temp`. Stores temperature thresholds (temp_min, temp_max, temp_crit, temp_default) and current temperature value. Provides read/write access for userspace daemon.

- **sensor.c** (`quectel_rm520n_temp_sensor.ko`): Virtual thermal sensor registration. Creates `/sys/devices/virtual/thermal/thermal_zoneX/temp`. Depends on main.c for temperature data.

- **hwmon.c** (`quectel_rm520n_temp_sensor_hwmon.ko`): Hardware monitoring (hwmon) interface. Creates `/sys/class/hwmon/hwmonX/temp1_input`. Primary interface for system monitoring tools. Depends on main.c.

**Critical**: All three modules must be loaded in order (main → sensor → hwmon) as later modules depend on symbols exported by main.c.

### Userspace Tool Architecture (src/)
Single binary (`quectel_rm520n_temp`) with multiple modes controlled by subcommands:

- **main.c**: Entry point, signal handling, subcommand routing
- **daemon.c**: Continuous monitoring mode - reads AT+QTEMP via serial, writes to kernel interfaces
- **cli.c**: One-shot temperature reading mode with JSON/watch support
- **config.c**: Configuration management, default values
- **uci_config.c**: UCI integration, reads `/etc/config/quectel_rm520n_thermal`
- **serial.c**: Serial port communication, AT command handling
- **temperature.c**: Temperature parsing from AT+QTEMP responses
- **system.c**: Kernel interface interactions (sysfs/hwmon writes)
- **logging.c**: Logging abstraction (syslog for daemon, stdout for CLI)
- **ui.c**: Help, version, and usage display

### Communication Flow
```
Modem (AT+QTEMP) → Serial Port (/dev/ttyUSB2)
                 ↓
         Userspace Daemon (daemon.c)
                 ↓
         Parse Temperature (temperature.c)
                 ↓
         Write to Kernel (system.c)
                 ↓
    /sys/kernel/quectel_rm520n/temp (main.c)
                 ↓
         ┌────────┴────────┐
         ↓                 ↓
    Thermal Zone     Hwmon Interface
    (sensor.c)       (hwmon.c)
```

### Temperature Handling
- All temperatures stored in **millidegrees Celsius (m°C)** internally
- AT+QTEMP returns degrees (e.g., "43")
- Converted to millidegrees (43000) before storage
- CLI supports `--celsius` flag to return degrees instead
- Default thresholds: min=-30°C, max=75°C, crit=85°C, default=40°C

### Configuration System
UCI configuration at `/etc/config/quectel_rm520n_thermal` with auto-reload support:
- **Service triggers**: OpenWrt procd monitors config changes and auto-reloads daemon
- **Threshold updates**: Config changes trigger kernel module threshold updates via `quectel_rm520n_temp config`
- **Manual reload**: After `uci commit`, run `/sbin/reload_config` to trigger auto-reload

Important UCI options:
- `serial_port`: Modem serial device (default: /dev/ttyUSB2)
- `interval`: Monitoring interval in seconds (default: 10)
- `temp_*_prefix`: Parsing prefixes for different modem models (supports any AT+QTEMP compatible modem)
- `fallback_register`: Auto-load kernel modules (default: 1)

### Modem Compatibility
Designed for Quectel RM520N but works with any modem supporting AT+QTEMP. The temperature parsing prefixes are UCI-configurable to support different response formats:
- `temp_modem_prefix`: Modem ambient temperature (default: "modem-ambient-usr")
- `temp_ap_prefix`: AP/CPU temperature (default: "cpuss-0-usr")
- `temp_pa_prefix`: Power amplifier temperature (default: "modem-lte-sub6-pa1")

## Development Guidelines

### Building for OpenWRT
- Main Makefile (root) is OpenWRT package definition using `include $(INCLUDE_DIR)/package.mk`
- Kernel modules built using `$(KERNEL_MAKE_FLAGS)` with Kbuild
- Userspace tool built using `$(TARGET_CC)` with cross-compilation support
- Package metadata (PKG_NAME, PKG_VERSION, etc.) passed as preprocessor defines

### Adding New Temperature Sources
1. Modify `temperature.c`: Add new parsing function for additional AT response prefixes
2. Update `daemon.c`: Add new temperature write to kernel interface
3. Update UCI schema in `files/etc/config/quectel_rm520n_thermal`: Add new prefix option
4. Update `uci_config.c`: Load new configuration option

### Kernel Module Development
- All modules include `common.h` for shared definitions and fallbacks
- Temperature thresholds stored in main.c and exported via EXPORT_SYMBOL_GPL
- Sensor and hwmon modules access shared temperature via external references
- Use `pr_info/pr_err` for kernel logging

### UCI Configuration Changes
When modifying configuration schema:
1. Update `files/etc/config/quectel_rm520n_thermal` (default config)
2. Update `uci_config.c` load_uci_config() function
3. Update `config.h` config_t structure
4. Update init script if service behavior changes

### Common Development Pitfalls
- **Module loading order**: Always load main.c module first (kernel dependency)
- **Temperature units**: Internal storage is always millidegrees, convert at boundaries
- **Serial port permissions**: Daemon needs read/write access to serial device
- **UCI commit**: Must call `/sbin/reload_config` after `uci commit` for auto-reload to work
- **PID file cleanup**: Daemon uses `/var/run/quectel_rm520n_temp.pid` for locking

### Testing Temperature Reading
```bash
# Test AT command manually
echo "AT+QTEMP" | socat - /dev/ttyUSB2

# Read temperature (one-shot)
quectel_rm520n_temp

# Read with JSON output
quectel_rm520n_temp --json

# Continuous monitoring (watch mode)
quectel_rm520n_temp --watch

# Check daemon status
/etc/init.d/quectel_rm520n_thermal status

# View daemon logs
logread | grep quectel

# Enable debug logging
uci set quectel_rm520n_thermal.settings.debug='1'
uci commit quectel_rm520n_thermal
/sbin/reload_config
```

### Finding Hwmon Interface
```bash
# Find Quectel hwmon device
for hwmon in /sys/class/hwmon/*; do
  if [ "$(cat $hwmon/name 2>/dev/null)" = "quectel_rm520n" ]; then
    echo "Found: $hwmon"
    cat $hwmon/temp1_input
  fi
done
```
