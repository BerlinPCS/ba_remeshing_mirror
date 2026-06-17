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
std::string format_file_name(const std::string& name) {
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

std::string timestamp_string() {
	const auto now = std::chrono::system_clock::now();
	const std::time_t time = std::chrono::system_clock::to_time_t(now);
	std::tm tm = *std::localtime(&time);
	std::ostringstream out;
	out << std::put_time(&tm, "%Y%m%d_%H%M%S");
	return out.str();
}

std::string trim_copy(const std::string& value) {
	const auto start = value.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return "";
	const auto end = value.find_last_not_of(" \t\r\n");
	return value.substr(start, end - start + 1);
}

namespace ba::ui {

void AppViewer::init() {
	file_paths.clear();
	mesh_names.clear();
	for (auto& file : std::filesystem::recursive_directory_iterator(DATA_DIR)) {
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

	plotter = std::make_unique<io::Plotter>(plot_config());
}

io::SharedConfig AppViewer::plot_config() const {
	return {
		current_file_name,
		current_mesh_path,
		export_dir,
		r_ctx,
		target_length_multiplier
	};
}

void AppViewer::reset(const std::string& file_path) {
	// Reset State
	l_ctx.logs = 0;
	p_ctx.store(ProgressState());
	io::remove_vtks();
	mesh.clear();

	// Load mesh and compute remeshing parameters
	current_mesh_path = file_path.empty() ? file_paths[selected_mesh] : file_path;
	current_file_name = std::filesystem::path(current_mesh_path).stem().string();
	pmp::read(mesh, current_mesh_path);
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
	static std::vector<const char*> names;
	names.clear();
	for (const auto& name : mesh_names) names.push_back(name.c_str());
	ImGui::SetNextItemWidth(180.0f);
	if (ImGui::Combo("##mesh_selector", &selected_mesh, names.data(), (int)names.size())) reset();
	ImGui::SameLine();
	if (ImGui::Button("Load from File")) {
		auto paths = pfd::open_file("Load Mesh", "",
									std::vector<std::string>{"Mesh files (*.off *.obj *.stl)", "*.off;*.obj;*.stl",
															 "OFF files (*.off)", "*.off", "OBJ files (*.obj)", "*.obj",
															 "STL files (*.stl)", "*.stl", "All files (*.*)", "*.*"},
									pfd::opt::none).result();
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
	static std::vector<const char*> names;
	names.clear();
	for (const auto& name : remesher_names) names.push_back(name.c_str());
	if (ImGui::Combo("##remesher_strategy", (int*)&r_type, names.data(), (int)names.size())) reset();

	// Remeshing creates a new thread so ui doesnt freeze
	ImGui::SameLine();
	if (ImGui::Button("Remesh")) {
		logger = std::make_unique<io::Logger>(plotter->log_path(plot_config(), {r_type, r_ctx.split, r_ctx.flip}));

		p_ctx.update([](ProgressState& s) {
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
			p_ctx.update([](ProgressState& s) { s.is_remeshing = false; });
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
		auto* phase_remesher = dynamic_cast<PhaseBasedRemesher*>(remesher.get());
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
	static std::vector<const char*> names;
	names.clear();
	for (const auto& name : split_mode_names) names.push_back(name.c_str());
	if (ImGui::Combo("Split Strategy##strategy", (int*)&r_ctx.split, names.data(), (int)names.size())) reset();

	names.clear();
	for (const auto& name : flip_mode_names) names.push_back(name.c_str());
	if (ImGui::Combo("Flip Strategy##flip_strategy", (int*)&r_ctx.flip, names.data(), (int)names.size())) reset();
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
	if (ImGui::Checkbox("Permanent Plots/Logs", &permanent_outputs)) {
		if (permanent_outputs) {
			default_export_dir_name = current_file_name + "_" + timestamp_string();
			export_rename_error.clear();
			export_rename_input.fill('\0');
			ImGui::OpenPopup("Create Export Folder");
		}
		else export_dir.clear();
	}
	ImGui::BeginDisabled(plotter->is_running());
	if (ImGui::Button("Plot Manual Runs")) {
		plotter->plot(io::PlotMode::MANUAL, plot_config());
	}
	ImGui::SameLine();
	if (ImGui::Button("Run All Strategies & Plot")) {
		plotter->plot(io::PlotMode::ALL, plot_config());
	}
	ImGui::SameLine();
	if (ImGui::Button("Run Preset & Plot")) {
		plotter->plot(io::PlotMode::PRESET, plot_config());
	}
	ImGui::EndDisabled();
	const std::string status = plotter->get_status();
	if (!status.empty()) {
		ImGui::TextWrapped("%s", status.c_str());
	}
}

void AppViewer::condition_updates() {
	// Popup while remeshing
	if (ImGui::BeginPopupModal("Remeshing Progress", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Remeshing in progress...");
		ProgressState state = p_ctx.load();
		QueueStats q = state.queue_stats;
		float progress = state.is_remeshing ? 0.0f : 1.0f;
		if (r_type == RemesherType::PRIORITY_GLOBAL) {
			if (q.popped > 0) progress = static_cast<float>(q.popped) / static_cast<float>(q.popped + q.queued);
		} else {
			progress = static_cast<float>(state.current_iteration) / static_cast<float>(r_ctx.iterations);
		}
		ImGui::ProgressBar(progress, ImVec2(350.0f, 0.0f));
		ImGui::Text("Operations: %d", state.metrics.operations);
		ImGui::Text("Total Edge Loss: %.4f", state.metrics.total_edge_loss);
		if (r_type == RemesherType::PRIORITY_GLOBAL) {
			ImGui::Text("Approximate workload: %d processed + %d queued", q.popped, q.queued);
			ImGui::Text("Queue: %d stale, %d rejected, %d rebuilds", q.stale, q.rejected, q.rebuilt);
			ImGui::Text("Termination: %s", to_string(state.termination_reason).c_str());
		}
		if (!state.is_remeshing) {
			draw_surface_mesh(mesh, r_ctx.target_length, loss_map_range);
			if (ImGui::Button("OK", ImVec2(350.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Create Export Folder", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Current folder:");
		ImGui::TextWrapped("%s", default_export_dir_name.c_str());
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::InputTextWithHint("##export_create", "Leave empty to keep current name", export_rename_input.data(), export_rename_input.size());
		if (!export_rename_error.empty()) {
			ImGui::TextColored(ImVec4(0.85f, 0.35f, 0.35f, 1.0f), "%s", export_rename_error.c_str());
		}
		ImGui::TextDisabled("Enter accepts, Escape cancels.");

		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			permanent_outputs = false;
			export_dir.clear();
			export_rename_error.clear();
			ImGui::CloseCurrentPopup();
		} else if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
			const std::string requested_name = trim_copy(export_rename_input.data());
			const std::string folder_name = requested_name.empty() ? default_export_dir_name : requested_name;
			if (folder_name.find('/') != std::string::npos ||
					   folder_name.find('\\') != std::string::npos ||
					   folder_name == "." || folder_name == "..") {
				export_rename_error = "Please enter a valid folder name.";
			} else {
				const std::string folder_path = OUT_EXPORT_DIR + folder_name;
				if (std::filesystem::exists(folder_path)) {
					export_rename_error = "A folder with that name already exists.";
				} else {
					try {
						std::filesystem::create_directories(folder_path);
						export_dir = folder_path;
						export_rename_error.clear();
						ImGui::CloseCurrentPopup();
					} catch (const std::filesystem::filesystem_error&) {
						export_rename_error = "Could not create the export folder.";
					}
				}
			}
		}

		ImGui::EndPopup();
	}
}

} // namespace ba::ui
