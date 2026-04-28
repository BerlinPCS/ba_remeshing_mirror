#include "remesher.h"
#include <pmp/surface_mesh.h>

namespace ba {

double Remesher::edge_length(pmp::Edge e) {
    pmp::Vertex v0 = mesh.vertex(e, 0);
    pmp::Vertex v1 = mesh.vertex(e, 1);

    pmp::Point p0 = mesh.position(v0);
    pmp::Point p1 = mesh.position(v1);

    return pmp::distance(p0, p1);
}

double Remesher::avg_edge_length() {
    double avg_length = 0.0;
    for(auto e : mesh.edges()) {
        avg_length += edge_length(e);
        std::cout << "Average Edge Length: " << edge_length(e) << std::endl;
    }
    avg_length /= mesh.n_edges();
    std::cout << "Average Edge Length: " << avg_length << std::endl;
    return avg_length;
}

double Remesher::get_edge_loss(pmp::Edge e) {
    // Currently using basic log10 of ratio between length and target length
    
    double length = edge_length(e);
    return std::pow(std::log10(length / target_length), 2);
}

int Remesher::ideal_valence(pmp::Vertex v) {
    if (mesh.is_boundary(v)) {
        return 4;
    }
    return 6;
}

void Remesher::split_long_edges(){
    // Gather Relevant Edges to Split later
    std::vector<pmp::Edge> edges_to_split;
    for(auto e : mesh.edges()) {
        if (edge_length(e) > target_length * l_max) {
            edges_to_split.push_back(e);
        }
    }

    std::cout << "Splitting " << edges_to_split.size() << " long edges." << std::endl;

    // Split gathered edges
    for (auto e : edges_to_split) {
        pmp::Vertex v0 = mesh.vertex(e, 0);
        pmp::Vertex v1 = mesh.vertex(e, 1);

        pmp::Point p0 = mesh.position(v0);
        pmp::Point p1 = mesh.position(v1);

        auto p_mid = 0.5 * (p0 + p1);
        auto v_mid = mesh.add_vertex(p_mid);

        mesh.split(e, v_mid);
    }
}

void Remesher::collapse_short_edges(){
    //STILL NEED TO TEST

    // Gather Relevant Edges to Collapse later
    std::vector<pmp::Edge> edges_to_collapse;
    for (auto e : mesh.edges()) {
        if (edge_length(e) < target_length * l_min) {
            edges_to_collapse.push_back(e);
        }
    }

    std::cout << "Collapsing " << edges_to_collapse.size() << " short edges." << std::endl;

    // Collapse gathered edges
    for (auto e : edges_to_collapse) {
        pmp::Halfedge h = mesh.halfedge(e, 0);
        if (mesh.is_collapse_ok(h)) {
            pmp::Vertex v_from = mesh.from_vertex(h);
            pmp::Vertex v_to = mesh.to_vertex(h);
            mesh.position(v_to) = 0.5 * (mesh.position(v_from) + mesh.position(v_to));

            mesh.collapse(h);
        }
    }
}

void Remesher::flip_edges(){
    // TODO - implement
}

void Remesher::smooth_vertices(){
    // TODO - implement
}

void Remesher::single_iteration(){
    split_long_edges();
    //collapse_short_edges();
    flip_edges();
    smooth_vertices();
}

void Remesher::remesh(){
    if(iterations > 0) {
        for(int i = 0; i < iterations; i++) {
            single_iteration();
        }
    } else {
        //TODO - run until converged
    }
}
} //namespace ba