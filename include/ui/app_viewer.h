#pragma once

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/types.h"
#include "io/logger.h"
#include "io/plot.h"
#include "remesher/remesher.h"

#include "polyscope/polyscope.h"
#include <pmp/io/io.h>

namespace ba::ui {

class AppViewer {
private:
	// Application State
	Mesh mesh;
	std::unique_ptr<Remesher> remesher;
	const std::vector<std::string> remesher_names = {"Standard", "Priority Local", "Priority Global"};
	const std::vector<std::string> split_mode_names = {"Sum", "Max", "Avg"};
	const std::vector<std::string> collapse_mode_names = {"Loss"};
	const std::vector<std::string> flip_mode_names = {"Valence", "Edge Loss"};
	std::vector<std::string> file_paths;
	std::vector<std::string> mesh_names;
	std::string current_file_name;
	std::string current_mesh_path;
	std::string export_dir;
	std::string default_export_dir_name;
	std::string export_rename_error;
	std::array<char, 256> export_rename_input = {};
	int selected_mesh = 8;
	bool show_vertex_loss = true;
	bool permanent_outputs = false;

	// Remesher Settings
	RemesherSettings r_ctx;
	RemesherType r_type = RemesherType::PRIORITY_GLOBAL;
	float average_edge_length = 0.0f;
	float target_length_multiplier = 1.5f;
	std::pair<double, double> loss_map_range = {0.0, 1.0};

	// Logging & Progress
	LoggingState l_ctx;
	std::unique_ptr<io::Logger> logger;
	std::unique_ptr<io::Plotter> plotter;
	SyncState<ProgressState> p_ctx;
	void log(bool initial_log = false);
	io::SharedConfig plot_config() const;

	// Reset Polyscope State
	void reset(const std::string& filepath = "");

	// UI Rendering
	void draw_ui();
	void draw_mesh_control();
	void draw_remesh_control();
	void draw_prio_control();
	void draw_visualization_control();
	void condition_updates();

public:
	void init();
	void run() { polyscope::show(); };
};

} // namespace ba::ui
