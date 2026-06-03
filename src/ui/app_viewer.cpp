#include "ui/app_viewer.h"
#include "core/types.h"
#include "imgui.h"
#include "remesher/evaluation_strategy.h"
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
    for (auto& file : std::filesystem::recursive_directory_iterator(path_to_data)) {
        if (file.is_regular_file()) {
            auto ext = file.path().extension().string();
            if (ext == ".off" || ext == ".obj" || ext == ".stl") {
                file_paths.push_back(file.path().string());
                mesh_names.push_back(format_file_name(file.path().stem().string()));
            }
        }
    }

    ps::options::groundPlaneMode = ps::GroundPlaneMode::ShadowOnly;
    ps::options::shadowBlurIters = 6;
    ps::init();
    if (!file_paths.empty()) reset();
    else ps::error("No valid mesh files found.");
    ps::state::userCallback = [this]() { this->draw_ui(); };
}

void AppViewer::reset(const std::string& file_path){
    // Reset State
    is_remeshing = false;
    current_progress_ops = 0;
    current_queue_size = 0;
    logs = 0;
    io::remove_vtks();
    mesh.clear();

    // Create the correct Remesher
    pmp::read(mesh, file_path.empty() ? file_paths[selected_mesh] : file_path);

    std::shared_ptr<EvaluationStrategy> evaluator;
    if (split_strategy == SPLIT_SUM) {
        evaluator = std::make_shared<SplitSumEvaluation>();
    } else if (split_strategy == SPLIT_MAX) {
        evaluator = std::make_shared<SplitMaxEvaluation>();
    } else if (split_strategy == SPLIT_AVG) {
        evaluator = std::make_shared<SplitAvgEvaluation>();
    }
    if (remesher_type == PRIORITY_LOCAL) {
        remesher = std::make_unique<RemesherPrioLocal>(mesh, evaluator);
    } else if (remesher_type == PRIORITY_GLOBAL) {
        RemesherPrioGlobal global = RemesherPrioGlobal(mesh, evaluator);
        global.set_flip_frequency(flip_frequency);
        global.set_separate_flip_queue(separate_flip_queue);
        remesher = std::make_unique<RemesherPrioGlobal>(global);

    }  else {
        remesher = std::make_unique<RemesherStandard>(mesh, evaluator);
    }

    remesher->set_op_gain_threshold((double)op_gain_threshold);
    if (log_frequency != 0) remesher->set_log_frequency(log_frequency);

    // Draw Mesh
    draw_surface_mesh(mesh, remesher->get_target_length())->setEdgeWidth(1.0)
                    ->getQuantity("Edge Loss")->setEnabled(show_vertex_loss);
    ps::view::resetCameraToHomeView();

    // Log initial values
    current_file_name = std::filesystem::path(file_paths[selected_mesh]).stem().string();
    std::stringstream log_path;
    log_path << OUT_LOG_DIR << "results_" << remesher_names[remesher_type] << "_" << current_file_name << ".csv";
    logger = std::make_unique<io::Logger>(log_path.str());
    log(remesher->get_metrics(), true);
}

void AppViewer::log(Metrics met, bool initial_log) {
    metrics = met;
    if (initial_log || logging) {
        if (logger) logger->log_iteration(metrics);
        if (vtk_export) {
            io::export_mesh_vtk(current_file_name, mesh, loss::get_vertex_losses(mesh, remesher->get_target_length()), logs);
        }
        logs++;
    }
}

void AppViewer::draw_ui() {
    ImGui::BeginDisabled(is_remeshing);
        draw_mesh_control();
        ImGui::BeginDisabled(!remesher);
            draw_remesh_control();
            if(remesher_type != BASE) draw_prio_control();
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
    if(ImGui::Combo("##remesher_strategy", &remesher_type, names.data(), (int)names.size())) reset();
    
    // Remeshing creates a new thread so ui doesnt freeze
    ImGui::SameLine();
    if (ImGui::Button("Remesh")) {
        is_remeshing = true;
        current_progress_ops = 0;
        current_queue_size = 0;
        
        remesher->set_progress_callback([this](int queue_size, Metrics met, bool is_log_tick) {
            current_progress_ops = met.operations;
            current_queue_size = queue_size;
            current_progress_loss = met.total_edge_loss;
            if (is_log_tick) log(met, false);
        });
        
        std::thread([this]() {
            remesher->remesh();
            is_remeshing = false;
        }).detach();

        ImGui::OpenPopup("Remeshing Progress");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Log Data", &logging);
    ImGui::SameLine();
    ImGui::BeginDisabled(!logging);
    ImGui::SetNextItemWidth(60.0);
    int true_log_freq = remesher->get_log_frequency();
    if (ImGui::InputInt("Log Freq", &true_log_freq, 0)) {
        remesher->set_log_frequency(true_log_freq);
        log_frequency = true_log_freq;
    }
    ImGui::EndDisabled(); // !logging
    if(remesher_type == BASE) {
        ImGui::SetNextItemWidth(250.0);
        if (ImGui::SliderInt("Iterations", &iterations, 0, 100, "%d")) {
            remesher->set_iterations(iterations);
        }
    }

    if(!logging && remesher_type == BASE) {
        ImGui::NewLine();
        ImGui::Text("Single Operations (Debug):");
        ImGui::Separator();
        if (ImGui::Button("Split")) {
            remesher->split_long_edges();
            draw_surface_mesh(mesh, remesher->get_target_length()); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Collapse")) {
            remesher->collapse_short_edges();
            draw_surface_mesh(mesh, remesher->get_target_length()); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Flip")) {
            remesher->flip_edges();
            draw_surface_mesh(mesh, remesher->get_target_length()); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Smooth")) {
            remesher->smooth_vertices();
            draw_surface_mesh(mesh, remesher->get_target_length()); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Iterate")) {
            remesher->single_iteration();
            draw_surface_mesh(mesh, remesher->get_target_length()); 
        }
    }
}

void AppViewer::draw_prio_control() {
    ImGui::NewLine();
    ImGui::Text("Priority-Based Remeshing:");
    ImGui::Separator();
    static std::vector<const char*> names;
    names.clear();
    for (const auto& name : strategy_names) names.push_back(name.c_str());
    ImGui::SetNextItemWidth(180.0f);
    if(ImGui::Combo("##strategy", &split_strategy, names.data(), (int)names.size())) reset();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if(ImGui::SliderFloat("Convergance Threshold", &op_gain_threshold, 1e-6f, 1e-2f, "%.6f", ImGuiSliderFlags_Logarithmic)) {
        remesher->set_op_gain_threshold((double)op_gain_threshold);
    }
    if(remesher_type == PRIORITY_GLOBAL) {
        ImGui::SetNextItemWidth(120.0f);
        if(ImGui::SliderInt("Flip Frequency", &flip_frequency, 1, 10)) {
            static_cast<ba::RemesherPrioGlobal*>(remesher.get())->set_flip_frequency(flip_frequency);
        }
        ImGui::SameLine();
        if(ImGui::Checkbox("Separate Flip Queue", &separate_flip_queue)) {
            static_cast<ba::RemesherPrioGlobal*>(remesher.get())->set_separate_flip_queue(separate_flip_queue);
        }
    }
}

void AppViewer::draw_visualization_control() {
    ImGui::NewLine();
    ImGui::Text("Visualization:");
    ImGui::Separator();
    if(ImGui::Checkbox("Show Vertex Loss", &show_vertex_loss)) {
        if(ps::hasSurfaceMesh("Mesh")) {
            ps::getSurfaceMesh("Mesh")->getQuantity("Edge Loss")->setEnabled(show_vertex_loss);
        }
    }
    /*
    ImGui::SameLine();
    if (ImGui::Button("Export Mesh to .vtk")) {
        std::string safe_name = std::filesystem::path(file_paths[selected_mesh]).stem().string();
        io::export_mesh_vtk(safe_name, mesh, loss::get_vertex_losses(mesh, remesher->get_target_length()));
    }
    */
    ImGui::Checkbox("Export VTK", &vtk_export);
}

void AppViewer::condition_updates(){
    // Popup while remeshing
    if (ImGui::BeginPopupModal("Remeshing Progress", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Remeshing in progress...");
        int cur_ops = current_progress_ops.load();
        int q_size = current_queue_size.load();
        float progress = q_size > 0 ? std::min(1.0f, (float)cur_ops / (float)q_size) : 0.0f;
        ImGui::ProgressBar(progress, ImVec2(250.0f, 0.0f));
        ImGui::Text("Operations: %d / ~%d", cur_ops, q_size);
        ImGui::Text("Total Edge Loss: %.4f", current_progress_loss.load());
        if (!is_remeshing) {
            draw_surface_mesh(mesh, remesher->get_target_length());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}


} // namespace ba::ui