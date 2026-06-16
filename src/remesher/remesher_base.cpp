#include "remesher/remesher_base.h"
#include "core/types.h"
#include "remesher/loss.h"

namespace ba {

// Base splitting operation
bool Remesher::split_edge(Edge e) {
	if (mesh.is_deleted(e)) return false;
	if (edge_length(mesh, e) <= r_ctx.target_length * L_MAX) return false;

	Point p0 = mesh.position(mesh.vertex(e, 0));
	Point p1 = mesh.position(mesh.vertex(e, 1));
	Point p_mid = 0.5 * (p0 + p1);

	mesh.split(e, p_mid);
	return true;
}

// base collapsing operation
bool Remesher::collapse_edge(Edge e) {
	Halfedge h;
	Point new_pos;
	if (!is_collapse_valid(mesh, e, h, new_pos, r_ctx.target_length)) return false;

	mesh.position(mesh.to_vertex(h)) = new_pos;
	mesh.collapse(h);
	return true;
}

// Base flipping operation
bool Remesher::flip_edge(Edge e) {
	if (mesh.is_deleted(e) || !mesh.is_flip_ok(e)) return false;
	mesh.flip(e);
	return true;
}

// Base smoothing operation (for single vertex, not entire mesh)
bool Remesher::smooth_vertex(Vertex v) {
	vec3 step = compute_smooth_step(mesh, v);
	if (pmp::norm(step) == 0.0) return false;
	mesh.position(v) += step;
	return true;
}

void Remesher::set_metrics() {
	// Pre-compute outside the lock to minimize hold time
	double edge_loss = loss::calc_total_edge_loss(mesh, r_ctx.target_length);
	const double current_volume = get_mesh_volume(mesh);
	double vol_ratio = original_volume > 0.0 ? current_volume / original_volume : -1.0;
	int verts = mesh.n_vertices(), edges = mesh.n_edges(), faces = mesh.n_faces();

	p_ctx.update([&](ProgressState &state) {
		state.metrics.total_edge_loss = edge_loss;
		state.metrics.volume_ratio = vol_ratio;
		state.metrics.vertex_count = verts;
		state.metrics.edge_count = edges;
		state.metrics.face_count = faces;
	});
}

void Remesher::remesh() {
	p_ctx.update([&](ProgressState &state) { state.total_iterations = r_ctx.iterations; });
	for (int i = 0; i < r_ctx.iterations; i++) {
		single_iteration();
		p_ctx.update([&](ProgressState &state) { state.current_iteration = i + 1; });
	}
	if (progress_callback) progress_callback(true);
}

// ------------------------ Helper functions ------------------------

OperationEvaluation Remesher::evaluate(const OpCandidate &cand) const {
	if (!evaluator) return {};
	switch (cand.type) {
	case OpType::Split:
		return evaluator->split(mesh, cand.e);
	case OpType::Collapse:
		return evaluator->collapse(mesh, cand.e);
	case OpType::Flip:
		return evaluator->flip(mesh, cand.e);
	case OpType::Smooth:
		return evaluator->smooth(mesh, cand.v);
	}
	return {};
}

bool Remesher::enqueue_candidate(OpQueue &pq, OpCandidate cand) {
	const OperationEvaluation evaluation = evaluate(cand);
	if (!evaluation.accepted) return false;
	cand.score = evaluation.priority;
	switch (cand.type) {
	case OpType::Split:
		cand.version = split_versions[cand.e];
		break;
	case OpType::Collapse:
		cand.version = collapse_versions[cand.e];
		break;
	case OpType::Flip:
		cand.version = flip_versions[cand.e];
		break;
	case OpType::Smooth:
		cand.version = smooth_versions[cand.v];
		break;
	}
	pq.push(cand);
	return true;
}

} // namespace ba
