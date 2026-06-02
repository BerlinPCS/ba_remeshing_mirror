#include "remesher/remesher_base.h"
#include "remesher/loss.h"
#include <chrono>

namespace ba {

// Base splitting operation
bool Remesher::split_edge(Edge e) {
    if (mesh.is_deleted(e)) return false;
    if (edge_length(mesh, e) <= target_length * L_MAX) return false;

    Vertex v0 = mesh.vertex(e, 0);
    Vertex v1 = mesh.vertex(e, 1);

    Point p0 = mesh.position(v0);
    Point p1 = mesh.position(v1);

    auto p_mid = 0.5 * (p0 + p1);
    auto v_mid = mesh.add_vertex(p_mid);

    mesh.split(e, v_mid);
    return true;
}

// base collapsing operation
bool Remesher::collapse_edge(Edge e) {
    if (mesh.is_deleted(e)) return false;
    if (edge_length(mesh, e) >= target_length * L_MIN) return false;

    Halfedge h; Point new_pos;
    if (!get_collapse_info(mesh, e, h, new_pos)) return false;

    Vertex v_from = mesh.from_vertex(h);
    Vertex v_to = mesh.to_vertex(h);

    // Check if the collapse would create a long edge, which would result in a loop
    bool creates_long_edge = false;
    for(auto v_n : mesh.vertices(v_from)) {
        if (v_n == v_to) continue;
        if (pmp::distance(new_pos, mesh.position(v_n)) > target_length * L_MAX) {
            creates_long_edge = true;
            break;
        }
    }

    if(!creates_long_edge) {
        for(auto v_n : mesh.vertices(v_to)) {
            if (v_n == v_from) continue;
            if (pmp::distance(new_pos, mesh.position(v_n)) > target_length * L_MAX) {
                creates_long_edge = true;
                break;
            }
        }
    }

    if (creates_long_edge) return false;
    mesh.position(v_to) = new_pos;
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

void Remesher::enqueue_candidate(OpQueue& pq, OpCandidate cand) {
    if (!evaluator) return;
	switch (cand.type) {
		case OpType::Split:
			cand.score = evaluator->split_score(mesh, cand.e);
			break;
		case OpType::Collapse:
			cand.score = evaluator->collapse_score(mesh, cand.e);
			break;
		case OpType::Flip:
			cand.score = evaluator->flip_score(mesh, cand.e);
			break;
		case OpType::Smooth:
			cand.score = evaluator->smooth_score(mesh, cand.v);
			break;
	}
	if (cand.score >= op_gain_threshold) pq.push(cand);
}

void Remesher::single_iteration() {
    metrics.split_count = split_long_edges();
	metrics.collapse_count = collapse_short_edges();
	metrics.flip_count = flip_edges();
	metrics.smooth_count = smooth_vertices();
}

void Remesher::set_metrics() {
    metrics.total_edge_loss = loss::calc_total_edge_loss(mesh, target_length);
    metrics.volume_ratio = volume_ratio(original_mesh, mesh);
    metrics.vertex_count = mesh.n_vertices();
    metrics.edge_count = mesh.n_edges();
    metrics.face_count = mesh.n_faces();
}

void Remesher::timed_iteration(){
    auto start = std::chrono::high_resolution_clock::now();
    single_iteration();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    metrics.time_ms = elapsed.count() * 1000;
    set_metrics();
    mesh.garbage_collection();
}

bool Remesher::converged(double prev_loss) {
    return prev_loss >= 0.0 && std::abs(prev_loss - metrics.total_edge_loss) < op_gain_threshold;
}

void Remesher::remesh(bool run_until_converged){
    int max_iters = (run_until_converged ? 100 : iterations);
    double prev_loss = -1.0;
    for(int i = 0; i < max_iters; i++) {
        timed_iteration();
        if (progress_callback) progress_callback(i + 1, metrics);
        if (run_until_converged) {
            if (converged(prev_loss)) {
                break; // Loss stabilized, exit early
            }
            prev_loss = metrics.total_edge_loss;
        }
    }
}

} //namespace ba