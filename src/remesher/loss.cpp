#include "remesher/loss.h"

namespace ba::loss {

double get_edge_loss(const Mesh& mesh, Edge e, double target_length) {
    // Currently using basic log10 of ratio between length and target length
    double ratio = edge_length(mesh, e) / target_length;
    double loss = std::log2(std::max(0.001, std::min(1000.0, ratio)));
    return loss * loss;
}

double calc_total_edge_loss(const Mesh& mesh, double target_length) {
    double total_loss = 0;
    for(auto e : mesh.edges()) {
        total_loss += get_edge_loss(mesh, e, target_length);
    }
    return total_loss;
}

std::vector<double> get_vertex_losses(const Mesh& mesh, double target_length) {
    std::vector<double> vertex_losses(mesh.vertices_size(), 0.0);
    for(auto v : mesh.vertices()) {
        for(auto h : mesh.halfedges(v)) {
            pmp::Edge e = mesh.edge(h);
            vertex_losses[v.idx()] += get_edge_loss(mesh, e, target_length);
        }
        if (mesh.valence(v) > 0) {
            vertex_losses[v.idx()] /= mesh.valence(v);
        }
    }
    return vertex_losses;
}

} // namespace ba::loss