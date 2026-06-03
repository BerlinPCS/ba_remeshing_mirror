#include "remesher/remesher_base.h"
#include "core/types.h"
#include "remesher/loss.h"

namespace ba {

// Base splitting operation
bool Remesher::split_edge(Edge e) {
    if (mesh.is_deleted(e)) return false;
    if (edge_length(mesh, e) <= target_length * L_MAX) return false;

    Point p0 = mesh.position(mesh.vertex(e, 0));
    Point p1 = mesh.position(mesh.vertex(e, 1));
    Point p_mid = 0.5 * (p0 + p1);
    
    mesh.split(e, p_mid);
    return true;
}

// base collapsing operation
bool Remesher::collapse_edge(Edge e) {
    Halfedge h; Point new_pos;
    if (!is_collapse_valid(mesh, e, h, new_pos, target_length)) return false;

    mesh.position(mesh.to_vertex(h)) = new_pos;
    mesh.collapse(h);
    return true;
}

// Base flipping operation
bool Remesher::flip_edge(Edge e) {
    if(evaluator && evaluator->flip_score(mesh, e) > 0) {
        mesh.flip(e);
        return true;
    }
    return false;
}

// Base smoothing operation (for single vertex, not entire mesh)
bool Remesher::smooth_vertex(Vertex v) {
    vec3 step = compute_smooth_step(mesh, v);
    if (pmp::norm(step) == 0.0) return false;
    mesh.position(v) += step;
    return true;
}

void Remesher::single_iteration() {
    split_long_edges();
	collapse_short_edges();
	flip_edges();
	smooth_vertices();
    set_metrics();
    mesh.garbage_collection();
}

void Remesher::set_metrics() {
    metrics.total_edge_loss = loss::calc_total_edge_loss(mesh, target_length);
    metrics.volume_ratio = volume_ratio(original_mesh, mesh);
    metrics.vertex_count = mesh.n_vertices();
    metrics.edge_count = mesh.n_edges();
    metrics.face_count = mesh.n_faces();
}

void Remesher::remesh() {
    for(int i = 0; i < iterations; i++) single_iteration();
    if (progress_callback) progress_callback(metrics.operations, metrics, true);
}

// ------------------------ Helper functions ------------------------

void Remesher::enqueue_candidate(OpQueue& pq, OpCandidate cand) {
    if (!evaluator) return;
	switch (cand.type) {
		case OpType::Split:
			cand.score = evaluator->split_score(mesh, cand.e);
			cand.version = split_versions[cand.e];
			break;
		case OpType::Collapse:
			cand.score = evaluator->collapse_score(mesh, cand.e);
			cand.version = collapse_versions[cand.e];
			break;
		case OpType::Flip:
			cand.score = evaluator->flip_score(mesh, cand.e);
			cand.version = flip_versions[cand.e];
			break;
		case OpType::Smooth:
			cand.score = evaluator->smooth_score(mesh, cand.v);
			cand.version = smooth_versions[cand.v];
			break;
	}
	if (cand.score >= op_gain_threshold) pq.push(cand);
}

} //namespace ba