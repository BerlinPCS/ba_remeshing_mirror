#include "ui/visuals.h"
#include "core/types.h"
#include "remesher/loss.h"

namespace ba::ui {

ps::SurfaceMesh* pmp_mesh_to_ps(Mesh const& mesh) {
    //Convert to Polyscope format
    std::vector<glm::vec3> vertices;
    std::vector<size_t> pmp_to_ps(mesh.vertices_size(), 0);
    vertices.reserve(mesh.n_vertices());
    size_t idx = 0;
    for(auto v : mesh.vertices()) {
        auto p = mesh.position(v);
        vertices.push_back(glm::vec3(p[0], p[1], p[2]));
        pmp_to_ps[v.idx()] = idx++;
    }

    std::vector<std::vector<size_t>> faces;
    faces.reserve(mesh.n_faces());
    for(auto f : mesh.faces()) {
        std::vector<size_t> face_indices;
        face_indices.reserve(mesh.valence(f));
        for(auto v : mesh.vertices(f)) {
            face_indices.push_back(pmp_to_ps[v.idx()]);
        }
        faces.push_back(std::move(face_indices));
    }

    return ps::registerSurfaceMesh("Mesh", vertices, faces);
}

std::pair<double, double> get_range(std::vector<double> const& losses, float threshold) {
    std::vector<double> sorted_losses = losses;
    size_t idx = std::min(sorted_losses.size() - 1, (size_t)(sorted_losses.size() * threshold));
    std::nth_element(sorted_losses.begin(), sorted_losses.begin() + idx, sorted_losses.end());
    double min_val = *std::min_element(losses.begin(), losses.end());
    double max_val = sorted_losses[idx];
    return {min_val, max_val};
}

ps::SurfaceMesh* draw_surface_mesh(Mesh const& mesh, float target_length) {
    // Create PS Mesh
    auto surface_mesh = pmp_mesh_to_ps(mesh);
    
    // Add vertex losses as a scalar quantity
    std::vector<double> vertex_losses = loss::get_vertex_losses(mesh, target_length);
    surface_mesh->addVertexScalarQuantity("Edge Loss", vertex_losses)
                ->setColorMap("coolwarm")
                ->setMapRange(get_range(vertex_losses, 0.9));

    return surface_mesh;
}

} // namespace ba::ui