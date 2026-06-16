#pragma once

#include "core/types.h"

#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ba::io {

struct PlotRunConfig {
	std::string mesh_name;
	std::string mesh_path;
	RemesherSettings settings;
	float target_length_multiplier = 1.0f;
	bool permanent_outputs = false;
};

class Plotter {
private:
	std::atomic<bool> background_task_running = false;
	std::mutex status_mutex;
	std::string background_status;
	std::string manual_export_dir;
	std::string last_plot_path;
	std::string pending_rename_export_dir;
	std::string manual_group_key;
	std::vector<std::pair<std::string, std::string>> manual_run_logs;

	void set_status(const std::string &status);

public:
	bool is_running() const { return background_task_running.load(); }
	std::string get_status();
	bool consume_completed_export_dir(std::string &export_dir);

	void reset_manual_runs();
	std::string default_log_path(const std::string &mesh_name, const std::string &strategy_name) const;
	std::string begin_manual_run_log(const PlotRunConfig &config, const std::string &strategy_name);

	void plot_manual_runs(const PlotRunConfig &config);
	void run_all_strategies_and_plot(const PlotRunConfig &config);
	void run_preset_and_plot(const PlotRunConfig &config);
};

} // namespace ba::io
