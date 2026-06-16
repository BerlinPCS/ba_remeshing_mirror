#include "io/plot.h"

#include "core/geo_utils.h"
#include "io/logger.h"
#include "remesher/remesher.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <pmp/io/io.h>
#include <sstream>
#include <thread>

namespace ba::io {

namespace {

const std::array<std::string, 3> remesher_names = {"Standard", "Priority Local", "Priority Global"};
const std::array<std::string, 3> split_mode_names = {"Sum", "Max", "Avg"};
const std::array<std::string, 2> flip_mode_names = {"Valence", "Edge Loss"};

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

std::string timestamp_string() {
	const auto now = std::chrono::system_clock::now();
	const std::time_t time = std::chrono::system_clock::to_time_t(now);
	std::tm tm = *std::localtime(&time);
	std::ostringstream out;
	out << std::put_time(&tm, "%Y%m%d_%H%M%S");
	return out.str();
}

std::unique_ptr<Remesher> make_remesher(RemesherType type, Mesh &mesh, RemesherSettings &settings,
										SyncState<ProgressState> &progress) {
	if (type == RemesherType::PRIORITY_LOCAL) {
		return std::make_unique<RemesherPrioLocal>(mesh, settings, progress);
	}
	if (type == RemesherType::PRIORITY_GLOBAL) {
		return std::make_unique<RemesherPrioGlobal>(mesh, settings, progress);
	}
	return std::make_unique<RemesherStandard>(mesh, settings, progress);
}

std::string make_export_dir(const std::string &mesh_name) {
	return (std::filesystem::path(BA_SOURCE_DIR) / "out" / "export" /
			(sanitize_file_part(mesh_name) + "_" + timestamp_string()))
		.string();
}

std::string make_log_path(const std::string &mesh_name, const std::string &strategy_name,
						  const std::string &export_dir = "") {
	const std::filesystem::path dir =
		export_dir.empty() ? std::filesystem::path(BA_SOURCE_DIR) / "out" / "logs" : std::filesystem::path(export_dir);
	const std::string filename = sanitize_file_part(mesh_name + "_" + strategy_name) + ".csv";
	return (dir / filename).string();
}

struct StrategyRun {
	RemesherType type;
	SplitMode split;
	FlipMode flip;
	std::string label;
	std::string file_key;
};

std::string strategy_label(RemesherType type, SplitMode split, FlipMode flip) {
	if (type == RemesherType::BASE) {
		return remesher_names[static_cast<int>(type)] + " / " + flip_mode_names[static_cast<int>(flip)];
	}
	return remesher_names[static_cast<int>(type)] + " / " + split_mode_names[static_cast<int>(split)] + " / " +
		   flip_mode_names[static_cast<int>(flip)];
}

std::string strategy_file_key(RemesherType type, SplitMode split, FlipMode flip) {
	if (type == RemesherType::BASE) {
		return remesher_names[static_cast<int>(type)] + "_" + flip_mode_names[static_cast<int>(flip)];
	}
	return remesher_names[static_cast<int>(type)] + "_" + split_mode_names[static_cast<int>(split)] + "_" +
		   flip_mode_names[static_cast<int>(flip)];
}

std::string make_manual_group_key(const PlotRunConfig &config) {
	std::ostringstream key;
	key << config.mesh_path << "|target=" << std::fixed << std::setprecision(8) << config.target_length_multiplier
		<< "|threshold=" << std::scientific << config.settings.op_gain_threshold
		<< "|iterations=" << config.settings.iterations;
	return key.str();
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
					const std::string file_key = strategy_file_key(type, split, flip);
					const auto existing = std::find_if(
						runs.begin(), runs.end(), [&](const StrategyRun &run) { return run.file_key == file_key; });
					if (existing == runs.end()) {
						runs.push_back({type, split, flip, strategy_label(type, split, flip), file_key});
					}
				}
			}
		}
	}
	return runs;
}

std::vector<std::pair<std::string, std::string>> run_strategy_batch(const PlotRunConfig &config,
																	const std::vector<StrategyRun> &runs,
																	const std::string &export_dir,
																	const std::function<void(const std::string &)> &set_status) {
	std::vector<std::pair<std::string, std::string>> logs;
	for (const StrategyRun &run : runs) {
		Mesh run_mesh;
		pmp::read(run_mesh, config.mesh_path);

		RemesherSettings settings = config.settings;
		settings.split = run.split;
		settings.flip = run.flip;
		settings.target_length = static_cast<float>(avg_edge_length(run_mesh) * config.target_length_multiplier);
		settings.log_frequency = std::max(1, static_cast<int>(run_mesh.n_edges()) / 8);
		settings.progress_frequency = 0;

		SyncState<ProgressState> progress;
		auto run_remesher = make_remesher(run.type, run_mesh, settings, progress);

		const std::string log_path = make_log_path(config.mesh_name, run.file_key, export_dir);
		Logger run_logger(log_path);
		run_logger.log_iteration(progress.load());
		run_remesher->set_progress_callback(
			[&](bool is_log_tick) {
				if (is_log_tick) run_logger.log_iteration(progress.load());
			});
		run_remesher->remesh();
		run_logger.log_iteration(progress.load());
		logs.push_back({log_path, run.label});
		set_status("Finished " + run.label);
	}
	return logs;
}

std::string make_plot_path(const std::string &mesh_name, const std::string &suffix,
						   const std::string &export_dir = "") {
	const std::filesystem::path dir =
		export_dir.empty() ? std::filesystem::path(BA_SOURCE_DIR) / "out" / "plots" : std::filesystem::path(export_dir);
	const std::string filename = sanitize_file_part(mesh_name + "_" + suffix) + ".svg";
	return (dir / filename).string();
}

std::string make_plot_metadata(const PlotRunConfig &config) {
	std::ostringstream metadata;
	metadata << "target length: " << std::fixed << std::setprecision(2) << config.target_length_multiplier
			 << "x average edge length (" << std::setprecision(6) << config.settings.target_length << ")"
			 << " | threshold: " << std::scientific << config.settings.op_gain_threshold
			 << " | iterations: " << std::dec << config.settings.iterations;
	return metadata.str();
}

int run_plot_script(const std::vector<std::pair<std::string, std::string>> &logs, const std::string &output_path,
					const std::string &title, const std::string &metadata) {
	std::ostringstream command;
	command << "MPLCONFIGDIR=/tmp " << shell_quote(python_executable()) << " "
			<< shell_quote((std::filesystem::path(BA_SOURCE_DIR) / "scripts" / "plot_metrics.py").string())
			<< " --output " << shell_quote(output_path) << " --title " << shell_quote(title) << " --metadata "
			<< shell_quote(metadata);
	for (const auto &[path, label] : logs) {
		command << " --csv " << shell_quote(path + "=" + label);
	}
	return std::system(command.str().c_str());
}

} // namespace

void Plotter::set_status(const std::string &status) {
	std::lock_guard<std::mutex> lock(status_mutex);
	background_status = status;
}

std::string Plotter::get_status() {
	std::lock_guard<std::mutex> lock(status_mutex);
	return background_status;
}

bool Plotter::consume_completed_export_dir(std::string &export_dir) {
	std::lock_guard<std::mutex> lock(status_mutex);
	if (pending_rename_export_dir.empty()) return false;
	export_dir = pending_rename_export_dir;
	pending_rename_export_dir.clear();
	return true;
}

void Plotter::reset_manual_runs() {
	std::lock_guard<std::mutex> lock(status_mutex);
	manual_run_logs.clear();
	manual_export_dir.clear();
	last_plot_path.clear();
	pending_rename_export_dir.clear();
	manual_group_key.clear();
}

std::string Plotter::default_log_path(const std::string &mesh_name, const std::string &strategy_name) const {
	return make_log_path(mesh_name, strategy_name);
}

std::string Plotter::begin_manual_run_log(const PlotRunConfig &config, const std::string &strategy_name) {
	std::lock_guard<std::mutex> lock(status_mutex);
	const std::string group_key = make_manual_group_key(config);
	if (manual_group_key != group_key) {
		manual_run_logs.clear();
		manual_export_dir.clear();
		manual_group_key = group_key;
	}

	const std::string export_dir =
		config.permanent_outputs ? (manual_export_dir.empty() ? make_export_dir(config.mesh_name) : manual_export_dir) : "";
	const std::string log_path = make_log_path(config.mesh_name, strategy_name, export_dir);
	auto existing =
		std::find_if(manual_run_logs.begin(), manual_run_logs.end(),
					 [&](const auto &entry) { return entry.second == strategy_name; });
	if (existing == manual_run_logs.end()) {
		manual_run_logs.push_back({log_path, strategy_name});
	} else {
		*existing = {log_path, strategy_name};
	}
	manual_export_dir = export_dir;
	return log_path;
}

void Plotter::plot_manual_runs(const PlotRunConfig &config) {
	std::vector<std::pair<std::string, std::string>> logs;
	std::string export_dir;
	{
		std::lock_guard<std::mutex> lock(status_mutex);
		if (manual_group_key != make_manual_group_key(config)) {
			background_status = "Manual runs use a different target length/settings. Run this configuration first.";
			return;
		}
		logs = manual_run_logs;
		export_dir = manual_export_dir;
	}
	if (logs.empty() || background_task_running.exchange(true)) return;

	if (config.permanent_outputs && export_dir.empty()) export_dir = make_export_dir(config.mesh_name);
	if (!config.permanent_outputs) export_dir.clear();

	const std::string output_path = make_plot_path(config.mesh_name, "manual_runs", export_dir);
	const std::string title = config.mesh_name + " Manual Remeshing Runs";
	const std::string metadata = make_plot_metadata(config);
	{
		std::lock_guard<std::mutex> lock(status_mutex);
		manual_export_dir = export_dir;
		last_plot_path = output_path;
	}
	set_status("Plotting manual runs...");

	std::thread([this, logs, output_path, title, metadata]() {
		const int result = run_plot_script(logs, output_path, title, metadata);
		if (result == 0) {
			{
				std::lock_guard<std::mutex> lock(status_mutex);
				if (!manual_export_dir.empty()) pending_rename_export_dir = manual_export_dir;
			}
			set_status("Plot written to " + output_path);
		} else {
			set_status("Plot command failed.");
		}
		background_task_running = false;
	}).detach();
}

void Plotter::run_all_strategies_and_plot(const PlotRunConfig &config) {
	if (config.mesh_path.empty() || background_task_running.exchange(true)) return;

	const std::string export_dir = config.permanent_outputs ? make_export_dir(config.mesh_name) : "";
	const std::string output_path = make_plot_path(config.mesh_name, "all_strategies", export_dir);
	const std::string metadata = make_plot_metadata(config);
	{
		std::lock_guard<std::mutex> lock(status_mutex);
		manual_export_dir = export_dir;
		last_plot_path = output_path;
	}
	set_status("Running all strategies...");

	std::thread([this, config, export_dir, output_path, metadata]() {
		const std::array<SplitMode, 3> split_modes = {SplitMode::SUM, SplitMode::MAX, SplitMode::AVG};
		const std::array<FlipMode, 2> flip_modes = {FlipMode::VALENCE, FlipMode::EDGE_LOSS};

		std::vector<StrategyRun> runs;
		for (FlipMode flip : flip_modes) {
			runs.push_back({RemesherType::BASE, config.settings.split, flip,
							strategy_label(RemesherType::BASE, config.settings.split, flip),
							strategy_file_key(RemesherType::BASE, config.settings.split, flip)});
		}
		for (RemesherType type : {RemesherType::PRIORITY_LOCAL, RemesherType::PRIORITY_GLOBAL}) {
			for (SplitMode split : split_modes) {
				for (FlipMode flip : flip_modes) {
					runs.push_back({type, split, flip, strategy_label(type, split, flip),
									strategy_file_key(type, split, flip)});
				}
			}
		}

		auto logs = run_strategy_batch(config, runs, export_dir, [this](const std::string &status) { set_status(status); });

		{
			std::lock_guard<std::mutex> lock(status_mutex);
			manual_run_logs = logs;
			manual_group_key = make_manual_group_key(config);
		}

		const int result = run_plot_script(logs, output_path, config.mesh_name + " All Remeshing Strategies", metadata);
		if (result == 0) {
			{
				std::lock_guard<std::mutex> lock(status_mutex);
				if (!manual_export_dir.empty()) pending_rename_export_dir = manual_export_dir;
			}
			set_status("All-strategy plot written to " + output_path);
		} else {
			set_status("All-strategy plot failed.");
		}
		background_task_running = false;
	}).detach();
}

void Plotter::run_preset_and_plot(const PlotRunConfig &config) {
	if (config.mesh_path.empty() || background_task_running.exchange(true)) return;

	const auto runs = load_preset_runs(config.settings);
	if (runs.empty()) {
		set_status("No preset runs found in config/plot_presets.csv.");
		background_task_running = false;
		return;
	}

	const std::string export_dir = config.permanent_outputs ? make_export_dir(config.mesh_name) : "";
	const std::string output_path = make_plot_path(config.mesh_name, "preset_runs", export_dir);
	const std::string metadata = make_plot_metadata(config);
	{
		std::lock_guard<std::mutex> lock(status_mutex);
		manual_export_dir = export_dir;
		last_plot_path = output_path;
	}
	set_status("Running preset...");

	std::thread([this, config, runs, export_dir, output_path, metadata]() {
		auto logs = run_strategy_batch(config, runs, export_dir, [this](const std::string &status) { set_status(status); });
		{
			std::lock_guard<std::mutex> lock(status_mutex);
			manual_run_logs = logs;
			manual_group_key = make_manual_group_key(config);
		}

		const int result = run_plot_script(logs, output_path, config.mesh_name + " Preset Remeshing Runs", metadata);
		if (result == 0) {
			{
				std::lock_guard<std::mutex> lock(status_mutex);
				if (!manual_export_dir.empty()) pending_rename_export_dir = manual_export_dir;
			}
			set_status("Preset plot written to " + output_path);
		} else {
			set_status("Preset plot failed.");
		}
		background_task_running = false;
	}).detach();
}

} // namespace ba::io
