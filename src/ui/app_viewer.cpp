#include "ui/app_viewer.h"
#include "core/types.h"
#include "imgui.h"
#include "ui/visuals.h"
#include "io/paraview.h"
#include "remesher/loss.h"

#include <sstream>
#include <filesystem>
#include <thread>
#include <portable-file-dialogs.h>

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
    if (!file_paths.empty()) reset();
    else polyscope::error("No valid mesh files found.");
    polyscope::state::userCallback = [this]() { this->draw_ui(); };
}

void AppViewer::reset(const std::string& file_path){
    // Reset State
    l_ctx.logs = 0;
    p_ctx.store(ProgressState());
    io::remove_vtks();
    mesh.clear();

    // Load mesh and compute remeshing parameters
    const std::string source_path = file_path.empty() ? file_paths[selected_mesh] : file_path;
    pmp::read(mesh, source_path);
    r_ctx.target_length = avg_edge_length(mesh);
    r_ctx.log_frequency = std::max(1, (int)mesh.n_edges() / 8);
    r_ctx.progress_frequency = std::max(1, (int)mesh.n_edges() / 16);

    if (r_type == RemesherType::PRIORITY_LOCAL) {
        remesher = std::make_unique<RemesherPrioLocal>(mesh, r_ctx, p_ctx);
    } else if (r_type == RemesherType::PRIORITY_GLOBAL) {
        remesher = std::make_unique<RemesherPrioGlobal>(mesh, r_ctx, p_ctx);
    }  else {
        remesher = std::make_unique<RemesherStandard>(mesh, r_ctx, p_ctx);
    }


    // Draw Mesh
    draw_surface_mesh(mesh, r_ctx.target_length)->setEdgeWidth(1.0)
                    ->getQuantity("Edge Loss")->setEnabled(show_vertex_loss);
    polyscope::view::resetCameraToHomeView();

    // Log initial values
    current_file_name = std::filesystem::path(source_path).stem().string();
    std::stringstream log_path;
    log_path << OUT_LOG_DIR << current_file_name << "_"
             << remesher_names[static_cast<int>(r_type)] << "_"
             << split_mode_names[static_cast<int>(r_ctx.split)] << "_"
             << flip_mode_names[static_cast<int>(r_ctx.flip)] << ".csv";
    logger = std::make_unique<io::Logger>(log_path.str());
    log(true);
}

void AppViewer::log(bool initial_log) {
    ProgressState state = p_ctx.load();
    if (initial_log || l_ctx.logging) {
        if (logger) logger->log_iteration(state);
        if (l_ctx.vtk_export) {
            io::export_mesh_vtk(current_file_name, mesh, loss::get_vertex_losses(mesh, r_ctx.target_length), l_ctx.logs);
        }
        l_ctx.logs++;
    }
}

void AppViewer::draw_ui() {
    ImGui::BeginDisabled(p_ctx.load().is_remeshing);
        draw_mesh_control();
        ImGui::BeginDisabled(!remesher);
            draw_remesh_control();
            if(r_type != RemesherType::BASE) draw_prio_control();
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
        auto paths = pfd::open_file(
            "Load Mesh", "", std::vector<std::string>{
                "Mesh files (*.off *.obj *.stl)", "*.off;*.obj;*.stl",
                "OFF files (*.off)", "*.off",
                "OBJ files (*.obj)", "*.obj",
                "STL files (*.stl)", "*.stl",
                "All files (*.*)", "*.*"
            }, pfd::opt::none
        ).result();
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
    if(ImGui::Combo("##remesher_strategy", (int*)&r_type, names.data(), (int)names.size())) reset();
    
    // Remeshing creates a new thread so ui doesnt freeze
    ImGui::SameLine();
    if (ImGui::Button("Remesh")) {
        p_ctx.update([](ProgressState& s) {
            s = ProgressState();
            s.is_remeshing = true;
        });
        
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
    if (ImGui::InputInt("Log Freq", &r_ctx.log_frequency, 0)) {}
    ImGui::EndDisabled(); // !logging
    if(r_type == RemesherType::BASE) {
        ImGui::SetNextItemWidth(250.0);
        if (ImGui::SliderInt("Iterations", &r_ctx.iterations, 0, 100, "%d")) {}
    }

    if(!l_ctx.logging) {
        auto* phase_remesher = dynamic_cast<PhaseBasedRemesher*>(remesher.get());
        if (!phase_remesher) return;
        ImGui::NewLine();
        ImGui::Text("Single Operations (Debug):");
        ImGui::Separator();
        if (ImGui::Button("Split")) {
            phase_remesher->run_debug_phase(OpType::Split);
            draw_surface_mesh(mesh, r_ctx.target_length); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Collapse")) {
            phase_remesher->run_debug_phase(OpType::Collapse);
            draw_surface_mesh(mesh, r_ctx.target_length); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Flip")) {
            phase_remesher->run_debug_phase(OpType::Flip);
            draw_surface_mesh(mesh, r_ctx.target_length); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Smooth")) {
            phase_remesher->run_debug_phase(OpType::Smooth);
            draw_surface_mesh(mesh, r_ctx.target_length); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Iterate")) {
            remesher->single_iteration();
            draw_surface_mesh(mesh, r_ctx.target_length); 
        }
    }
}

void AppViewer::draw_prio_control() {
    ImGui::NewLine();
    ImGui::Text("Priority-Based Remeshing:");
    ImGui::Separator();
    static std::vector<const char*> names;
    names.clear();
    for (const auto& name : split_mode_names) names.push_back(name.c_str());
    ImGui::SetNextItemWidth(180.0f);
    if(ImGui::Combo("Split Strategy##strategy", (int*)&r_ctx.split, names.data(), (int)names.size())) reset();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if(ImGui::SliderFloat("Convergance Threshold", &r_ctx.op_gain_threshold, 1e-6f, 1e-2f, "%.6f", ImGuiSliderFlags_Logarithmic)) {}
    
    if(r_type == RemesherType::PRIORITY_GLOBAL) {
        names.clear();
        for (const auto& name : flip_mode_names) names.push_back(name.c_str());
        ImGui::SetNextItemWidth(180.0f);
        if(ImGui::Combo("Flip Strategy##flip_strategy", (int*)&r_ctx.flip, names.data(), (int)names.size())) reset();
        if (r_ctx.flip == FlipMode::VALENCE) {
            ImGui::SameLine();
            ImGui::TextDisabled("(heuristic)");
            ImGui::SetNextItemWidth(120.0f);
            if(ImGui::SliderInt("Flip Frequency", &r_ctx.flip_frequency, 1, 10)) {}
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            if(ImGui::InputInt("Operation Limit (0 = auto)", &r_ctx.operation_limit, 0)) {
                r_ctx.operation_limit = std::max(0, r_ctx.operation_limit);
            }
        }
    }
}

void AppViewer::draw_visualization_control() {
    ImGui::NewLine();
    ImGui::Text("Miscellaneous:");
    ImGui::Separator();
    if(ImGui::Checkbox("Show Vertex Loss", &show_vertex_loss)) {
        if(polyscope::hasSurfaceMesh("Mesh")) {
            polyscope::getSurfaceMesh("Mesh")->getQuantity("Edge Loss")->setEnabled(show_vertex_loss);
        }
    }
    /*
    ImGui::SameLine();
    if (ImGui::Button("Export Mesh to .vtk")) {
        std::string safe_name = std::filesystem::path(file_paths[selected_mesh]).stem().string();
        io::export_mesh_vtk(safe_name, mesh, loss::get_vertex_losses(mesh, remesher->get_target_length()));
    }
    */
    ImGui::Checkbox("Export VTK", &l_ctx.vtk_export);
    /*if (ImGui::Button("Run All Benchmarks & Plot")) {
        reset();
        Mesh benchmark_mesh = mesh; 
        
        std::thread([this, benchmark_mesh]() {
            run_benchmarks_headless(benchmark_mesh);
        }).detach();
    }*/
}

void AppViewer::condition_updates(){
    // Popup while remeshing
    if (ImGui::BeginPopupModal("Remeshing Progress", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Remeshing in progress...");
        ProgressState state = p_ctx.load();
        const int processed = r_type == RemesherType::PRIORITY_GLOBAL
            ? state.processed_candidates
            : state.metrics.operations;
        const int workload = processed + state.current_queue_size;
        float progress = workload > 0
            ? static_cast<float>(processed) / static_cast<float>(workload)
            : (state.is_remeshing ? 0.0f : 1.0f);
        if (r_type == RemesherType::BASE && state.total_iterations > 0) {
            progress = static_cast<float>(state.current_iteration) / static_cast<float>(state.total_iterations);
        }
        ImGui::ProgressBar(progress, ImVec2(350.0f, 0.0f));
        ImGui::Text("Approximate workload: %d processed + %d queued",
                    processed, state.current_queue_size);
        ImGui::Text("Operations: %d", state.metrics.operations);
        ImGui::Text("Total Edge Loss: %.4f", state.metrics.total_edge_loss);
        if(r_type == RemesherType::PRIORITY_GLOBAL) {
            ImGui::Text("Queue: %d stale, %d rejected, %d rebuilds",
                        state.queue_stats.stale, state.queue_stats.rejected, state.queue_stats.rebuilt);
            ImGui::Text("Termination: %s", termination_reason_name(state.termination_reason));
        }
        if (!state.is_remeshing) {
            draw_surface_mesh(mesh, r_ctx.target_length);
            if(ImGui::Button("OK", ImVec2(350.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}


} // namespace ba::ui
