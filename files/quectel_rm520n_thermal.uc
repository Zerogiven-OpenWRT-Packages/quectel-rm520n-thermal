// Prometheus ucode collector for Quectel RM520N modem temperature
// Reads temperature data and statistics from sysfs

import { readfile } from "fs";

function read_sysfs(path) {
	try {
		let content = readfile(path);
		if (content != null) {
			return trim(content);
		}
	} catch (e) {
		// File doesn't exist or can't be read
	}
	return null;
}

function parse_stats(stats_content) {
	let stats = {};
	if (!stats_content)
		return stats;

	let lines = split(stats_content, "\n");
	for (let line in lines) {
		let match_result = match(line, /^([^:]+):\s*(\d+)/);
		if (match_result && length(match_result) >= 3) {
			stats[match_result[1]] = int(match_result[2]);
		}
	}
	return stats;
}

// Check if sysfs interface exists
let temp = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp");
if (temp == null) {
	return false;
}

// Create gauge metrics
let m_temp = gauge("quectel_modem_temperature_celsius"); // ucode-lsp disable
let m_temp_min = gauge("quectel_modem_temp_min_celsius"); // ucode-lsp disable
let m_temp_max = gauge("quectel_modem_temp_max_celsius"); // ucode-lsp disable
let m_temp_crit = gauge("quectel_modem_temp_crit_celsius"); // ucode-lsp disable
let m_updates = gauge("quectel_modem_updates_total"); // ucode-lsp disable
let m_last_update = gauge("quectel_modem_last_update_timestamp_seconds"); // ucode-lsp disable
let m_daemon = gauge("quectel_daemon_running"); // ucode-lsp disable

// Current temperature (convert from millidegrees to degrees)
if (temp) {
	m_temp({}, int(temp) / 1000);
}

// Temperature thresholds
let temp_min = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp_min");
if (temp_min) {
	m_temp_min({}, int(temp_min) / 1000);
}

let temp_max = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp_max");
if (temp_max) {
	m_temp_max({}, int(temp_max) / 1000);
}

let temp_crit = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp_crit");
if (temp_crit) {
	m_temp_crit({}, int(temp_crit) / 1000);
}

// Kernel module statistics
let stats_content = read_sysfs("/sys/kernel/quectel_rm520n_thermal/stats");
if (stats_content) {
	let stats = parse_stats(stats_content);

	if (stats.total_updates != null) {
		m_updates({}, stats.total_updates);
	}

	if (stats.last_update_time != null) {
		m_last_update({}, stats.last_update_time);
	}
}

// Check daemon status
let pid = read_sysfs("/var/run/quectel_rm520n_temp.pid");
let daemon_running = 0;
if (pid) {
	let stat = read_sysfs(`/proc/${pid}/stat`);
	daemon_running = stat ? 1 : 0;
}
m_daemon({}, daemon_running);
