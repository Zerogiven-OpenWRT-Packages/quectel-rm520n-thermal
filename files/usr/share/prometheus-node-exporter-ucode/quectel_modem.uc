// Prometheus ucode collector for Quectel RM520N modem temperature
// Reads temperature data and statistics from sysfs and outputs Prometheus metrics

import { readfile } from "fs";

function read_sysfs(path) {
	try {
		let content = readfile(path);
		if (content)
			return trim(content);
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
		let match = match(line, /^([^:]+):\s*(\d+)/);
		if (match && length(match) >= 3) {
			stats[match[1]] = int(match[2]);
		}
	}
	return stats;
}

function check_daemon_running() {
	// Check if daemon is running via PID file
	let pid = read_sysfs("/var/run/quectel_rm520n_temp.pid");
	if (!pid)
		return 0;

	// Verify process exists
	let stat = read_sysfs(`/proc/${pid}/stat`);
	return stat ? 1 : 0;
}

// Main collector function
function collect() {
	let metrics = [];

	// Read current temperature (in millidegrees Celsius)
	let temp = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp");
	if (temp) {
		let temp_celsius = int(temp) / 1000;
		push(metrics,
			"# HELP quectel_modem_temperature_celsius Current modem temperature in Celsius",
			"# TYPE quectel_modem_temperature_celsius gauge",
			`quectel_modem_temperature_celsius ${temp_celsius}`
		);
	}

	// Read temperature thresholds
	let temp_min = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp_min");
	if (temp_min) {
		let temp_min_celsius = int(temp_min) / 1000;
		push(metrics,
			"# HELP quectel_modem_temp_min_celsius Minimum temperature threshold in Celsius",
			"# TYPE quectel_modem_temp_min_celsius gauge",
			`quectel_modem_temp_min_celsius ${temp_min_celsius}`
		);
	}

	let temp_max = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp_max");
	if (temp_max) {
		let temp_max_celsius = int(temp_max) / 1000;
		push(metrics,
			"# HELP quectel_modem_temp_max_celsius Maximum temperature threshold in Celsius",
			"# TYPE quectel_modem_temp_max_celsius gauge",
			`quectel_modem_temp_max_celsius ${temp_max_celsius}`
		);
	}

	let temp_crit = read_sysfs("/sys/kernel/quectel_rm520n_thermal/temp_crit");
	if (temp_crit) {
		let temp_crit_celsius = int(temp_crit) / 1000;
		push(metrics,
			"# HELP quectel_modem_temp_crit_celsius Critical temperature threshold in Celsius",
			"# TYPE quectel_modem_temp_crit_celsius gauge",
			`quectel_modem_temp_crit_celsius ${temp_crit_celsius}`
		);
	}

	// Read kernel module statistics
	let stats_content = read_sysfs("/sys/kernel/quectel_rm520n_thermal/stats");
	if (stats_content) {
		let stats = parse_stats(stats_content);

		if (stats.total_updates != null) {
			push(metrics,
				"# HELP quectel_modem_updates_total Total number of temperature updates from kernel",
				"# TYPE quectel_modem_updates_total counter",
				`quectel_modem_updates_total ${stats.total_updates}`
			);
		}

		if (stats.last_update_time != null) {
			push(metrics,
				"# HELP quectel_modem_last_update_timestamp_seconds Timestamp of last temperature update",
				"# TYPE quectel_modem_last_update_timestamp_seconds gauge",
				`quectel_modem_last_update_timestamp_seconds ${stats.last_update_time}`
			);
		}
	}

	// Check daemon status
	let daemon_running = check_daemon_running();
	push(metrics,
		"# HELP quectel_daemon_running Daemon running status (1=running, 0=stopped)",
		"# TYPE quectel_daemon_running gauge",
		`quectel_daemon_running ${daemon_running}`
	);

	return join("\n", metrics) + "\n";
}

// Export the collector
return collect();
