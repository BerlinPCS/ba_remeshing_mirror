#include "ui/app_viewer.h"
#include "core/types.h"
#include "imgui.h"
#include "io/paraview.h"
#include "remesher/loss.h"
#include "ui/visuals.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <portable-file-dialogs.h>
#include <thread>

// Helper Function to Format File Names for ImGUI - not part of class
std::string format_file_name(const std::string &name) {
	std::string result;
	bool capitalizeNext = true;
	for (char ch : name) {
		if (ch == '_' || ch == '-') {
			result += ' ';
			capitalizeNext = true;
		} else {
			if (capitalizeNext) {
				result += std::toupper(ch);
				capitalizeNext = false;
			} else {
				result += ch;
			}
		}
	}
	return result;
}

std::string trim_copy(const std::string &value) {
	const auto start = value.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return "";
	const auto end = value.find_last_not_of(" \t\r\n");
	return value.substr(start, end - start + 1);
}

namespace ba::ui {

void AppViewer::init() {
	file_paths.clear();
	mesh_names.clear();
	for (auto &file : std::filesystem::recursive_directory_iterator(DATA_DIR)) {
		if (file.is_regular_file()) {
			auto ext = file.path().extension().string();
			if (ext == ".off" || ext == ".obj" || ext == ".stl") {
				file_paths.push_back(file.path().string());
				mesh_names.push_back(format_file_name(file.path().stem().string()));
			}
		}
	}

	polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::ShadowOnly;
	polyscope::options::shadowBlurIters = 6;
	polyscope::init();
	if (!file_paths.empty())
		reset();
	else
		polyscope::error("No valid mesh files found.");
	polyscope::state::userCallback = [this]() { this->draw_ui(); };
}

void AppViewer::create_logger(const std::string &log_path) {
	logger = std::make_unique<io::Logger>(log_path);
}

io::PlotRunConfig AppViewer::plot_config() const {
	io::PlotRunConfig config;
	config.mesh_name = current_file_name;
	config.mesh_path = current_mesh_path;
	config.settings = r_ctx;
	config.target_length_multiplier = target_length_multiplier;
	config.permanent_outputs = permanent_outputs;
	return config;
}

void AppViewer::reset(const std::string &file_path) {
	// Reset State
	l_ctx.logs = 0;
	p_ctx.store(ProgressState());
	io::remove_vtks();
	mesh.clear();

	// Load mesh and compute remeshing parameters
	const std::string source_path = file_path.empty() ? file_paths[selected_mesh] : file_path;
	const bool mesh_changed = current_mesh_path != source_path;
	current_mesh_path = source_path;
	pmp::read(mesh, source_path);
	average_edge_length = static_cast<float>(avg_edge_length(mesh));
	r_ctx.target_length = average_edge_length * target_length_multiplier;
	loss_map_range = get_loss_range(loss::get_vertex_losses(mesh, r_ctx.target_length));
	r_ctx.log_frequency = std::max(1, (int)mesh.n_edges() / 8);
	r_ctx.progress_frequency = std::max(1, (int)mesh.n_edges() / 16);

	if (r_type == RemesherType::PRIORITY_LOCAL) {
		remesher = std::make_unique<RemesherPrioLocal>(mesh, r_ctx, p_ctx);
	} else if (r_type == RemesherType::PRIORITY_GLOBAL) {
		remesher = std::make_unique<RemesherPrioGlobal>(mesh, r_ctx, p_ctx);
	} else {
		remesher = std::make_unique<RemesherStandard>(mesh, r_ctx, p_ctx);
	}

	// Draw Mesh
	draw_surface_mesh(mesh, r_ctx.target_length, loss_map_range)
		->setEdgeWidth(1.0)
		->getQuantity("Edge Loss")
		->setEnabled(show_vertex_loss);
	polyscope::view::resetCameraToHomeView();

	// Log initial values
	current_file_name = std::filesystem::path(source_path).stem().string();
	const std::string strategy_name = remesher_names[static_cast<int>(r_type)] + "_" +
									  split_mode_names[static_cast<int>(r_ctx.split)] + "_" +
									  flip_mode_names[static_cast<int>(r_ctx.flip)];
	create_logger(plotter.default_log_path(current_file_name, strategy_name));
	if (mesh_changed) plotter.reset_manual_runs();
	log(true);
}

void AppViewer::log(bool initial_log) {
	ProgressState state = p_ctx.load();
	if (initial_log || l_ctx.logging) {
		if (logger) logger->log_iteration(state);
		if (l_ctx.vtk_export) {
			io::export_mesh_vtk(current_file_name, mesh, loss::get_vertex_losses(mesh, r_ctx.target_length),
								l_ctx.logs);
		}
		l_ctx.logs++;
	}
}

void AppViewer::draw_ui() {
	ImGui::BeginDisabled(p_ctx.load().is_remeshing);
	draw_mesh_control();
	ImGui::BeginDisabled(!remesher);
	draw_remesh_control();
	if (r_type != RemesherType::BASE) draw_prio_control();
	draw_visualization_control();
	ImGui::EndDisabled(); // !remesher
	ImGui::EndDisabled(); // is_remeshing
	condition_updates();
}

void AppViewer::draw_mesh_control() {
	ImGui::Text("Meshes:");
	ImGui::Separator();
	static std::vector<const char *> names;
	names.clear();
	for (const auto &name : mesh_names) names.push_back(name.c_str());
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::Combo("##mesh_selector", &selected_mesh, names.data(), (int)names.size())) reset();
	ImGui::SameLine();
	if (ImGui::Button("Load from File")) {
		auto paths = pfd::open_file("Load Mesh", "",
									std::vector<std::string>{"Mesh files (*.off *.obj *.stl)", "*.off;*.obj;*.stl",
															 "OFF files (*.off)", "*.off", "OBJ files (*.obj)", "*.obj",
															 "STL files (*.stl)", "*.stl", "All files (*.*)", "*.*"},
									pfd::opt::none)
						 .result();
		if (!paths.empty()) reset(paths[0]);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Mesh")) reset();
}

void AppViewer::draw_remesh_control() {
	ImGui::NewLine();
	ImGui::Text("Remeshing:");
	ImGui::Separator();
	ImGui::SetNextItemWidth(180.0f);
	static std::vector<const char *> names;
	names.clear();
	for (const auto &name : remesher_names) names.push_back(name.c_str());
	if (ImGui::Combo("##remesher_strategy", (int *)&r_type, names.data(), (int)names.size())) reset();

	// Remeshing creates a new thread so ui doesnt freeze
	ImGui::SameLine();
	if (ImGui::Button("Remesh")) {
		const std::string strategy_name = remesher_names[static_cast<int>(r_type)] + "_" +
										  split_mode_names[static_cast<int>(r_ctx.split)] + "_" +
										  flip_mode_names[static_cast<int>(r_ctx.flip)];
		create_logger(plotter.begin_manual_run_log(plot_config(), strategy_name));

		p_ctx.update([](ProgressState &s) {
			s = ProgressState();
			s.is_remeshing = true;
		});

		remesher->set_metrics();
		log(true);
		remesher->set_progress_callback([this](bool is_log_tick) {
			if (is_log_tick) log();
		});

		std::thread([this]() {
			remesher->remesh();
			p_ctx.update([](ProgressState &s) { s.is_remeshing = false; });
		}).detach();

		ImGui::OpenPopup("Remeshing Progress");
	}
	ImGui::SameLine();
	ImGui::Checkbox("Log Data", &l_ctx.logging);
	ImGui::SameLine();
	ImGui::BeginDisabled(!l_ctx.logging);
	ImGui::SetNextItemWidth(60.0);
	if (ImGui::InputInt("Log Freq", &r_ctx.log_frequency, 0)) {
	}
	ImGui::EndDisabled(); // !logging
	if (r_type != RemesherType::PRIORITY_GLOBAL) {
		ImGui::SetNextItemWidth(250.0);
		if (ImGui::SliderInt("Iterations", &r_ctx.iterations, 0, 100, "%d")) {
		}
	}
	ImGui::SetNextItemWidth(250.0f);
	if (ImGui::SliderFloat("Target Edge Length", &target_length_multiplier, 0.33f, 3.0f, "%.2fx")) {
		r_ctx.target_length = average_edge_length * target_length_multiplier;
		loss_map_range = get_loss_range(loss::get_vertex_losses(mesh, r_ctx.target_length));
	}

	if (!l_ctx.logging) {
		auto *phase_remesher = dynamic_cast<PhaseBasedRemesher *>(remesher.get());
		if (!phase_remesher) return;
		ImGui::NewLine();
		ImGui::Text("Single Operations (Debug):");
		ImGui::Separator();
		if (ImGui::Button("Split")) {
			phase_remesher->run_debug_phase(OpType::Split);
			draw_surface_mesh(mesh, r_ctx.target_length, loss_map_range);
		}
		ImGui::SameLine();
		if (ImGui::Button("Collapse")) {
			phase_remesher->run_debug_phase(OpType::Collapse);
			draw_surface_mesh(mesh, r_ctx.target_length, loss_map_range);
		}
		ImGui::SameLine();
		if (ImGui::Button("Flip")) {
			phase_remesher->run_debug_phase(OpType::Flip);
			draw_surface_mesh(mesh, r_ctx.target_length, loss_map_range);
		}
		ImGui::SameLine();
		if (ImGui::Button("Smooth")) {
			phase_remesher->run_debug_phase(OpType::Smooth);
			draw_surface_mesh(mesh, r_ctx.target_length, loss_map_range);
		}
		ImGui::SameLine();
		if (ImGui::Button("Iterate")) {
			remesher->single_iteration();
			draw_surface_mesh(mesh, r_ctx.target_length, loss_map_range);
		}
	}
}

void AppViewer::draw_prio_control() {
	ImGui::NewLine();
	ImGui::Text("Priority-Based Remeshing:");
	ImGui::Separator();
    if (ImGui::SliderFloat("Convergance Threshold", &r_ctx.op_gain_threshold, 1e-6f, 1e-2f, "%.6f",
						   ImGuiSliderFlags_Logarithmic)) {
	}
	static std::vector<const char *> names;
	names.clear();
	for (const auto &name : split_mode_names) names.push_back(name.c_str());
	if (ImGui::Combo("Split Strategy##strategy", (int *)&r_ctx.split, names.data(), (int)names.size())) reset();

	names.clear();
	for (const auto &name : flip_mode_names) names.push_back(name.c_str());
	if (ImGui::Combo("Flip Strategy##flip_strategy", (int *)&r_ctx.flip, names.data(), (int)names.size())) reset();
	if (r_ctx.flip == FlipMode::VALENCE) {
		if (ImGui::SliderInt("Flip Frequency", &r_ctx.flip_frequency, 1, 10)) {
		}
	}
}

void AppViewer::draw_visualization_control() {
	ImGui::NewLine();
	ImGui::Text("Miscellaneous:");
	ImGui::Separator();
	if (ImGui::Checkbox("Show Vertex Loss", &show_vertex_loss)) {
		if (polyscope::hasSurfaceMesh("Mesh")) {
			polyscope::getSurfaceMesh("Mesh")->getQuantity("Edge Loss")->setEnabled(show_vertex_loss);
		}
	}
	if (show_vertex_loss) {
		ImGui::SameLine();
		if (ImGui::Button("Reset Range")) {
			loss_map_range = get_loss_range(loss::get_vertex_losses(mesh, r_ctx.target_length));
			draw_surface_mesh(mesh, r_ctx.target_length, loss_map_range);
		}
	}
	ImGui::Checkbox("Export VTK", &l_ctx.vtk_export);
	ImGui::Checkbox("Permanent Plots/Logs", &permanent_outputs);
	ImGui::BeginDisabled(plotter.is_running());
	if (ImGui::Button("Plot Manual Runs")) {
		plotter.plot_manual_runs(plot_config());
	}
	ImGui::SameLine();
	if (ImGui::Button("Run All Strategies & Plot")) {
		plotter.run_all_strategies_and_plot(plot_config());
	}
	ImGui::SameLine();
	if (ImGui::Button("Run Preset & Plot")) {
		plotter.run_preset_and_plot(plot_config());
	}
	ImGui::EndDisabled();
	const std::string status = plotter.get_status();
	if (!status.empty()) {
		ImGui::TextWrapped("%s", status.c_str());
	}
}

void AppViewer::condition_updates() {
	std::string completed_export_dir;
	if (plotter.consume_completed_export_dir(completed_export_dir)) {
		pending_export_rename_dir = completed_export_dir;
		export_rename_error.clear();
		export_rename_input.fill('\0');
		ImGui::OpenPopup("Rename Export Folder");
	}

	// Popup while remeshing
	if (ImGui::BeginPopupModal("Remeshing Progress", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Remeshing in progress...");
		ProgressState state = p_ctx.load();
        QueueStats q = state.queue_stats;
        float progress = state.is_remeshing ? 0.0f : 1.0f;
        if(r_type == RemesherType::PRIORITY_GLOBAL) {
            if(q.popped > 0) progress = static_cast<float>(q.popped) / static_cast<float>(q.popped + q.queued);
        } else {
            progress = static_cast<float>(state.metrics.operations) / static_cast<float>(r_ctx.iterations);
        }
		ImGui::ProgressBar(progress, ImVec2(350.0f, 0.0f));
		ImGui::Text("Operations: %d", state.metrics.operations);
		ImGui::Text("Total Edge Loss: %.4f", state.metrics.total_edge_loss);
		if (r_type == RemesherType::PRIORITY_GLOBAL) {
            ImGui::Text("Approximate workload: %d processed + %d queued", q.popped, q.queued);
			ImGui::Text("Queue: %d stale, %d rejected, %d rebuilds", q.stale, q.rejected, q.rebuilt);
			ImGui::Text("Termination: %s", termination_reason_name(state.termination_reason));
		}
		if (!state.is_remeshing) {
			draw_surface_mesh(mesh, r_ctx.target_length, loss_map_range);
			if (ImGui::Button("OK", ImVec2(350.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Rename Export Folder", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		const std::filesystem::path current_path(pending_export_rename_dir);
		const std::string current_name = current_path.filename().string();
		ImGui::Text("Current folder:");
		ImGui::TextWrapped("%s", current_name.c_str());
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		const bool submit =
			ImGui::InputTextWithHint("##export_rename", "Leave empty to keep current name", export_rename_input.data(),
									 export_rename_input.size(), ImGuiInputTextFlags_EnterReturnsTrue);
		if (!export_rename_error.empty()) {
			ImGui::TextColored(ImVec4(0.85f, 0.35f, 0.35f, 1.0f), "%s", export_rename_error.c_str());
		}
		ImGui::TextDisabled("Enter accepts, Escape keeps the current folder name.");

		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			export_rename_error.clear();
			ImGui::CloseCurrentPopup();
		} else if (submit) {
			const std::string requested_name = trim_copy(export_rename_input.data());
			if (requested_name.empty() || requested_name == current_name) {
				export_rename_error.clear();
				ImGui::CloseCurrentPopup();
			} else if (requested_name.find('/') != std::string::npos || requested_name.find('\\') != std::string::npos ||
					   requested_name == "." || requested_name == "..") {
				export_rename_error = "Please enter a valid folder name.";
			} else {
				const std::filesystem::path renamed_path = current_path.parent_path() / requested_name;
				if (std::filesystem::exists(renamed_path)) {
					export_rename_error = "A folder with that name already exists.";
				} else {
					try {
						std::filesystem::rename(current_path, renamed_path);
						pending_export_rename_dir = renamed_path.string();
						export_rename_error.clear();
						ImGui::CloseCurrentPopup();
					} catch (const std::filesystem::filesystem_error &) {
						export_rename_error = "Could not rename the export folder.";
					}
				}
			}
		}

		ImGui::EndPopup();
	}
}

} // namespace ba::ui
