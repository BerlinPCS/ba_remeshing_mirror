#include <iostream>
#include <memory>

#include <pmp/surface_mesh.h>
#include <pmp/io/io.h>
#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

#include "remesher.h"

/**
 * \brief Loads the given pmp::SurfaceMesh into Polyscope.
 * Converts the halfedge data structure to the format expected by Polyscope.
 * 
 * \param mesh The mesh to be loaded into Polyscope
 * \param mesh_name The name to be given to the mesh in Polyscope
 */
polyscope::SurfaceMesh* update_polyscope(pmp::SurfaceMesh& mesh, std::string mesh_name = "mesh") {
    mesh.garbage_collection();

    //Convert to Polyscope format
    std::vector<glm::vec3> vertices;
    for (auto v : mesh.vertices()) {
        auto p = mesh.position(v);
        vertices.push_back(glm::vec3(p[0], p[1], p[2]));
    }

    std::vector<std::vector<size_t>> faces;
    for (auto f : mesh.faces()) {
        std::vector<size_t> face_indices;
        for (auto v : mesh.vertices(f)) {
            face_indices.push_back(v.idx());
        }
        faces.push_back(face_indices);
    }

    return polyscope::registerSurfaceMesh(mesh_name, vertices, faces);
}

int main() {
    //Options
    polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::ShadowOnly;
    polyscope::options::shadowBlurIters = 6;

    //Initialize Polyscope
    polyscope::init();

    //Create Remesher.
    std::unique_ptr<ba::Remesher> remesher;

    //Load Mesh
    pmp::SurfaceMesh mesh;
    std::vector<std::string> mesh_files = {
        "data/stanford-bunny.obj",
        "data/stanford-bunny-low.obj",
        "data/test/test_split.obj",
        "data/test/test_collapse.obj",
        "data/test/test_flip.obj",
        "data/test/test_smooth.obj"
    };
    std::vector<const char*> mesh_names = {
        "High Quality Bunny",
        "Low Quality Bunny",
        "Test Split",
        "Test Collapse",
        "Test Flip",
        "Test Smooth"
    };
    int selected_mesh = 0;

    /**
     * \brief Lambda Function to load a mesh based on a .obj, .stl, or .off file. 
     * Creates a new remesher instance for the loaded mesh.
     * 
     * \param filepath The path to the mesh file.
     * \param length The target edge length for remeshing. Optional, if not included, uses average mesh edge length.
     * \param iterations The number of iterations for remeshing. Optional, if not included, bases iterations off loss.
     */
    auto load_mesh = [&](const std::string& filepath, double length = 0, int iterations = 0) {
        mesh.clear();
        pmp::read(mesh, filepath);
        
        if(length && iterations) remesher = std::make_unique<ba::Remesher>(mesh, length, iterations);
        else if (length) remesher = std::make_unique<ba::Remesher>(mesh, length);
        else if (iterations) remesher = std::make_unique<ba::Remesher>(mesh, iterations);
        else remesher = std::make_unique<ba::Remesher>(mesh);
        
        update_polyscope(mesh)->setEdgeWidth(1.0);
    };

    //Create UI
    polyscope::state::userCallback = [&]() {
        ImGui::Text("Meshes:");
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::Combo("##mesh_selector", &selected_mesh, mesh_names.data(), (int)mesh_names.size())) {
            load_mesh(mesh_files[selected_mesh]);
        }

        ImGui::Separator(); 

        ImGui::Text("Single Operations:");
        if (ImGui::Button("Split Long Edges")) {
            remesher->split_long_edges();
            update_polyscope(mesh); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Collapse Short Edges")) {
            remesher->collapse_short_edges();
            update_polyscope(mesh); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Flip Edges")) {
            remesher->flip_edges();
            update_polyscope(mesh); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Smooth Vertices")) {
            remesher->smooth_vertices();
            update_polyscope(mesh); 
        }

        ImGui::Separator();

        ImGui::Text("Remeshing:");
        if (ImGui::Button("Run 1 Iteration")) {
            remesher->single_iteration();
            update_polyscope(mesh); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Remesh")) {
            remesher->remesh();
            update_polyscope(mesh); 
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0);
        if (remesher) {
            int iterations_input = remesher->get_iterations();
            if (ImGui::InputInt("Iterations", &iterations_input)) {
                remesher->set_iterations(iterations_input);
            }
        }
    };

    load_mesh("data/stanford-bunny.obj");
    polyscope::show();

    return 0;
}