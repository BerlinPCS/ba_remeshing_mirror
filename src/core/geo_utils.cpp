#include "core/geo_utils.h"

namespace ba {

double edge_length(const Mesh& mesh, Edge e) {
    Vertex v0 = mesh.vertex(e, 0);
    Vertex v1 = mesh.vertex(e, 1);

    Point p0 = mesh.position(v0);
    Point p1 = mesh.position(v1);

    return pmp::distance(p0, p1);
}

double avg_edge_length(const Mesh& mesh) {
    if (mesh.n_edges() == 0) return 0.0;
    double avg_length = 0.0;
    for(auto e : mesh.edges()) {
        avg_length += edge_length(mesh, e);
    }
    avg_length /= mesh.n_edges();
    return avg_length;
}

Normal face_normal(const Mesh& mesh, Face f) {
    auto v_it = mesh.vertices(f);
    auto it = v_it.begin();
    Point p0 = mesh.position(*it); ++it;
    Point p1 = mesh.position(*it); ++it;
    Point p2 = mesh.position(*it);
    
    return pmp::cross(p1 - p0, p2 - p0);
}

Normal vertex_normal(const Mesh& mesh, Vertex v) {
    Normal normal(0, 0, 0);
    for(auto f : mesh.faces(v)) {
        normal += face_normal(mesh, f);
    }
    return pmp::normalize(normal);
}

int ideal_valence(const Mesh& mesh, Vertex v) {
    if (mesh.is_boundary(v)) {
        return 4;
    }
    return 6;
}

double get_mesh_volume(const Mesh& mesh) {
    double volume = 0.0;
    for (auto f : mesh.faces()) {
        auto v_it = mesh.vertices(f);
        auto it = v_it.begin();
        Point p0 = mesh.position(*it); ++it;
        Point p1 = mesh.position(*it); ++it;
        Point p2 = mesh.position(*it);
        
        volume += pmp::dot(p0, pmp::cross(p1, p2));
    }
    return std::abs(volume) / 6.0;
}

double volume_ratio(const Mesh& mesh1, const Mesh& mesh2) {
    double vol1 = get_mesh_volume(mesh1), vol2 = get_mesh_volume(mesh2);
    if (vol1 == 0.0 || vol2 == 0.0) return -1.0; 
    return vol2 / vol1;
}

} // namespace ba
