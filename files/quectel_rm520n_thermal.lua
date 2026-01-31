-- Quectel RM520N modem temperature collector
-- For prometheus-node-exporter-lua
--
-- Reads temperature from kernel sysfs interface when daemon is running,
-- or falls back to CLI tool for direct modem query.

local fs = require "nixio.fs"
local cjson = require "cjson"

local SYSFS_BASE = "/sys/kernel/quectel_rm520n_thermal"
local CLI_PATH = "/usr/bin/quectel_rm520n_temp"
local PID_FILE = "/var/run/quectel_rm520n_temp.pid"

-- Helper: trim whitespace
local function trim(s)
    if not s then return nil end
    return string.gsub(s, "^%s*(.-)%s*$", "%1")
end

-- Helper: read file contents
local function read_file(path)
    local content = get_contents(path)
    if content then
        return trim(content)
    end
    return nil
end

-- Helper: check if daemon is running
local function is_daemon_running()
    local pid = read_file(PID_FILE)
    if pid then
        local stat = read_file("/proc/" .. pid .. "/stat")
        return stat ~= nil
    end
    return false
end

-- Helper: read temperature from CLI (fallback)
local function read_from_cli()
    if not fs.access(CLI_PATH, "x") then
        return nil
    end

    local handle = io.popen(CLI_PATH .. " read --json --celsius 2>/dev/null")
    if not handle then
        return nil
    end

    local output = handle:read("*a")
    handle:close()

    if not output or output == "" then
        return nil
    end

    local ok, data = pcall(cjson.decode, output)
    if ok and data and data.status == "ok" then
        return tonumber(data.temperature)
    end

    return nil
end

-- Main scrape function
local function scrape()
    local temp = nil
    local source = nil
    local daemon_running = is_daemon_running()

    -- Try sysfs first (daemon data)
    local sysfs_temp = read_file(SYSFS_BASE .. "/temp")
    if sysfs_temp then
        temp = tonumber(sysfs_temp) / 1000  -- Convert millidegrees to degrees
        source = "sysfs"
    elseif not daemon_running then
        -- Fallback to CLI if daemon not running
        temp = read_from_cli()
        if temp then
            source = "cli"
        end
    end

    -- Export temperature if available
    if temp then
        metric("quectel_modem_temperature_celsius", "gauge", nil, temp)
        metric("quectel_modem_source", "gauge", {source=source}, 1)
    end

    -- Export thresholds (sysfs only)
    local temp_min = read_file(SYSFS_BASE .. "/temp_min")
    if temp_min then
        metric("quectel_modem_temp_min_celsius", "gauge", nil,
            tonumber(temp_min) / 1000)
    end

    local temp_max = read_file(SYSFS_BASE .. "/temp_max")
    if temp_max then
        metric("quectel_modem_temp_max_celsius", "gauge", nil,
            tonumber(temp_max) / 1000)
    end

    local temp_crit = read_file(SYSFS_BASE .. "/temp_crit")
    if temp_crit then
        metric("quectel_modem_temp_crit_celsius", "gauge", nil,
            tonumber(temp_crit) / 1000)
    end

    -- Export statistics (sysfs only)
    local stats = read_file(SYSFS_BASE .. "/stats")
    if stats then
        local updates = string.match(stats, "total_updates:%s*(%d+)")
        if updates then
            metric("quectel_modem_updates_total", "counter", nil,
                tonumber(updates))
        end

        local last_update = string.match(stats, "last_update_time:%s*(%d+)")
        if last_update then
            metric("quectel_modem_last_update_timestamp_seconds", "gauge", nil,
                tonumber(last_update))
        end
    end

    -- Export daemon status
    metric("quectel_modem_daemon_running", "gauge", nil,
        daemon_running and 1 or 0)
end

return { scrape = scrape }
