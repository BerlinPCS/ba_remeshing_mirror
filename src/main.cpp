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
    pmp::SurfaceMesh mesh_high_quality_bunny, mesh_low_quality_bunny;
    pmp::read(mesh_high_quality_bunny, "data/stanford-bunny.obj");
    pmp::read(mesh_low_quality_bunny, "data/stanford-bunny-low.obj");

    //Render Mesh
    updatePolyscope(mesh_high_quality_bunny, "stanford-bunny-high");
    polyscope::getSurfaceMesh("stanford-bunny-high")->setEnabled(false);
    updatePolyscope(mesh_low_quality_bunny, "stanford-bunny-low");
    polyscope::show();

    return 0;
}