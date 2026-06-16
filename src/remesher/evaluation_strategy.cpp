#include "remesher/evaluation_strategy.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include "core/geo_utils.h"
#include "remesher/loss.h"

namespace ba {

OperationEvaluation EvaluationStrategy::split(const Mesh &mesh, Edge e) const {
	OperationEvaluation result;
	if (mesh.is_deleted(e) || edge_length(mesh, e) <= ctx.target_length * L_MAX) return result;

	const double length = edge_length(mesh, e);
	const double before = loss::get_edge_loss_from_length(length, ctx.target_length);
	const Point mid = 0.5 * (mesh.position(mesh.vertex(e, 0)) + mesh.position(mesh.vertex(e, 1)));
	const Halfedge h0 = mesh.halfedge(e, 0);
	const Halfedge h1 = mesh.halfedge(e, 1);

	std::vector<double> new_losses(2, loss::get_edge_loss_from_length(0.5 * length, ctx.target_length));
	if (!mesh.is_boundary(h0)) {
		const Point opposite = mesh.position(mesh.to_vertex(mesh.next_halfedge(h0)));
		new_losses.push_back(loss::get_edge_loss_from_length(pmp::distance(mid, opposite), ctx.target_length));
	}
	if (!mesh.is_boundary(h1)) {
		const Point opposite = mesh.position(mesh.to_vertex(mesh.next_halfedge(h1)));
		new_losses.push_back(loss::get_edge_loss_from_length(pmp::distance(mid, opposite), ctx.target_length));
	}

	const double sum_after = std::accumulate(new_losses.begin(), new_losses.end(), 0.0);
	double heuristic_after = sum_after;
	if (ctx.split == SplitMode::MAX) {
		heuristic_after = *std::max_element(new_losses.begin(), new_losses.end());
	} else if (ctx.split == SplitMode::AVG) {
		heuristic_after = sum_after / static_cast<double>(new_losses.size());
	}

	result.priority = before - heuristic_after;
	result.edge_loss_gain = before - sum_after;
	result.valid = true;
	result.accepted = result.edge_loss_gain > ctx.op_gain_threshold;
	return result;
}

OperationEvaluation EvaluationStrategy::collapse(const Mesh &mesh, Edge e) const {
	OperationEvaluation result;
	Halfedge h;
	Point new_pos;
	if (!is_collapse_valid(mesh, e, h, new_pos, ctx.target_length)) return result;

	const Vertex v_from = mesh.from_vertex(h);
	const Vertex v_to = mesh.to_vertex(h);
	const Point p_from = mesh.position(v_from);
	const Point p_to = mesh.position(v_to);

	double before = loss::get_edge_loss(mesh, e, ctx.target_length);
	double after = 0.0;
	std::vector<Vertex> neighbors;

	for (auto v : mesh.vertices(v_from)) {
		if (v == v_to) continue;
		neighbors.push_back(v);
		before += loss::get_edge_loss_from_length(pmp::distance(p_from, mesh.position(v)), ctx.target_length);
	}
	for (auto v : mesh.vertices(v_to)) {
		if (v == v_from) continue;
		before += loss::get_edge_loss_from_length(pmp::distance(p_to, mesh.position(v)), ctx.target_length);
		if (std::find(neighbors.begin(), neighbors.end(), v) == neighbors.end()) neighbors.push_back(v);
	}
	for (auto v : neighbors) {
		after += loss::get_edge_loss_from_length(pmp::distance(new_pos, mesh.position(v)), ctx.target_length);
	}

	result.priority = before - after;
	result.edge_loss_gain = result.priority;
	result.valid = true;
	result.accepted = result.edge_loss_gain > ctx.op_gain_threshold;
	return result;
}

OperationEvaluation EvaluationStrategy::flip(const Mesh &mesh, Edge e) const {
	OperationEvaluation result;
	if (mesh.is_deleted(e) || !mesh.is_flip_ok(e)) return result;

	const Vertex v1 = mesh.vertex(e, 0);
	const Vertex v2 = mesh.vertex(e, 1);
	const Vertex w1 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(e, 0)));
	const Vertex w2 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(e, 1)));

	const double before_loss = loss::get_edge_loss(mesh, e, ctx.target_length);
	const double after_loss =
		loss::get_edge_loss_from_length(pmp::distance(mesh.position(w1), mesh.position(w2)), ctx.target_length);
	result.edge_loss_gain = before_loss - after_loss;
	result.valid = true;

	if (ctx.flip == FlipMode::EDGE_LOSS) {
		result.priority = result.edge_loss_gain;
		result.accepted = result.edge_loss_gain > ctx.op_gain_threshold;
		return result;
	}

	const int v1_v = mesh.valence(v1);
	const int v2_v = mesh.valence(v2);
	const int w1_v = mesh.valence(w1);
	const int w2_v = mesh.valence(w2);
	const int iv1 = ideal_valence(mesh, v1);
	const int iv2 = ideal_valence(mesh, v2);
	const int iw1 = ideal_valence(mesh, w1);
	const int iw2 = ideal_valence(mesh, w2);

	const int before_valence = (v1_v - iv1) * (v1_v - iv1) + (v2_v - iv2) * (v2_v - iv2) + (w1_v - iw1) * (w1_v - iw1) +
							   (w2_v - iw2) * (w2_v - iw2);
	const int after_valence = (v1_v - 1 - iv1) * (v1_v - 1 - iv1) + (v2_v - 1 - iv2) * (v2_v - 1 - iv2) +
							  (w1_v + 1 - iw1) * (w1_v + 1 - iw1) + (w2_v + 1 - iw2) * (w2_v + 1 - iw2);

	result.priority = static_cast<double>(before_valence - after_valence);
	result.accepted = result.priority > 0.0;
	return result;
}

OperationEvaluation EvaluationStrategy::smooth(const Mesh &mesh, Vertex v) const {
	OperationEvaluation result;
	if (mesh.is_deleted(v) || mesh.is_boundary(v)) return result;

	const vec3 step = compute_smooth_step(mesh, v);
	if (pmp::norm(step) == 0.0) return result;

	const Point p_new = mesh.position(v) + step;
	double before = 0.0;
	double after = 0.0;
	for (auto neighbor : mesh.vertices(v)) {
		before += loss::get_edge_loss_from_length(pmp::distance(mesh.position(v), mesh.position(neighbor)),
												  ctx.target_length);
		after += loss::get_edge_loss_from_length(pmp::distance(p_new, mesh.position(neighbor)), ctx.target_length);
	}

	result.priority = before - after;
	result.edge_loss_gain = result.priority;
	result.valid = true;
	result.accepted = result.edge_loss_gain > ctx.op_gain_threshold;
	return result;
}

} // namespace ba
