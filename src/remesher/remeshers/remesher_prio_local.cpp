#include "remesher/remeshers/remesher_prio_local.h"

namespace ba {

int RemesherPrioLocal::split_long_edges() {
	OpQueue pq;
	for (auto e : mesh.edges()) enqueue_candidate(pq, OpCandidate(OpType::Split, e));

	int count = 0;
	while (!pq.empty()) {
		OpCandidate cand = pq.top();
		pq.pop();
		if (cand.score < 0) break;

		Vertex v0 = mesh.vertex(cand.e, 0);
		Vertex v1 = mesh.vertex(cand.e, 1);

		if (split_edge(cand.e)) {
			count++;
			// Enqueue the new edges - no need to update 1 ring neighborhood, since nothing is moved
			// PMP keeps cand.e valid, one of the endpoints will have been updated to the new vertex
			// Add all 4 edges of the new vertex, since they are all new as well
			Vertex e_v0 = mesh.vertex(cand.e, 0);
			Vertex e_v1 = mesh.vertex(cand.e, 1);
			Vertex v_mid = (e_v0 != v0 && e_v0 != v1) ? e_v0 : e_v1;
			if (v_mid.is_valid()) for (auto e : mesh.edges(v_mid)) enqueue_candidate(pq, OpCandidate(OpType::Split, e));
		}
	}
	return count;
}

int RemesherPrioLocal::collapse_short_edges() {
	OpQueue pq;
	for (auto e : mesh.edges()) enqueue_candidate(pq, OpCandidate(OpType::Collapse, e));

	int count = 0;
	while (!pq.empty()) {
		OpCandidate cand = pq.top();
		pq.pop();
		if (cand.score < 0) break;

        // Check if this is invalid or stale data (updated data, if applicable, should have been added
		double current_score = evaluator->collapse_score(mesh, cand.e);
		if (std::abs(current_score - cand.score) > 1e-5) continue;

		Halfedge h; Point new_pos;
		get_collapse_info(mesh, cand.e, h, new_pos);
		Vertex v_keep = mesh.to_vertex(h);

		if (collapse_edge(cand.e)) {
			count++;
			for (auto e_out : mesh.edges(v_keep)) {
				enqueue_candidate(pq, OpCandidate(OpType::Collapse, e_out));
			}
		}
	}
	return count;
}

int RemesherPrioLocal::flip_edges() {
	OpQueue pq;
	for (auto e : mesh.edges()) enqueue_candidate(pq, OpCandidate(OpType::Flip, e));

	int count = 0;
	while (!pq.empty()) {
		OpCandidate cand = pq.top();
		pq.pop();
		if (cand.score < 0) break;

        // Check if this is invalid or stale data (updated data, if applicable, should have been added
		double current_score = evaluator->flip_score(mesh, cand.e);
		if (std::abs(current_score - cand.score) > 1e-5) continue;

		if (flip_edge(cand.e)) {
			count++;
			// All neighboring vertices will have had their valence change - enqueue all again
			std::vector<Vertex> vertices(
				{mesh.vertex(cand.e, 0), 
					mesh.vertex(cand.e, 1), 
					mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(cand.e, 0))), 
					mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(cand.e, 1)))
				});
			
			for (auto v : vertices) {
				for (auto e : mesh.edges(v)) {
					enqueue_candidate(pq, OpCandidate(OpType::Flip, e));
				}
			}
		}
	}
	return count;
}

int RemesherPrioLocal::smooth_vertices() {
	OpQueue pq;
	for (auto v : mesh.vertices()) enqueue_candidate(pq, OpCandidate(OpType::Smooth, v));

	int count = 0;
	while (!pq.empty()) {
		OpCandidate cand = pq.top();
		pq.pop();
		if (cand.score < 0) break;

		if (smooth_vertex(cand.v)) count++;
	}
	return count;
}

} // namespace ba
