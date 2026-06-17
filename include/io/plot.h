#pragma once

#include "core/types.h"

#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ba::io {

enum class PlotMode { MANUAL, ALL, PRESET };

inline std::string to_string(PlotMode mode) {
	switch (mode) {
	case PlotMode::MANUAL: return "manual";
	case PlotMode::ALL: return "all";
	case PlotMode::PRESET: return "preset";
	}
	return "";
}

struct SharedConfig {
	std::string mesh_name;
	std::string mesh_path;
	std::string export_dir;
	RemesherSettings settings;
	float target_length_multiplier;
};

struct StrategyRun {
	RemesherType type;
	SplitMode split;
	FlipMode flip;
};

using LogID = std::pair<std::string, std::string>;

class Plotter {
private:
	SharedConfig ctx;
	std::atomic<bool> background_task_running = false;
	std::mutex status_mutex;
	std::string background_status;

	std::string current_group_hash;
	std::vector<LogID> current_group_logs;

	static std::string hash_group(const SharedConfig& config);
	void set_status(const std::string& status);
	void reset_manual_group(const std::string& group_hash);

	std::vector<LogID> run_strategy_batch(const SharedConfig& config, const std::vector<StrategyRun>& runs);
	void run_plot_script(const SharedConfig& config, PlotMode mode, const std::vector<LogID>& logs);

public:
	Plotter(const SharedConfig& config) : ctx(config), current_group_hash(hash_group(ctx)) {};
	bool is_running() const { return background_task_running.load(); }
	std::string get_status();
	void reset_export_dirs();
	std::string log_path(const SharedConfig& config, const StrategyRun run);
	void plot(PlotMode mode, const SharedConfig& config);
};

} // namespace ba::io
