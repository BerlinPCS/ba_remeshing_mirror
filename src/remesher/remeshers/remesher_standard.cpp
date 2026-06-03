#include "remesher/remeshers/remesher_standard.h"

namespace ba {

void RemesherStandard::split_long_edges(){
    int& count = metrics.split_count;
    std::vector<Edge> edges_to_split;
    edges_to_split.reserve(mesh.n_edges() / 4); // Basic assumption that 25% need splitting, also for other operations
    for(auto e : mesh.edges()) {
        if (edge_length(mesh, e) > target_length * L_MAX) {
            edges_to_split.push_back(e);
        }
    }

    for(auto e : edges_to_split) {
        if (split_edge(e)) {
            count++;
            report_progress(0);
        }
    }
}

void RemesherStandard::collapse_short_edges(){
    int& count = metrics.collapse_count;
    std::vector<Edge> edges_to_collapse;
    edges_to_collapse.reserve(mesh.n_edges() / 4); // same assumption
    for(auto e : mesh.edges()) {
        if (edge_length(mesh, e) < target_length * L_MIN) {
            edges_to_collapse.push_back(e);
        }
    }

    for(auto e : edges_to_collapse) {
        if (collapse_edge(e)) {
            count++;
            report_progress(0);
        }
    }
}

void RemesherStandard::flip_edges(){
    int& count = metrics.flip_count;
    std::vector<Edge> edges_to_check;
    edges_to_check.reserve(mesh.n_edges() / 4); //same assumption
    for(auto e : mesh.edges()) {
        if (mesh.is_flip_ok(e)) {
            edges_to_check.push_back(e);
        }
    }

    for(auto e : edges_to_check) {
        if (flip_edge(e)) {
            count++;
            report_progress(0);
        }
    }
}

// Cant reuse the base operation, since all steps have to be calculated before updating positions
void RemesherStandard::smooth_vertices(){
    int& count = metrics.smooth_count;
    auto v_new = mesh.add_vertex_property<vec3>("v:new", vec3(0,0,0));

    for(auto v : mesh.vertices()) {
        v_new[v] = compute_smooth_step(mesh, v);
    }

    for(auto v : mesh.vertices()){
        if(pmp::norm(v_new[v]) > 0.0) {
            mesh.position(v) += v_new[v];
            count++;
            report_progress(0);
        }
    }
    mesh.remove_vertex_property(v_new);
}

} //namespace ba