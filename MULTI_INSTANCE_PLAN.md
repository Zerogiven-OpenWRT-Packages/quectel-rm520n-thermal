# Multi-Instance Support Implementation Plan

## Finalized Decisions

| Decision | Choice |
|----------|--------|
| Max instances | 8 |
| Instance naming | Auto-generated if not set (`modem0`, `modem1`, ...) |
| Default instance | 0 (when not specified) |
| Kernel module | Single module with internal instance array |
| Config compatibility | Legacy (`config settings`) + new (`config modem`) coexist |
| Instance assignment | Legacy=0, new modem sections=1,2,... (or 0,1,2 if no legacy) |
| Daemon architecture | One process per instance (Option A) |

---

## Scope

Add multi-instance support to existing `quectel-rm520n-thermal` project. No rename.

---

## Components to Modify

### 1. Kernel Module (`src/kmod/`)

**Single module with instance array:**

```
/sys/kernel/quectel_rm520n_thermal/
├── modem0/
│   ├── temp
│   ├── temp_min
│   ├── temp_max
│   └── temp_crit
├── modem1/
│   └── ...
└── modem7/
```

- `main.c`: Parent kobject + instance array (max 8)
- `hwmon.c`: Per-instance hwmon device (`quectel_rm520n_thermal0`, ...)
- `sensor.c`: Per-instance thermal zone

### 2. Daemon (`src/`)

**New parameter:** `--instance N`

```bash
quectel_rm520n_temp daemon --instance 0
quectel_rm520n_temp daemon --instance 1
quectel_rm520n_temp daemon              # defaults to instance 0
```

- `main.c`: Parse `--instance N` parameter
- `daemon.c`: Instance-specific sysfs paths, PID files
- `uci_config.c`: `config_read_uci_by_instance(N)` - handles both formats

### 3. UCI Config

**Legacy format (still works):**
```
config settings 'settings'
    option serial_port '/dev/ttyUSB2'
    option interval '30'
```

**New format:**
```
config modem 'primary'
    option enabled '1'
    option name 'Primary Modem'
    option serial_port '/dev/ttyUSB2'
    option interval '30'

config modem 'backup'
    option enabled '1'
    option name 'Backup Modem'
    option serial_port '/dev/ttyUSB5'
```

**Instance assignment:**
- Legacy `settings` exists → instance 0
- New `modem` sections → instance 1, 2, ... (or 0, 1, 2 if no legacy)

### 4. Init Script (`files/etc/init.d/`)

- Count enabled configs (legacy + new modem sections)
- Start daemon per instance: `daemon --instance N`
- procd handles per-instance restart

### 5. CLI (`src/cli.c`)

- `--instance N`: Read specific instance
- `--all`: Read all configured instances

### 6. Prometheus Collector (`files/extra/quectel_modem.uc`)

- Discover all configured instances from UCI
- Export metrics with labels: `instance`, `name`

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/kmod/main.c` | Instance array, parent kobject |
| `src/kmod/hwmon.c` | Per-instance hwmon registration |
| `src/kmod/sensor.c` | Per-instance thermal zone |
| `src/main.c` | `--instance N` parameter |
| `src/daemon.c` | Instance-specific paths |
| `src/uci_config.c` | `config_read_uci_by_instance()` |
| `src/cli.c` | `--instance`, `--all` flags |
| `files/etc/init.d/*.init` | Multi-instance startup |
| `files/extra/quectel_modem.uc` | Multi-instance metrics |

---

## Testing Scenarios

| Scenario | Config | Expected |
|----------|--------|----------|
| Legacy only | `config settings` | Instance 0, works as before |
| New single | `config modem 'x'` | Instance 0 |
| New multiple | 2x `config modem` | Instance 0, 1 |
| Mixed | Legacy + modem | Legacy=0, modem=1 |
| Disabled | `option enabled '0'` | Skipped |

---

## Estimated Effort

4-6 hours for Phase 1 (multi-instance support)
