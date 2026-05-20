#include "ui/app_viewer.h"

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

ps::SurfaceMesh* AppViewer::update_polyscope() {
    mesh.garbage_collection();

    //Convert to Polyscope format
    std::vector<glm::vec3> vertices;
    vertices.reserve(mesh.n_vertices());
    for(auto v : mesh.vertices()) {
        auto p = mesh.position(v);
        vertices.push_back(glm::vec3(p[0], p[1], p[2]));
    }

    std::vector<std::vector<size_t>> faces;
    faces.reserve(mesh.n_faces());
    for(auto f : mesh.faces()) {
        std::vector<size_t> face_indices;
        face_indices.reserve(mesh.valence(f));
        for(auto v : mesh.vertices(f)) {
            face_indices.push_back(v.idx());
        }
        faces.push_back(std::move(face_indices));
    }

    mesh_ps = polyscope::registerSurfaceMesh("mesh", vertices, faces);
    
    if (show_vertex_loss) {
        add_vertex_loss();
        has_vertex_loss = true;
    }
    return mesh_ps;
}

void AppViewer::load_mesh(const std::string& filepath) {
    mesh.clear();
    pmp::read(mesh, filepath);
    remesher = std::make_unique<ba::Remesher>(mesh);
    has_vertex_loss = false; 
    update_polyscope()->setEdgeWidth(1.0);
    polyscope::view::resetCameraToHomeView();
}

void AppViewer::add_vertex_loss() {
    if (remesher && mesh_ps) {
        std::vector<double> vertex_losses = loss::get_vertex_losses(mesh, remesher->get_target_length());
        std::vector<double> sorted_losses = vertex_losses;
        size_t idx = std::min(sorted_losses.size() - 1, (size_t)(sorted_losses.size() * 0.90));
        std::nth_element(sorted_losses.begin(), sorted_losses.begin() + idx, sorted_losses.end());
        
        double min_val = *std::min_element(vertex_losses.begin(), vertex_losses.end());
        double max_val = sorted_losses[idx];

        mesh_ps->addVertexScalarQuantity("Edge Loss", vertex_losses)
               ->setColorMap("coolwarm")
               ->setMapRange({min_val, max_val}) 
               ->setEnabled(true);
    }
}

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

    polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::ShadowOnly;
    polyscope::options::shadowBlurIters = 6;
    polyscope::init();
    if (!file_paths.empty()) {
        load_mesh(file_paths[0]);
    }
    polyscope::state::userCallback = [this]() { this->draw_ui(); };
}

void AppViewer::draw_ui() {
    ImGui::BeginDisabled(is_remeshing);
    draw_mesh_control();
    ImGui::Separator(); 
    ImGui::BeginDisabled(!remesher);
        draw_remesh_control();
        ImGui::Separator();
        draw_visualization_control();
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
    if (ImGui::Combo("##mesh_selector", &selected_mesh, names.data(), (int)names.size())) {
        load_mesh(file_paths[selected_mesh]);
    }
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
        if (!paths.empty()) load_mesh(paths[0]);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Mesh")) {
        load_mesh(file_paths[selected_mesh]);
    }
}

void AppViewer::draw_remesh_control() {
    ImGui::Text("Remeshing:");
    if (ImGui::Button("Iterate")) {
        remesher->single_iteration();
        update_polyscope(); 
    }
    ImGui::SameLine();
    
    // Remeshing creates a new thread so ui doesnt freeze
    if (ImGui::Button("Remesh")) {
        if(export_vtk) io::remove_vtks("../../out/vtk");
        is_remeshing = true;
        current_progress_iter = 0;
        total_progress_iters = (run_until_converged ? 100 : remesher->get_iterations());
        
        remesher->set_progress_callback([this](int cur, int total, double loss) {
            current_progress_iter = cur;
            total_progress_iters = total;
            current_progress_loss = loss;
            if (export_vtk) {
                std::vector<double> vertex_losses = loss::get_vertex_losses(mesh, remesher->get_target_length());
                if(!vertex_losses.empty()) {
                    std::ostringstream s;
                    s << "../../out/vtk/exported_mesh_" << std::setfill('0') << std::setw(4) 
                      << current_progress_iter << ".vtk";
                    io::export_mesh_vtk(s.str(), mesh, vertex_losses);
                }
            }
        });
        
        std::thread([this]() {
            remesher->remesh(log_csv, run_until_converged);
            is_remeshing = false;
            remesh_finished = true;
        }).detach();
        
        ImGui::OpenPopup("Remeshing Progress");
    }
    
    ImGui::BeginDisabled(run_until_converged);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(25.0);
    int iterations_input = remesher->get_iterations();
    if (ImGui::InputInt("# Iterations", &iterations_input, 0)) {
        remesher->set_iterations(iterations_input);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Converge", &run_until_converged);
    ImGui::SameLine();
    ImGui::Checkbox("CSV", &log_csv);
    ImGui::SameLine();
    ImGui::Checkbox("VTK", &export_vtk);

    ImGui::Text("Operations:");
    if (ImGui::Button("Split")) {
        remesher->split_long_edges();
        update_polyscope(); 
    }
    ImGui::SameLine();
    if (ImGui::Button("Collapse")) {
        remesher->collapse_short_edges();
        update_polyscope(); 
    }
    ImGui::SameLine();
    if (ImGui::Button("Flip")) {
        remesher->flip_edges();
        update_polyscope(); 
    }
    ImGui::SameLine();
    if (ImGui::Button("Smooth")) {
        remesher->smooth_vertices();
        update_polyscope(); 
    }
}

void AppViewer::draw_visualization_control() {
    ImGui::Text("Visualisation:");
    ImGui::Checkbox("Show Vertex Loss", &show_vertex_loss);
    ImGui::SameLine();
    if (ImGui::Button("Export Mesh to .vtk")) {
        std::vector<double> vertex_losses = loss::get_vertex_losses(mesh, remesher->get_target_length());
        if(!vertex_losses.empty()) {
            io::export_mesh_vtk("../../out/vtk/exported_mesh.vtk", mesh, vertex_losses);
        }
    }
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
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (remesh_finished) {
        update_polyscope();
        remesh_finished = false;
    }

    // Update Vertex Loss Quantity based on UI Selection
    if (show_vertex_loss && !has_vertex_loss) {
        add_vertex_loss();
        has_vertex_loss = true;
    } else if (!show_vertex_loss && has_vertex_loss) {
        mesh_ps->getQuantity("Edge Loss")->setEnabled(false);
        mesh_ps->removeQuantity("Edge Loss");
        has_vertex_loss = false;
    }
}


} // namespace ba::ui