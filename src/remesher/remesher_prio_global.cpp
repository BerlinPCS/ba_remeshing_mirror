#include "remesher/remesher_prio_global.h"

namespace ba {

void RemesherPrioGlobal::enqueue_neighborhood(Vertex v, OpQueue& pq) {
	if (mesh.is_deleted(v)) return;
	enqueue_candidate(pq, OpCandidate(OpType::Smooth, v));
	for (auto h : mesh.halfedges(v)) {
		Edge e = mesh.edge(h);
		enqueue_candidate(pq, OpCandidate(OpType::Split, e));
		enqueue_candidate(pq, OpCandidate(OpType::Collapse, e));
		enqueue_candidate(pq, OpCandidate(OpType::Smooth, mesh.to_vertex(h)));
	}
}

void RemesherPrioGlobal::single_iteration() {
	int split_count = 0;
	int collapse_count = 0;
	int flip_count = 0;
	int smooth_count = 0;
	int count = 0;

	OpQueue pq;
	for (auto e : mesh.edges()) {
		enqueue_candidate(pq, OpCandidate(OpType::Split, e));
		enqueue_candidate(pq, OpCandidate(OpType::Collapse, e));
	}
	for (auto v : mesh.vertices()) enqueue_candidate(pq, OpCandidate(OpType::Smooth, v));

	size_t current_flip_edge_idx = 0;

	while (!pq.empty()) {
		OpCandidate cand = pq.top();
		pq.pop();
		if (cand.score < 0) break;

		// Check if this is stale data that has changed (updated data should have been added to queue)
		double current_score = -1.0;
		if (cand.type == OpType::Split) {
			current_score = split_score(cand.e);
		} else if (cand.type == OpType::Collapse) {
			current_score = collapse_score(cand.e);
		} else if (cand.type == OpType::Smooth) {
			current_score = smooth_score(cand.v);
		}
		if (std::abs(current_score - cand.score) > 1e-5) {
			continue;
		}

		if (cand.type == OpType::Split) {
			// Need to enqueue more than for the local since valence changes
			Vertex v0 = mesh.vertex(cand.e, 0);
			Vertex v1 = mesh.vertex(cand.e, 1);
			Halfedge h0 = mesh.halfedge(cand.e, 0);
			Halfedge h1 = mesh.halfedge(cand.e, 1);
			Vertex v2 = mesh.is_boundary(h0) ? Vertex() : mesh.to_vertex(mesh.next_halfedge(h0));
			Vertex v3 = mesh.is_boundary(h1) ? Vertex() : mesh.to_vertex(mesh.next_halfedge(h1));
			
			if (split_edge(cand.e)) {
				split_count++;
				enqueue_neighborhood(v0, pq);
				enqueue_neighborhood(v1, pq);
				if (v2.is_valid()) enqueue_neighborhood(v2, pq);
				if (v3.is_valid()) enqueue_neighborhood(v3, pq);
			}
		} else if (cand.type == OpType::Collapse) {
			Halfedge h; Point new_pos;
			Vertex v_keep;
			// to_vertex(h) is the vertex that is updated to the new position in collapse_edge()
			if (get_collapse_info(cand.e, h, new_pos)) v_keep = mesh.to_vertex(h);

			if (collapse_edge(cand.e)) {
				collapse_count++;
				if (v_keep.is_valid()) enqueue_neighborhood(v_keep, pq);
			}
		} else if (cand.type == OpType::Smooth) {
			if (smooth_vertex(cand.v)) {
				smooth_count++;
				enqueue_neighborhood(cand.v, pq);
			}
		}

		if (count % flip_frequency == 0) {
			while (current_flip_edge_idx < mesh.edges_size()) {
				Edge e_flip(current_flip_edge_idx++);
				if (flip_edge(e_flip)) {
					flip_count++;
					break;
				}
			}
		}
		count = split_count + collapse_count + flip_count + smooth_count;
	}

	metrics.split_count = split_count;
	metrics.collapse_count = collapse_count;
	metrics.flip_count = flip_count;
	metrics.smooth_count = smooth_count;
}

} // namespace ba
