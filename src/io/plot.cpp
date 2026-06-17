#include "io/plot.h"

#include "core/types.h"
#include "io/logger.h"
#include "remesher/remesher.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <pmp/io/io.h>
#include <sstream>
#include <thread>

namespace ba::io {

namespace {

const std::array<std::string, 3> remesher_names = {"Standard", "Priority Local", "Priority Global"};
const std::array<std::string, 3> split_mode_names = {"Sum", "Max", "Avg"};
const std::array<std::string, 2> flip_mode_names = {"Valence", "Edge Loss"};
const std::array<SplitMode, 3> all_split_modes = {SplitMode::SUM, SplitMode::MAX, SplitMode::AVG};
const std::array<FlipMode, 2> all_flip_modes = {FlipMode::VALENCE, FlipMode::EDGE_LOSS};

std::string sanitize_file_part(std::string value) {
	for (char &ch : value) {
		if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '.') ch = '_';
	}
	return value;
}

std::string shell_quote(const std::string &value) {
	std::string result = "'";
	for (char ch : value) {
		if (ch == '\'')
			result += "'\\''";
		else
			result += ch;
	}
	result += "'";
	return result;
}

std::string python_executable() {
	const std::filesystem::path venv_python = std::filesystem::path(BA_SOURCE_DIR) / "scripts" / ".venv" / "bin" / "python";
	if (std::filesystem::exists(venv_python)) return venv_python.string();
	return "python3";
}

std::unique_ptr<Remesher> make_remesher(RemesherType type, Mesh &mesh, RemesherSettings &settings, SyncState<ProgressState> &progress) {
	if (type == RemesherType::PRIORITY_LOCAL) {
		return std::make_unique<RemesherPrioLocal>(mesh, settings, progress);
	}
	if (type == RemesherType::PRIORITY_GLOBAL) {
		return std::make_unique<RemesherPrioGlobal>(mesh, settings, progress);
	}
	return std::make_unique<RemesherStandard>(mesh, settings, progress);
}

std::string format_strategy(const StrategyRun& run, std::string sep) {
	if (run.type == RemesherType::BASE) {
		return remesher_names[static_cast<int>(run.type)] + sep +
			   flip_mode_names[static_cast<int>(run.flip)];
	}
	return remesher_names[static_cast<int>(run.type)] + sep +
		   split_mode_names[static_cast<int>(run.split)] + sep +
		   flip_mode_names[static_cast<int>(run.flip)];
}

std::vector<StrategyRun> make_all_strategy_runs() {
	std::vector<StrategyRun> runs;
	for (FlipMode flip : all_flip_modes) {
		runs.push_back({RemesherType::BASE, SplitMode::SUM, flip});
	}
	for (RemesherType type : {RemesherType::PRIORITY_LOCAL, RemesherType::PRIORITY_GLOBAL}) {
		for (SplitMode split : all_split_modes) {
			for (FlipMode flip : all_flip_modes) {
				runs.push_back({type, split, flip});
			}
		}
	}
	return runs;
}

std::string trim(std::string value) {
	const auto start = value.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return "";
	const auto end = value.find_last_not_of(" \t\r\n");
	return value.substr(start, end - start + 1);
}

std::string lower(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return std::tolower(ch); });
	return value;
}

bool parse_remesher_type(const std::string &value, RemesherType &out) {
	const std::string normalized = lower(trim(value));
	if (normalized == "standard" || normalized == "base") {
		out = RemesherType::BASE;
		return true;
	}
	if (normalized == "priority local" || normalized == "priority_local" || normalized == "local") {
		out = RemesherType::PRIORITY_LOCAL;
		return true;
	}
	if (normalized == "priority global" || normalized == "priority_global" || normalized == "global") {
		out = RemesherType::PRIORITY_GLOBAL;
		return true;
	}
	return false;
}

bool parse_split_mode(const std::string &value, SplitMode &out) {
	const std::string normalized = lower(trim(value));
	if (normalized == "sum") {
		out = SplitMode::SUM;
		return true;
	}
	if (normalized == "max") {
		out = SplitMode::MAX;
		return true;
	}
	if (normalized == "avg" || normalized == "average") {
		out = SplitMode::AVG;
		return true;
	}
	return false;
}

bool is_all_token(const std::string &value) { return lower(trim(value)) == "all"; }

bool parse_flip_mode(const std::string &value, FlipMode &out) {
	const std::string normalized = lower(trim(value));
	if (normalized == "valence") {
		out = FlipMode::VALENCE;
		return true;
	}
	if (normalized == "edge loss" || normalized == "edge_loss" || normalized == "loss") {
		out = FlipMode::EDGE_LOSS;
		return true;
	}
	return false;
}

std::vector<std::string> split_csv_line(const std::string &line) {
	std::vector<std::string> values;
	std::stringstream ss(line);
	std::string value;
	while (std::getline(ss, value, ',')) values.push_back(trim(value));
	return values;
}

std::vector<StrategyRun> load_preset_runs(const RemesherSettings &settings) {
	std::vector<StrategyRun> runs;
	const std::filesystem::path config_path = std::filesystem::path(BA_SOURCE_DIR) / "config" / "plot_presets.csv";
	std::ifstream file(config_path);
	if (!file.is_open()) return runs;

	std::string line;
	while (std::getline(file, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '#') continue;
		const auto values = split_csv_line(line);
		if (values.size() < 3) continue;

		std::vector<RemesherType> types;
		if (is_all_token(values[0])) {
			types = {RemesherType::BASE, RemesherType::PRIORITY_LOCAL, RemesherType::PRIORITY_GLOBAL};
		} else {
			RemesherType type;
			if (!parse_remesher_type(values[0], type)) continue;
			types.push_back(type);
		}

		std::vector<FlipMode> flips;
		if (is_all_token(values[2])) {
			flips = {FlipMode::VALENCE, FlipMode::EDGE_LOSS};
		} else {
			FlipMode flip = settings.flip;
			if (!parse_flip_mode(values[2], flip)) continue;
			flips.push_back(flip);
		}

		for (RemesherType type : types) {
			std::vector<SplitMode> splits;
			if (is_all_token(values[1])) {
				if (type == RemesherType::BASE) {
					splits.push_back(settings.split);
				} else {
					splits = {SplitMode::SUM, SplitMode::MAX, SplitMode::AVG};
				}
			} else {
				SplitMode split = settings.split;
				if (!parse_split_mode(values[1], split)) continue;
				splits.push_back(split);
			}

			for (SplitMode split : splits) {
				for (FlipMode flip : flips) {
					const StrategyRun run = {type, split, flip};
					const std::string file_key = format_strategy(run, "_");
					const auto existing = std::find_if(
						runs.begin(), runs.end(), [&](const StrategyRun &r) { return format_strategy(r, "_") == file_key; });
					if (existing == runs.end()) runs.push_back(run);
				}
			}
		}
	}
	return runs;
}

std::string get_log_path(const std::string& dir, const std::string &mesh_name, const StrategyRun& run) {
	const std::filesystem::path output_dir = dir.empty() ? std::filesystem::path(OUT_LOG_DIR) : std::filesystem::path(dir);
	const std::string filename = sanitize_file_part(mesh_name + "_" + format_strategy(run, "_")) + ".csv";
	return (output_dir / filename).string();
}

std::string get_plot_path(const std::string& dir, const std::string &mesh_name, PlotMode mode) {
	const std::filesystem::path output_dir = dir.empty() ? std::filesystem::path(OUT_PLOT_DIR) : std::filesystem::path(dir);
	const std::string filename = sanitize_file_part(mesh_name + "_" + to_string(mode)) + ".svg";
	return (output_dir / filename).string();
}

std::string make_plot_metadata(const SharedConfig &config) {
	std::ostringstream metadata;
	metadata << "target length: " << std::fixed << std::setprecision(2) << config.target_length_multiplier
			 << "x avg" << " | threshold: " << std::scientific << config.settings.op_gain_threshold
			 << " | iterations: " << std::dec << config.settings.iterations;
	return metadata.str();
}

} // namespace

std::string Plotter::hash_group(const SharedConfig &config) {
	std::ostringstream key(make_plot_metadata(config));
	key << "|" << config.mesh_path << "|" << config.export_dir;
	return std::to_string(std::hash<std::string>()(key.str()));
}

void Plotter::set_status(const std::string &status) {
	std::lock_guard<std::mutex> lock(status_mutex);
	background_status = status;
}

std::string Plotter::get_status() {
	std::lock_guard<std::mutex> lock(status_mutex);
	return background_status;
}

void Plotter::reset_manual_group(const std::string& group_hash) {
	current_group_logs.clear();
	current_group_hash = group_hash;
}

std::string Plotter::log_path(const SharedConfig &config, const StrategyRun run) {
	std::lock_guard<std::mutex> lock(status_mutex);
	ctx = config;
	const std::string group_hash = hash_group(ctx);
	if (current_group_hash != group_hash) {
		reset_manual_group(group_hash);
		background_status = "Reset Manual Group";
	}

	const std::string log_path = get_log_path(ctx.export_dir, ctx.mesh_name, run);
	const std::string strat_name = format_strategy(run, "_");
	auto existing = std::find_if(current_group_logs.begin(), current_group_logs.end(),
								 [&](const auto &entry) { return entry.second == strat_name; });
	if (existing == current_group_logs.end()) current_group_logs.push_back({log_path, strat_name});
	else *existing = {log_path, strat_name};
	return log_path;
}

std::vector<LogID> Plotter::run_strategy_batch(const SharedConfig &config, const std::vector<StrategyRun> &runs) {
	std::vector<LogID> logs;
	Mesh original_mesh;
	pmp::read(original_mesh, config.mesh_path);

	for (const StrategyRun &run : runs) {
		Mesh run_mesh = original_mesh;
		SyncState<ProgressState> progress;
		RemesherSettings settings = config.settings;
		settings.split = run.split;
		settings.flip = run.flip;
		settings.progress_frequency = 0;
		auto run_remesher = make_remesher(run.type, run_mesh, settings, progress);

		const std::string log_path = get_log_path(config.export_dir, config.mesh_name, run);
		Logger run_logger(log_path);
		run_logger.log_iteration(progress.load());
		run_remesher->set_progress_callback(
			[&](bool is_log_tick) {
				if (is_log_tick) run_logger.log_iteration(progress.load());
			});
		run_remesher->remesh();
		run_logger.log_iteration(progress.load());
		std::string label = format_strategy(run, " / ");
		logs.push_back({log_path, label});
		set_status("Finished " + label);
	}
	return logs;
}

void Plotter::run_plot_script(const SharedConfig &config, PlotMode mode, const std::vector<LogID> &logs) {
	const std::string plot_path = get_plot_path(config.export_dir, config.mesh_name, mode);
	std::ostringstream command;
	command << "MPLCONFIGDIR=/tmp "
			<< shell_quote(python_executable()) << " " << shell_quote(PLOT_SCRIPT_PATH)
			<< " --output " << shell_quote(plot_path)
			<< " --title " << shell_quote(config.mesh_name)
			<< " --metadata " << shell_quote(make_plot_metadata(config));
	for (const auto &[path, label] : logs) command << " --csv " << shell_quote(path + "=" + label);
	if (std::system(command.str().c_str()) == 0) set_status("Plot written to " + plot_path);
	else set_status("Plot failed");
}

void Plotter::plot(const PlotMode m, const SharedConfig &config) {
	if (config.mesh_path.empty() || background_task_running.exchange(true)) return;

	std::vector<StrategyRun> runs;
	std::vector<LogID> manual_logs;
	if (m == PlotMode::PRESET) {
		runs = load_preset_runs(config.settings);
		if (runs.empty()) {
			set_status("No preset runs found in config/plot_presets.csv.");
			background_task_running = false;
			return;
		}
	} else if (m == PlotMode::ALL) runs = make_all_strategy_runs();

	{
		std::lock_guard<std::mutex> lock(status_mutex);
		if (m == PlotMode::MANUAL) {
			if (current_group_hash != hash_group(config) || current_group_logs.empty()) {
				background_status = "No Logs found for this configuration. Run some strategies first.";
				background_task_running = false;
				return;
			}
			manual_logs = current_group_logs;
		}
		ctx = config;
	}

	set_status("Plotting...");
	std::thread([this, config, m, runs, manual_logs]() {
		const std::vector<LogID> logs = (m == PlotMode::MANUAL) ? manual_logs : run_strategy_batch(config, runs);
		run_plot_script(config, m, logs);
		background_task_running = false;
	}).detach();
}

} // namespace ba::io
