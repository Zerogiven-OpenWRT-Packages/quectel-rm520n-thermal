# Quectel Modem Prometheus Collector

This Lua collector provides Prometheus metrics for Quectel RM520N modem temperature monitoring.

## Installation

1. Install the quectel-rm520n-thermal package (this collector is included)
2. Install prometheus-node-exporter-lua:
   ```bash
   opkg update
   opkg install prometheus-node-exporter-lua
   ```

3. The collector will be automatically discovered at:
   `/usr/lib/lua/prometheus-collectors/quectel_modem.lua`

4. Access metrics via the node exporter:
   ```bash
   curl http://localhost:9100/metrics | grep quectel
   ```

## Metrics Exported

- **quectel_modem_temperature_celsius**: Current modem temperature in °C
- **quectel_modem_temp_min_celsius**: Minimum temperature threshold
- **quectel_modem_temp_max_celsius**: Maximum temperature threshold
- **quectel_modem_temp_crit_celsius**: Critical temperature threshold
- **quectel_modem_updates_total**: Total number of temperature updates (from kernel)
- **quectel_modem_last_update_timestamp_seconds**: Timestamp of last update
- **quectel_daemon_running**: Daemon status (1=running, 0=stopped)

## Data Source

Metrics are read from sysfs:
- `/sys/kernel/quectel_rm520n_thermal/temp`
- `/sys/kernel/quectel_rm520n_thermal/temp_{min,max,crit}`
- `/sys/kernel/quectel_rm520n_thermal/stats`

## Example Prometheus Configuration

```yaml
scrape_configs:
  - job_name: 'openwrt'
    static_configs:
      - targets: ['router.local:9100']
        labels:
          instance: 'openwrt-router'
```

## Example Grafana Query

```promql
# Current modem temperature
quectel_modem_temperature_celsius

# Temperature approaching critical
(quectel_modem_temperature_celsius / quectel_modem_temp_crit_celsius) * 100

# Alert when temperature exceeds threshold
quectel_modem_temperature_celsius > quectel_modem_temp_crit_celsius
```
