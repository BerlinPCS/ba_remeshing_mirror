#include "ui/app_viewer.h"
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
    current_total_iters = 0;
    is_remeshing = false;
    current_progress_iter = 0;
    total_progress_iters = 100;
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
        remesher = std::make_unique<RemesherPrioGlobal>(mesh, evaluator);
    }  else {
        remesher = std::make_unique<RemesherStandard>(mesh, evaluator);
    }

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

void AppViewer::log(IterationMetrics met, bool initial_log) {
    metrics = met;
    metrics.iteration_num = current_total_iters;
    if (initial_log || logging) {
        if (logger) logger->log_iteration(metrics);
        if (vtk_export) {
            io::export_mesh_vtk(current_file_name, mesh, loss::get_vertex_losses(mesh, remesher->get_target_length()), current_total_iters);
        }
    }
    current_total_iters.fetch_add(1);
}

void AppViewer::draw_ui() {
    ImGui::BeginDisabled(is_remeshing);
        draw_mesh_control();
        ImGui::Separator(); 
        ImGui::BeginDisabled(!remesher);
            draw_remesh_control();
            ImGui::Separator();
            if (ImGui::TreeNode("Visualization:")) {
                draw_visualization_control();
                ImGui::TreePop();
            }
        ImGui::EndDisabled(); // !remesher
    ImGui::EndDisabled(); // is_remeshing
    condition_updates();
}

void AppViewer::draw_mesh_control() {
    ImGui::Text("Meshes:");
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
    ImGui::Text("Remeshing:");
    ImGui::SetNextItemWidth(180.0f);
    static std::vector<const char*> names;
    names.clear();
    for (const auto& name : remesher_names) names.push_back(name.c_str());
    if(ImGui::Combo("##remesher_strategy", &remesher_type, names.data(), (int)names.size())) reset();

    if (remesher_type == PRIORITY_GLOBAL) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if(ImGui::SliderInt("Flip Frequency", &flip_frequency, 1, 10)) {
            static_cast<ba::RemesherPrioGlobal*>(remesher.get())->set_flip_frequency(flip_frequency);
        }
    }
    if(remesher_type != BASE) {
        names.clear();
        for (const auto& name : strategy_names) names.push_back(name.c_str());
        ImGui::SetNextItemWidth(180.0f);
        if(ImGui::Combo("##strategy", &split_strategy, names.data(), (int)names.size())) reset();
    }

    if (ImGui::Button("Iterate")) {
        remesher->timed_iteration();
        log(remesher->get_metrics());
        draw_surface_mesh(mesh, remesher->get_target_length()); 
    }
    ImGui::SameLine();
    
    // Remeshing creates a new thread so ui doesnt freeze
    if (ImGui::Button("Remesh")) {
        is_remeshing = true;
        current_progress_iter = 0;
        total_progress_iters = (run_until_converged ? 100 : iterations);
        
        remesher->set_progress_callback([this](int cur, IterationMetrics met) {
            current_progress_iter = cur;
            current_progress_loss = met.total_edge_loss;
            log(met);
        });
        
        std::thread([this]() {
            remesher->remesh(run_until_converged);
            is_remeshing = false;
        }).detach();

        ImGui::OpenPopup("Remeshing Progress");
    }
    
    ImGui::BeginDisabled(run_until_converged);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(25.0);
    if (ImGui::InputInt("# Iterations", &iterations, 0)) {
        remesher->set_iterations(iterations);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Converge", &run_until_converged);
    ImGui::SameLine();
    ImGui::Checkbox("Log Data", &logging);

    ImGui::BeginDisabled(logging || remesher_type == PRIORITY_GLOBAL);
    ImGui::Text("Operations:");
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
    ImGui::EndDisabled(); // logging
}

void AppViewer::draw_visualization_control() {
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
        int cur = current_progress_iter.load();
        int total = total_progress_iters.load();
        float progress = total > 0 ? (float)cur / (float)total : 0.0f;
        ImGui::ProgressBar(progress, ImVec2(250.0f, 0.0f));
        ImGui::Text("Iteration: %d / %d", cur, total);
        ImGui::Text("Total Edge Loss: %.4f", current_progress_loss.load());
        if (!is_remeshing) {
            draw_surface_mesh(mesh, remesher->get_target_length());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}


} // namespace ba::ui