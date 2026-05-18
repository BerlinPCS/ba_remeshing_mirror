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
    }
    avg_length /= mesh.n_edges();
    std::cout << "Average Edge Length: " << avg_length << std::endl;
    return avg_length;
}

double Remesher::get_edge_loss(pmp::Edge e) {
    // Currently using basic log10 of ratio between length and target length
    
    double loss = std::log2(edge_length(e) / target_length);
    return loss * loss;
}

int Remesher::ideal_valence(pmp::Vertex v) {
    if (mesh.is_boundary(v)) {
        return 4;
    }
    return 6;
}

pmp::Normal Remesher::face_normal(pmp::Face f) {
    std::vector<pmp::Point> points;
    for(auto v : mesh.vertices(f)) {
        points.push_back(mesh.position(v));
    }
    return pmp::cross(points[1] - points[0], points[2] - points[0]);
}

pmp::Normal Remesher::vertex_normal(pmp::Vertex v) {
    pmp::Normal normal(0, 0, 0);
    for(auto f : mesh.faces(v)) {
        normal += face_normal(f);
    }
    return pmp::normalize(normal);
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
    for(auto e : edges_to_split) {
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
    // Gather Relevant Edges to Collapse later
    std::vector<pmp::Edge> edges_to_collapse;
    for(auto e : mesh.edges()) {
        //Skip boundary edges for now
        if (!mesh.is_boundary(e) && edge_length(e) < target_length * l_min) {
            edges_to_collapse.push_back(e);
        }
    }

    std::cout << "Collapsing " << edges_to_collapse.size() << " short edges." << std::endl;

    // Collapse gathered edges
    for(auto e : edges_to_collapse) {
        if (mesh.is_deleted(e)) continue;
        pmp::Halfedge h = mesh.halfedge(e, 0);
        pmp::Vertex v_from = mesh.from_vertex(h);
        pmp::Vertex v_to = mesh.to_vertex(h);

        if(mesh.is_boundary(v_from) || mesh.is_boundary(v_to)) continue;
        
        if (mesh.is_collapse_ok(h)) {
            pmp::Point p_mid = 0.5 * (mesh.position(v_from) + mesh.position(v_to));

            // Check if the collapse would create a long edge, which would result in a loop
            bool creates_long_edge = false;
            for(auto v_n : mesh.vertices(v_from)) {
                if (pmp::distance(p_mid, mesh.position(v_n)) > target_length * l_max) {
                    creates_long_edge = true;
                    break;
                }
            }

            if(!creates_long_edge) {
                for(auto v_n : mesh.vertices(v_from)) {
                    if (pmp::distance(p_mid, mesh.position(v_n)) > target_length * l_max) {
                        creates_long_edge = true;
                        break;
                    }
                }
            }

            if (creates_long_edge) continue;

            mesh.position(v_to) = p_mid;
            mesh.collapse(h);
        }
    }
}

void Remesher::flip_edges(){
    // Gather Relevant Edges to Check later
    std::vector<pmp::Edge> edges_to_check;
    for(auto e : mesh.edges()) {
        if (mesh.is_flip_ok(e)) {
            edges_to_check.push_back(e);
        }
    }

    std::cout << "Checking " << edges_to_check.size() << " edges to flip." << std::endl;

    for(auto e : edges_to_check) {
        if (!mesh.is_flip_ok(e)) continue;
        pmp::Vertex v1 = mesh.vertex(e, 0);
        pmp::Vertex v2 = mesh.vertex(e, 1);
        pmp::Vertex w1 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(e, 0)));
        pmp::Vertex w2 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(e, 1)));

        int v1_v = mesh.valence(v1);
        int v2_v = mesh.valence(v2);
        int w1_v = mesh.valence(w1);
        int w2_v = mesh.valence(w2);

        // Use squared loss to heavily penalize bad vertices.
        // Alternatively use abs instead
        int d = (v1_v - ideal_valence(v1)) * (v1_v - ideal_valence(v1)) + 
                (v2_v - ideal_valence(v2)) * (v2_v - ideal_valence(v2)) + 
                (w1_v - ideal_valence(w1)) * (w1_v - ideal_valence(w1)) + 
                (w2_v - ideal_valence(w2)) * (w2_v - ideal_valence(w2));

        int d_ = (v1_v - 1 - ideal_valence(v1)) * (v1_v - 1 - ideal_valence(v1)) + 
                 (v2_v - 1 - ideal_valence(v2)) * (v2_v - 1 - ideal_valence(v2)) + 
                 (w1_v + 1 - ideal_valence(w1)) * (w1_v + 1 - ideal_valence(w1)) + 
                 (w2_v + 1 - ideal_valence(w2)) * (w2_v + 1 - ideal_valence(w2));
        
        if(d_ < d) {
            mesh.flip(e);
        }
    }
}

//Based off https://stanford-cs248.github.io/Cardinal3D/meshedit/global/remesh/
//Didn't add the part where the vertexes get moved gently
void Remesher::smooth_vertices(){
    auto v_new = mesh.add_vertex_property<pmp::vec3>("v:new", pmp::vec3(0,0,0));

    for(auto v : mesh.vertices()) {
        // Do not smooth boundary vertexes
        if(mesh.is_boundary(v)) continue;

        // Get center of neighbours
        pmp::Point center(0, 0, 0);
        for(auto v : mesh.vertices(v)){
            center += mesh.position(v);
        }
        center /= mesh.valence(v);

        // Calculate the new position
        pmp::vec3 dir = center - mesh.position(v);
        pmp::Normal normal = vertex_normal(v);
        v_new[v] = dir - pmp::dot(dir, normal) * normal;
    }

    // Update vertex positions all at once
    for(auto v : mesh.vertices()){
        if(!mesh.is_boundary(v)) {
            mesh.position(v) += v_new[v];
        }
    }

    std::cout << "Smoothed vertices." << std::endl;

    mesh.remove_vertex_property(v_new);
}

void Remesher::single_iteration(){
    split_long_edges();
    collapse_short_edges();
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