#include "remesher/remesher.h"
#include "core/geo_utils.h"
#include <pmp/surface_mesh.h>

namespace ba {

double Remesher::get_edge_loss(Edge e) {
    // Currently using basic log10 of ratio between length and target length
    
    double ratio = edge_length(mesh, e) / target_length;
    double loss = std::log2(std::max(0.001, std::min(1000.0, ratio)));
    return loss * loss;
}

void Remesher::split_long_edges(){
    // Gather Relevant Edges to Split later
    std::vector<Edge> edges_to_split;
    edges_to_split.reserve(mesh.n_edges() / 4); // basic heuristic
    for(auto e : mesh.edges()) {
        if (edge_length(mesh, e) > target_length * l_max) {
            edges_to_split.push_back(e);
        }
    }

    std::cout << "Splitting " << edges_to_split.size() << " long edges." << std::endl;

    // Split gathered edges
    for(auto e : edges_to_split) {
        Vertex v0 = mesh.vertex(e, 0);
        Vertex v1 = mesh.vertex(e, 1);

        Point p0 = mesh.position(v0);
        Point p1 = mesh.position(v1);

        auto p_mid = 0.5 * (p0 + p1);
        auto v_mid = mesh.add_vertex(p_mid);

        mesh.split(e, v_mid);
    }
}

void Remesher::collapse_short_edges(){
    // Gather Relevant Edges to Collapse later
    std::vector<Edge> edges_to_collapse;
    edges_to_collapse.reserve(mesh.n_edges() / 4);
    for(auto e : mesh.edges()) {
        if (edge_length(mesh, e) < target_length * l_min) {
            edges_to_collapse.push_back(e);
        }
    }

    std::cout << "Collapsing " << edges_to_collapse.size() << " short edges." << std::endl;

    // Collapse gathered edges
    for(auto e : edges_to_collapse) {
        if (mesh.is_deleted(e)) continue;
        Halfedge h = mesh.halfedge(e, 0);

        // If one of the vertices is on the boundary, we collapse into the boundary vertex.
        // Otherwise, we pull the boundary inwards, changing the shape drastically.
        // Choose proper Halfedge to collapse into the boundary vertex
        if (mesh.is_boundary(mesh.from_vertex(h)) && !mesh.is_boundary(mesh.to_vertex(h))) h = mesh.halfedge(e, 1);
        if (mesh.is_collapse_ok(h)) {
            Vertex v_from = mesh.from_vertex(h);
            Vertex v_to = mesh.to_vertex(h);
            Point pos_to = mesh.position(v_to);
            Point new_pos = (mesh.is_boundary(v_to)) ? pos_to : 0.5 * (mesh.position(v_from) + pos_to);

            // Check if the collapse would create a long edge, which would result in a loop
            bool creates_long_edge = false;
            for(auto v_n : mesh.vertices(v_from)) {
                if (v_n == v_to) continue;
                if (pmp::distance(new_pos, mesh.position(v_n)) > target_length * l_max) {
                    creates_long_edge = true;
                    break;
                }
            }

            if(!creates_long_edge) {
                for(auto v_n : mesh.vertices(v_to)) {
                    if (v_n == v_from) continue;
                    if (pmp::distance(new_pos, mesh.position(v_n)) > target_length * l_max) {
                        creates_long_edge = true;
                        break;
                    }
                }
            }

            if (creates_long_edge) continue;

            mesh.position(v_to) = new_pos;
            mesh.collapse(h);
        }
    }
}

void Remesher::flip_edges(){
    // Gather Relevant Edges to Check later
    std::vector<Edge> edges_to_check;
    edges_to_check.reserve(mesh.n_edges() / 4);
    for(auto e : mesh.edges()) {
        if (mesh.is_flip_ok(e)) {
            edges_to_check.push_back(e);
        }
    }

    std::cout << "Checking " << edges_to_check.size() << " edges to flip." << std::endl;

    for(auto e : edges_to_check) {
        if (!mesh.is_flip_ok(e)) continue;
        Vertex v1 = mesh.vertex(e, 0);
        Vertex v2 = mesh.vertex(e, 1);
        Vertex w1 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(e, 0)));
        Vertex w2 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(e, 1)));

        int v1_v = mesh.valence(v1);
        int v2_v = mesh.valence(v2);
        int w1_v = mesh.valence(w1);
        int w2_v = mesh.valence(w2);

        int iv1 = ideal_valence(mesh, v1);
        int iv2 = ideal_valence(mesh, v2);
        int iw1 = ideal_valence(mesh, w1);
        int iw2 = ideal_valence(mesh, w2);

        // Use squared loss to heavily penalize bad vertices.
        // Alternatively use abs instead
        int d = (v1_v - iv1) * (v1_v - iv1) + 
                (v2_v - iv2) * (v2_v - iv2) + 
                (w1_v - iw1) * (w1_v - iw1) + 
                (w2_v - iw2) * (w2_v - iw2);

        int d_ = (v1_v - 1 - iv1) * (v1_v - 1 - iv1) + 
                 (v2_v - 1 - iv2) * (v2_v - 1 - iv2) + 
                 (w1_v + 1 - iw1) * (w1_v + 1 - iw1) + 
                 (w2_v + 1 - iw2) * (w2_v + 1 - iw2);
        
        if(d_ < d) {
            mesh.flip(e);
        }
    }
}

//Based off https://stanford-cs248.github.io/Cardinal3D/meshedit/global/remesh/
//Didn't add the part where the vertexes get moved gently
void Remesher::smooth_vertices(){
    auto v_new = mesh.add_vertex_property<vec3>("v:new", vec3(0,0,0));

    for(auto v : mesh.vertices()) {
        // Do not smooth boundary vertexes
        if(mesh.is_boundary(v)) continue;

        // Get center of neighbours
        Point center(0, 0, 0);
        for(auto vn : mesh.vertices(v)){
            center += mesh.position(vn);
        }
        center /= mesh.valence(v);

        // Calculate the new position
        vec3 dir = center - mesh.position(v);
        Normal normal = vertex_normal(mesh, v);
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