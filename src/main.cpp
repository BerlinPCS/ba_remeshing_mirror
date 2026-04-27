#include <iostream>

#include <pmp/surface_mesh.h>
#include <pmp/io/io.h>

#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

int main() {
    //Options
    polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::ShadowOnly;
    polyscope::options::shadowBlurIters = 6;

    //Initialize Polyscope
    polyscope::init();

    //Load Mesh
    pmp::SurfaceMesh mesh;
    pmp::read(mesh,"data/stanford-bunny.obj");

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

    //Render Mesh
    polyscope::registerSurfaceMesh("test_mesh", vertices, faces);
    polyscope::show();

    return 0;
}