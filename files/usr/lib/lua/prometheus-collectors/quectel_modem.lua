-- Prometheus collector for Quectel RM520N modem temperature metrics
-- This collector reads temperature data and daemon statistics from sysfs
-- and outputs metrics in Prometheus format for the node exporter

local function read_sysfs(path)
    local f = io.open(path, "r")
    if not f then
        return nil
    end
    local value = f:read("*line")
    f:close()
    return value
end

local function collect()
    local metrics = {}

    -- Read current temperature (in millidegrees)
    local temp = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp")
    if temp then
        local temp_celsius = tonumber(temp) / 1000
        table.insert(metrics, string.format(
            "# HELP quectel_modem_temperature_celsius Current modem temperature in Celsius\n" ..
            "# TYPE quectel_modem_temperature_celsius gauge\n" ..
            "quectel_modem_temperature_celsius %d", temp_celsius))
    end

    -- Read temperature thresholds
    local temp_min = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp_min")
    if temp_min then
        local temp_min_celsius = tonumber(temp_min) / 1000
        table.insert(metrics, string.format(
            "# HELP quectel_modem_temp_min_celsius Minimum temperature threshold in Celsius\n" ..
            "# TYPE quectel_modem_temp_min_celsius gauge\n" ..
            "quectel_modem_temp_min_celsius %d", temp_min_celsius))
    end

    local temp_max = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp_max")
    if temp_max then
        local temp_max_celsius = tonumber(temp_max) / 1000
        table.insert(metrics, string.format(
            "# HELP quectel_modem_temp_max_celsius Maximum temperature threshold in Celsius\n" ..
            "# TYPE quectel_modem_temp_max_celsius gauge\n" ..
            "quectel_modem_temp_max_celsius %d", temp_max_celsius))
    end

    local temp_crit = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp_crit")
    if temp_crit then
        local temp_crit_celsius = tonumber(temp_crit) / 1000
        table.insert(metrics, string.format(
            "# HELP quectel_modem_temp_crit_celsius Critical temperature threshold in Celsius\n" ..
            "# TYPE quectel_modem_temp_crit_celsius gauge\n" ..
            "quectel_modem_temp_crit_celsius %d", temp_crit_celsius))
    end

    -- Read kernel module statistics
    local stats = read_sysfs("/sys/kernel/quectel_rm520n_thermal/stats")
    if stats then
        -- Parse stats file (format: "total_updates: 123\nlast_update_time: 456789")
        local total_updates = stats:match("total_updates:%s*(%d+)")
        local last_update_time = stats:match("last_update_time:%s*(%d+)")

        if total_updates then
            table.insert(metrics, string.format(
                "# HELP quectel_modem_updates_total Total number of temperature updates\n" ..
                "# TYPE quectel_modem_updates_total counter\n" ..
                "quectel_modem_updates_total %s", total_updates))
        end

        if last_update_time then
            table.insert(metrics, string.format(
                "# HELP quectel_modem_last_update_timestamp_seconds Timestamp of last update\n" ..
                "# TYPE quectel_modem_last_update_timestamp_seconds gauge\n" ..
                "quectel_modem_last_update_timestamp_seconds %s", last_update_time))
        end
    end

    -- Check if daemon is running
    local daemon_running = 0
    local pid_file = io.open("/var/run/quectel_rm520n_temp.pid", "r")
    if pid_file then
        local pid = pid_file:read("*line")
        pid_file:close()
        -- Check if process exists
        local proc = io.open("/proc/" .. pid .. "/stat", "r")
        if proc then
            daemon_running = 1
            proc:close()
        end
    end

    table.insert(metrics, string.format(
        "# HELP quectel_daemon_running Daemon running status (1=running, 0=stopped)\n" ..
        "# TYPE quectel_daemon_running gauge\n" ..
        "quectel_daemon_running %d", daemon_running))

    return table.concat(metrics, "\n\n") .. "\n"
end

-- Return the collector function for the node exporter to call
return { scrape = collect }
