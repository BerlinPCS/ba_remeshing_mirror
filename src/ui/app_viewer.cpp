#include "ui/app_viewer.h"

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


    ImGui::Separator(); 
    ImGui::Text("Single Operations:");
    if (ImGui::Button("Split Long Edges")) {
        remesher->split_long_edges();
        update_polyscope(); 
    }
    ImGui::SameLine();
    if (ImGui::Button("Collapse Short Edges")) {
        remesher->collapse_short_edges();
        update_polyscope(); 
    }
    ImGui::SameLine();
    if (ImGui::Button("Flip Edges")) {
        remesher->flip_edges();
        update_polyscope(); 
    }
    ImGui::SameLine();
    if (ImGui::Button("Smooth Vertices")) {
        remesher->smooth_vertices();
        update_polyscope(); 
    }


    ImGui::Separator();
    ImGui::Text("Remeshing:");
    if (ImGui::Button("Run 1 Iteration")) {
        remesher->single_iteration();
        update_polyscope(); 
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Remesh") && remesher) {
        is_remeshing = true;
        current_progress_iter = 0;
        total_progress_iters = (remesher->get_run_until_converged() ? 100 : remesher->get_iterations() + 1);
        
        remesher->set_progress_callback([this](int cur, int total, double loss) {
            current_progress_iter = cur;
            total_progress_iters = total;
            current_progress_loss = loss;
        });
        
        std::thread([this]() {
            remesher->remesh();
            is_remeshing = false;
            remesh_finished = true;
        }).detach();
        
        ImGui::OpenPopup("Remeshing Progress");
    }
    
    ImGui::SameLine();
    ImGui::Text("Iter.:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0);
    if (remesher) {
        int iterations_input = remesher->get_iterations();
        if (ImGui::InputInt("##", &iterations_input)) {
            remesher->set_iterations(iterations_input);
        }
    }
    ImGui::SameLine();
    bool run_until_converged_input = remesher->get_run_until_converged();
    ImGui::Checkbox("Converge", &run_until_converged_input);
    if(run_until_converged_input != remesher->get_run_until_converged()) {
        remesher->set_run_until_converged(run_until_converged_input);
    }
    ImGui::SameLine();
    bool log_metrics_input = remesher->get_log_metrics();
    ImGui::Checkbox("Log", &log_metrics_input);
    if (log_metrics_input != remesher->get_log_metrics()) {
        remesher->set_log_metrics(log_metrics_input);
    }

    ImGui::Separator();
    ImGui::Text("Visualisation:");
    ImGui::Checkbox("Show Vertex Loss", &show_vertex_loss);
    if (show_vertex_loss && !has_vertex_loss) {
        add_vertex_loss();
        has_vertex_loss = true;
    } else if (!show_vertex_loss && has_vertex_loss) {
        mesh_ps->removeQuantity("Edge Loss");
        has_vertex_loss = false;
    }
    ImGui::EndDisabled();
    
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
}

void AppViewer::add_vertex_loss() {
    if (remesher && mesh_ps) {
        std::vector<double> vertex_losses(mesh.n_vertices(), 0.0);

        for(auto v : mesh.vertices()) {            
            for(auto h : mesh.halfedges(v)) {
                pmp::Edge e = mesh.edge(h);
                vertex_losses[v.idx()] += remesher->get_edge_loss(e);
            }
            vertex_losses[v.idx()] /= mesh.valence(v);
        }
        mesh_ps->addVertexScalarQuantity("Edge Loss", vertex_losses)->setColorMap("coolwarm");
    }
}

} // namespace ba::ui