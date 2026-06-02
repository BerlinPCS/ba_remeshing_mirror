#include "remesher/evaluation_strategy.h"
#include "remesher/loss.h"
#include <algorithm>
#include "core/geo_utils.h"

namespace ba {

double EvaluationStrategy::split_score(const Mesh& mesh, Edge e) {
    if (mesh.is_deleted(e)) return -1.0;
    if (edge_length(mesh, e) <= target_length * L_MAX) return -1.0;

	Vertex v0 = mesh.vertex(e, 0);
	Vertex v1 = mesh.vertex(e, 1);
    Point p_mid = 0.5 * (mesh.position(v0) + mesh.position(v1)); // Mid is where the split woud happen

    // The two points opposite of the edge e
    Halfedge h0 = mesh.next_halfedge(mesh.halfedge(e, 0));
	Halfedge h1 = mesh.next_halfedge(mesh.halfedge(e, 1));
	Vertex v2 = mesh.is_boundary(h0) ? Vertex() : mesh.to_vertex(h0);
	Vertex v3 = mesh.is_boundary(h1) ? Vertex() : mesh.to_vertex(h1);
    Point p2 = (v2.is_valid()) ? mesh.position(v2) : p_mid;
    Point p3 = (v3.is_valid()) ? mesh.position(v3) : p_mid;    

    double length = edge_length(mesh, e);
    // Loss of the single edge within the two trianges
	double before = loss::get_edge_loss_from_length(length, target_length);
    // Loss of all 4 edges within the two triangles
	double after = 2.0 * loss::get_edge_loss_from_length(0.5 * length, target_length) + 
                   loss::get_edge_loss_from_length(pmp::distance(p2, p_mid), target_length) + 
                   loss::get_edge_loss_from_length(pmp::distance(p3, p_mid), target_length);
    double score = before - after;
    return (score >= op_gain_threshold) ? score : -1.0;
}

double EvaluationStrategy::collapse_score(const Mesh& mesh, Edge e) {
    if (mesh.is_deleted(e)) return -1.0;
    if (edge_length(mesh, e) >= target_length * L_MIN) return -1.0;

    Halfedge h;
    Point new_pos;
    if (!get_collapse_info(mesh, e, h, new_pos)) return -1.0;

    Vertex v_from = mesh.from_vertex(h);
    Vertex v_to = mesh.to_vertex(h);
    Point p_from = mesh.position(v_from);
    Point p_to = mesh.position(v_to);

    double before = loss::get_edge_loss(mesh, e, target_length);
    double after = 0.0;

    // We have to evaluate all edges that are one step away, since they would be affected by the operation
    std::vector<Vertex> neighbors;
    // Evaluate edges around v_from
    for (auto v_n : mesh.vertices(v_from)) {
        if (v_n == v_to) continue;
        neighbors.push_back(v_n);
        before += loss::get_edge_loss_from_length(pmp::distance(p_from, mesh.position(v_n)), target_length);
    }
    // Evaluate edges around v_to
    for (auto v_n : mesh.vertices(v_to)) {
        if (v_n == v_from) continue;
        before += loss::get_edge_loss_from_length(pmp::distance(p_to, mesh.position(v_n)), target_length);
        if (std::find(neighbors.begin(), neighbors.end(), v_n) == neighbors.end()) {
            neighbors.push_back(v_n);
        }
    }
    // Evaluate the new edges connecting to the merged vertex
    for (auto v_n : neighbors) {
        after += loss::get_edge_loss_from_length(pmp::distance(new_pos, mesh.position(v_n)), target_length);
    }
    double score = before - after;
    return (score >= op_gain_threshold) ? score : -1.0;
}

// Ignore for Global Prio Queue - unable to compare since this is valence loss
int EvaluationStrategy::flip_score(const Mesh& mesh, Edge e) {
    if (mesh.is_deleted(e)) return -1;
	if (!mesh.is_flip_ok(e)) return -1;
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

    // Calculate difference from the ideal valence
	int before = (v1_v - iv1) * (v1_v - iv1) + 
			(v2_v - iv2) * (v2_v - iv2) + 
			(w1_v - iw1) * (w1_v - iw1) + 
			(w2_v - iw2) * (w2_v - iw2);

    // And for the flipped edge
	int after = (v1_v - 1 - iv1) * (v1_v - 1 - iv1) + 
			 (v2_v - 1 - iv2) * (v2_v - 1 - iv2) + 
			 (w1_v + 1 - iw1) * (w1_v + 1 - iw1) + 
			 (w2_v + 1 - iw2) * (w2_v + 1 - iw2);

    // And compare
	return before - after;
}

double EvaluationStrategy::smooth_score(const Mesh& mesh, Vertex v) {
    if (mesh.is_deleted(v)) return -1.0;
	if (mesh.is_boundary(v)) return -1.0;

	vec3 step = compute_smooth_step(mesh, v);
    if (pmp::norm(step) == 0.0) return -1.0;
	Point p_new = mesh.position(v) + step;

    double before = loss::single_vertex_loss(mesh, target_length, v);
    double after = 0.0;
    for (auto v_n : mesh.vertices(v)) {
        Point p_n = mesh.position(v_n);

        after += loss::get_edge_loss_from_length(pmp::distance(p_new, p_n), target_length);
    }
    after /= mesh.valence(v);
	double score = before - after;
    return (score >= op_gain_threshold) ? score : -1.0;
}

} // namespace ba
