# Quectel RM520N Thermal Management Tools

## Overview

This project provides a set of tools and kernel modules for managing the temperature of the Quectel RM520N modem. It is designed as an OpenWRT package and can be integrated into custom OpenWRT builds. The package includes support for dynamic Device Tree Overlays (DTO), a configurable daemon, and various kernel modules for thermal management.

## Features

- **OpenWRT Integration**: Fully compatible with OpenWRT build systems, allowing seamless integration into custom firmware builds.
- **Dynamic Device Tree Overlay (DTO)**: Supports dynamic registration of virtual sensors via DTOs.
- **Configurable Daemon**: A userspace daemon reads modem temperature via AT commands and updates sysfs, virtual sensors, and hwmon nodes.
- **Kernel Modules**: Includes kernel modules for sysfs-based temperature reporting, hwmon integration, and virtual thermal sensors.
- **Open Source**: Licensed under the MIT License for maximum flexibility.

## Components

1. **Kernel Modules**:
   - `quectel_rm520n_temp`: Provides a sysfs interface for temperature reporting.
   - `quectel_rm520n_temp_sensor`: Registers a virtual thermal sensor via the Linux Thermal Framework.
   - `quectel_rm520n_temp_sensor_hwmon`: Integrates with the hwmon subsystem for hardware monitoring.

2. **Userspace Daemon**:
   - `quectel_rm520n_temp_daemon`: Reads temperature via AT commands and updates sysfs, hwmon, and virtual sensors.

3. **Device Tree Overlay**:
   - A DTO file (`quectel_rm520n_temp_sensor_overlay.dts`) for dynamic sensor registration.

4. **Configuration**:
   - Configurable via UCI (`/etc/config/quectel_rm520n_temp`).

## Installation

### Prerequisites

- OpenWRT build environment set up.
- Kernel headers for your target platform.

### Building the Package

1. Clone this repository into your OpenWRT package directory:

   ```bash
   git clone https://github.com/your-repo/quectel-rm520n-thermal.git package/quectel-rm520n-thermal
   ```

2. Add the package to your build configuration:

   ```bash
   make menuconfig
   ```

   Navigate to `Utilities` and select `Quectel RM520N Thermal Management Tools`.

3. Build the firmware:

   ```bash
   make
   ```

4. Flash the firmware to your device.

### Installing on an Existing System

If you already have OpenWRT installed, you can build and install the package manually:

```bash
make package/quectel-rm520n-thermal/compile V=s
opkg install bin/packages/.../quectel-rm520n-thermal.ipk
```

## Configuration

### UCI Configuration

Edit the configuration file at `/etc/config/quectel_rm520n_temp`:

```ini
config quectel_rm520n_temp 'settings'
    option serial_port '/dev/ttyUSB3'
    option interval '10'
    option baud_rate '115200'
    option temp_prefix 'modem-ambient-usr'
    option error_value 'N/A'
```

- `serial_port`: Serial port for AT communication.
- `interval`: Polling interval in seconds.
- `baud_rate`: Baud rate for the serial connection.
- `temp_prefix`: Prefix for temperature values in AT responses.
- `error_value`: Value to write in case of errors.

### Starting the Daemon

Enable and start the daemon:

```bash
/etc/init.d/quectel_rm520n_temp enable
/etc/init.d/quectel_rm520n_temp start
```

## Usage

- **Sysfs Interface**: Access temperature via `/sys/kernel/quectel_rm520n_temp/temp`.
- **Virtual Sensor**: Update and monitor temperature via `/sys/devices/platform/quectel_rm520n_temp-sensor@0/cur_temp`.
- **Hwmon Integration**: Check `/sys/class/hwmon/hwmonX/temp1_input` for temperature values.

## License

This project is licensed under the GNU General Public License v2. See the `LICENSE` file for details.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub.

## Support

For questions or support, please contact the maintainer or open an issue on GitHub.
