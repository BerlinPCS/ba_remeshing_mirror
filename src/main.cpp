#include <iostream>

#include <pmp/surface_mesh.h>
#include <pmp/io/io.h>

#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

#include "remesher.h"

//Feed a updated mesh to Polyscope, which can then be rendered
void updatePolyscope(pmp::SurfaceMesh& mesh, std::string mesh_name) {
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

    polyscope::registerSurfaceMesh(mesh_name, vertices, faces);
}

int main() {
    //Options
    polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::ShadowOnly;
    polyscope::options::shadowBlurIters = 6;

    //Initialize Polyscope
    polyscope::init();

    //Load Mesh
    pmp::SurfaceMesh mesh_high_quality_bunny, mesh_low_quality_bunny, test;
    pmp::read(mesh_high_quality_bunny, "data/stanford-bunny.obj");
    pmp::read(mesh_low_quality_bunny, "data/stanford-bunny-low.obj");
    pmp::read(test, "data/test.obj");

    //Create Remesher
    ba::Remesher remesher(test, 1.0, 2);

    //Render Mesh
    updatePolyscope(mesh_high_quality_bunny, "stanford-bunny-high");
    polyscope::getSurfaceMesh("stanford-bunny-high")->setEnabled(false);
    updatePolyscope(mesh_low_quality_bunny, "stanford-bunny-low");
    polyscope::getSurfaceMesh("stanford-bunny-low")->setEnabled(false);
    updatePolyscope(test, "test");

    //Create UI
    polyscope::state::userCallback = [&]() {
        if (ImGui::Button("Run 1 Iteration")) {
            remesher.single_iteration();
            updatePolyscope(test, "test"); 
        }

        if (ImGui::Button("Split Long Edges")) {
            remesher.split_long_edges();
            updatePolyscope(test, "test"); 
        }

        if (ImGui::Button("Collapse Short Edges")) {
            remesher.collapse_short_edges();
            updatePolyscope(test, "test"); 
        }

        if (ImGui::Button("Flip Edges")) {
            remesher.flip_edges();
            updatePolyscope(test, "test"); 
        }

        if (ImGui::Button("Smooth Vertices")) {
            remesher.smooth_vertices();
            updatePolyscope(test, "test"); 
        }
    };
    polyscope::show();

    return 0;
}