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

void get_collapse_info(const Mesh& mesh, Edge e, Halfedge& h, Point& new_pos) {
    h = mesh.halfedge(e, 0);
    if (mesh.is_boundary(mesh.from_vertex(h)) 
        && !mesh.is_boundary(mesh.to_vertex(h))) h = mesh.halfedge(e, 1);
    Vertex v_from = mesh.from_vertex(h);
    Vertex v_to = mesh.to_vertex(h);
    new_pos = (mesh.is_boundary(v_to)) ? mesh.position(v_to) : 0.5 * (mesh.position(v_from) + mesh.position(v_to));
}

bool is_collapse_valid(const Mesh& mesh, Edge e, Halfedge& h, Point& new_pos, double target_length) {
    if (mesh.is_deleted(e)) return false;
    if (edge_length(mesh, e) >= target_length * L_MIN) return false;

    // Collapse into a boundary vertex if possible, otherwise into midpoint of edge
    get_collapse_info(mesh, e, h, new_pos);
    if (!mesh.is_collapse_ok(h)) return false;
    
    // Check if the collapse would create a long edge, which would result in a loop
    for(auto v : mesh.vertices(mesh.from_vertex(h))) {
        if (v == mesh.to_vertex(h)) continue;
        if (pmp::distance(new_pos, mesh.position(v)) > target_length * L_MAX) return false;
    }
    if (!mesh.is_boundary(mesh.to_vertex(h))) {
        for(auto v : mesh.vertices(mesh.to_vertex(h))) {
            if (v == mesh.from_vertex(h)) continue;
            if (pmp::distance(new_pos, mesh.position(v)) > target_length * L_MAX) return false;
        }
    }

    return true;
}

vec3 compute_smooth_step(const Mesh& mesh, Vertex v) {
    if (mesh.is_deleted(v)) return vec3(0, 0, 0);
    if (mesh.is_boundary(v)) return vec3(0, 0, 0);
    if (mesh.valence(v) == 0) return vec3(0, 0, 0);

    Point center(0, 0, 0);
    for(auto vn : mesh.vertices(v)){
        center += mesh.position(vn);
    }
    center /= mesh.valence(v);

    vec3 dir = center - mesh.position(v);
    Normal normal = vertex_normal(mesh, v);
    vec3 tangential = dir - pmp::dot(dir, normal) * normal;

    double scale = 1.0; // TODO: Implement gentle moving at some point?
    return tangential * scale;
}

} // namespace ba
