  Architecture Decision: Multiple Daemon Instances vs Single Daemon

  I recommend Option A: Multiple Daemon Instances (one per modem) because:
  - Cleaner code changes
  - Isolation - one modem failure doesn't affect others
  - Can restart individual modem monitoring
  - Fits OpenWRT's lightweight process model

  Detailed Implementation Plan

  1. Kernel Module Changes

  Current:
  /sys/kernel/quectel_rm520n_thermal/temp
  /sys/kernel/quectel_rm520n_thermal/temp_min
  ...

  New:
  /sys/kernel/quectel_rm520n_thermal0/temp
  /sys/kernel/quectel_rm520n_thermal1/temp
  /sys/kernel/quectel_rm520n_thermal2/temp
  ...

  Changes needed:
  - Add module parameter instance=N (default 0 for backward compatibility)
  - Update kobject_create_and_add() to use instance-specific name
  - hwmon device names: quectel_rm520n_thermal0, quectel_rm520n_thermal1, etc.
  - Each instance is a separate module load: insmod quectel_rm520n_temp.ko instance=0

  Files to modify:
  - src/kmod/main.c - add instance parameter, modify sysfs path creation
  - src/kmod/hwmon.c - use instance-specific device name
  - src/kmod/sensor.c - use instance-specific thermal zone name

  ---
  2. UCI Configuration Changes

  Current:
  config settings 'config'
      option serial_port '/dev/ttyUSB2'
      option interval '30'
      option baud_rate '115200'
      ...

  New:
  config modem 'modem0'
      option enabled '1'
      option label 'Primary'
      option serial_port '/dev/ttyUSB2'
      option interval '30'
      option baud_rate '115200'
      ...

  config modem 'modem1'
      option enabled '1'
      option label 'Secondary'
      option serial_port '/dev/ttyUSB5'
      option interval '30'
      option baud_rate '115200'
      ...

  config modem 'modem2'
      option enabled '0'
      option label 'Tertiary'
      option serial_port '/dev/ttyUSB8'
      option interval '60'
      option baud_rate '115200'
      ...

  Files to modify:
  - files/quectel_rm520n_thermal - UCI config template
  - src/uci_config.c - read specific modem section instead of 'config' section

  ---
  3. Daemon Changes

  Add instance parameter:
  quectel_rm520n_temp daemon --instance modem0
  quectel_rm520n_temp daemon --instance modem1

  Changes needed:
  - Add --instance <name> command line parameter
  - Read UCI config for that specific modem section
  - Use instance-specific sysfs paths: /sys/kernel/quectel_rm520n_thermal<N>/
  - Use instance-specific PID file: /var/run/quectel_rm520n_temp_<instance>.pid
  - Update logging to include instance name

  Files to modify:
  - src/include/common.h - add instance name to global context
  - src/main.c - parse --instance parameter
  - src/daemon.c - use instance-specific paths and PID file
  - src/uci_config.c - read modem-specific section
  - src/logging.c - prefix logs with instance name

  ---
  4. CLI Tool Changes

  Current:
  quectel_rm520n_temp read
  quectel_rm520n_temp status

  New:
  quectel_rm520n_temp read --instance modem0
  quectel_rm520n_temp read --instance modem1
  quectel_rm520n_temp read --all        # Read all enabled modems
  quectel_rm520n_temp status --all      # Status of all instances

  Files to modify:
  - src/cli.c - add instance parameter support
  - Add --all flag to iterate through all UCI modem sections

  ---
  5. Init Script Changes

  Current:
  start_service() {
      procd_open_instance
      procd_set_param command /usr/bin/quectel_rm520n_temp daemon
      ...
  }

  New:
  start_service() {
      # Iterate through UCI modem sections
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

      procd_open_instance "$cfg"
      procd_set_param command /usr/bin/quectel_rm520n_temp daemon --instance "$cfg"
      procd_set_param respawn 3600 5 5
      procd_set_param stdout 1
      procd_set_param stderr 1
      procd_close_instance
  }

  Files to modify:
  - files/quectel_rm520n_thermal.init - complete rewrite of start/stop logic

  ---
  6. Prometheus Collector Changes

  Current metrics:
  quectel_modem_temperature_celsius 45.0

  New metrics with labels:
  quectel_modem_temperature_celsius{instance="modem0",label="Primary"} 45.0
  quectel_modem_temperature_celsius{instance="modem1",label="Secondary"} 42.0
  quectel_modem_temperature_celsius{instance="modem2",label="Tertiary"} NaN

  Changes needed:
  - Discover all modem instances from UCI config
  - Iterate through enabled modems
  - Add instance and label labels to all metrics
  - Skip disabled modems or return NaN for offline modems

  Files to modify:
  - files/quectel_rm520n_thermal.uc - add UCI iteration and metric labels

  ---
  7. Kernel Module Loading

  Changes needed:
  - Load multiple instances of kernel modules
  - Either at boot or dynamically when modem detected

  Option A - Static loading:
  # In init script or hotplug
  insmod quectel_rm520n_temp.ko instance=0
  insmod quectel_rm520n_temp.ko instance=1
  insmod quectel_rm520n_temp.ko instance=2

  Option B - Hotplug based:
  - Detect modem insertion via hotplug
  - Load kernel module for that instance
  - More complex but cleaner

  ---
  Performance & Resource Impact

  Memory Usage (per additional modem)

  | Component                 | Size      | Notes                        |
  |---------------------------|-----------|------------------------------|
  | Daemon process (TEXT)     | ~50-100KB | Shared across all instances  |
  | Daemon process (DATA/BSS) | ~20KB     | Per instance                 |
  | Daemon process (Stack)    | ~8MB      | Per instance (mostly unused) |
  | Daemon process (Heap)     | ~10-20KB  | Per instance                 |
  | Kernel module (TEXT)      | ~30-40KB  | Shared if same module        |
  | Kernel module (DATA)      | ~5-10KB   | Per instance                 |
  | sysfs entries             | ~2KB      | Per instance                 |
  | hwmon device              | ~1KB      | Per instance                 |

  Total per additional modem: ~8-10MB RSS (mostly stack), ~50KB actual usage

  For 3 modems: ~24-30MB total additional RAM

  CPU Usage

  - Each daemon wakes every 30-60 seconds
  - Serial read takes ~100-500ms
  - CPU time: <1% per daemon (negligible)
  - 3 modems: still <3% CPU usage total

  Impact on 256MB RAM Router

  - Base OpenWRT: ~60-80MB
  - Your current system: probably ~100-120MB
  - With 3 modems: +24-30MB = ~124-150MB
  - Still plenty of headroom

  Disk Space

  - No significant increase (code shared)
  - UCI config: +200 bytes per modem
  - Additional log entries

  ---
  Backward Compatibility Strategy

  Option 1: Auto-migrate
  - If old config format detected, create config modem 'modem0' with existing settings
  - Transparent upgrade

  Option 2: Keep both formats
  - Support both config settings 'config' (single modem) and config modem 'modemX' (multi-modem)
  - More complex but allows gradual migration

  Recommendation: Option 1 with auto-migration

  ---
  Development Phases

  Phase 1: Kernel Module Multi-Instance Support

  - Add instance parameter
  - Test with manual module loading
  - Verify multiple sysfs paths work

  Phase 2: UCI Config & Daemon

  - New UCI format
  - Daemon instance parameter
  - Migration script

  Phase 3: Init Script

  - Multi-instance startup
  - Handle enable/disable per modem

  Phase 4: CLI Tool

  - Instance parameter
  - --all support

  Phase 5: Prometheus Integration

  - Metric labels
  - Multi-instance discovery

  Phase 6: Testing & Polish

  - Test with 1, 2, and 3 modems
  - Error handling
  - Documentation

  ---
  Estimated Effort

  - Phase 1: 4-6 hours (kernel changes, testing)
  - Phase 2: 3-4 hours (daemon changes)
  - Phase 3: 2-3 hours (init script)
  - Phase 4: 2-3 hours (CLI updates)
  - Phase 5: 2-3 hours (Prometheus)
  - Phase 6: 4-6 hours (testing, docs)

  Total: ~17-25 hours of development

  ---
  Questions to Consider

  1. Kernel module instance limit: Hard-code max 8 instances or make it configurable?
  2. Hotplug support: Auto-detect modem insertion and start monitoring?
  3. Naming: Use modem0, modem1 or allow custom names like primary, backup, failover?
  4. CLI default: If no --instance specified, should it error or show all?
  5. Migration: Automatic or require manual config update?

  My recommendations:
  1. Max 8 instances (more than enough)
  2. Skip hotplug for first version (add later if needed)
  3. Use modem0, modem1 as section names, but allow label option for friendly names
  4. CLI --all by default if multiple modems configured
  5. Automatic migration on first boot after upgrade
  