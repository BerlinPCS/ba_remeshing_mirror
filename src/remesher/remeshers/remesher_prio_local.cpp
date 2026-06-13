#include "remesher/remeshers/remesher_prio_local.h"

namespace ba {

void RemesherPrioLocal::split_long_edges() {
	OpQueue pq;
	for (auto e : mesh.edges()) enqueue_candidate(pq, OpCandidate(OpType::Split, e));

	while (!pq.empty()) {
		OpCandidate cand = pq.top();
		pq.pop();
		if (cand.score < 0) break;

		// Check if this is invalid or stale data
		if (mesh.is_deleted(cand.e) || cand.version != split_versions[cand.e]) continue;

		Vertex v0 = mesh.vertex(cand.e, 0);
		Vertex v1 = mesh.vertex(cand.e, 1);

		if (split_edge(cand.e)) {
			report_progress(OpType::Split, pq.size());

			// Enqueue the new edges - no need to update 1 ring neighborhood, since nothing is moved
			// PMP keeps cand.e valid, one of the endpoints will have been updated to the new vertex
			// Add all 4 edges of the new vertex, since they are all new as well
			Vertex e_v0 = mesh.vertex(cand.e, 0);
			Vertex e_v1 = mesh.vertex(cand.e, 1);
			Vertex v_mid = (e_v0 != v0 && e_v0 != v1) ? e_v0 : e_v1;
			current_tick++;
			for (auto h : mesh.halfedges(v_mid)) {
				Edge e = mesh.edge(h);
				if (split_versions[e] != current_tick) {
					split_versions[e] = current_tick;
					enqueue_candidate(pq, OpCandidate(OpType::Split, e));
				}
				for (auto e_out : mesh.edges(mesh.to_vertex(h))) {
					if (split_versions[e_out] != current_tick) {
						split_versions[e_out] = current_tick;
						enqueue_candidate(pq, OpCandidate(OpType::Split, e_out));
					}
				}
			}
		}
	}
}

void RemesherPrioLocal::collapse_short_edges() {
	OpQueue pq;
	for (auto e : mesh.edges()) enqueue_candidate(pq, OpCandidate(OpType::Collapse, e));

	while (!pq.empty()) {
		OpCandidate cand = pq.top();
		pq.pop();
		if (cand.score < 0) break;

        // Check if this is invalid or stale data
		if (mesh.is_deleted(cand.e) || cand.version != collapse_versions[cand.e]) continue;

		Halfedge h; Point new_pos;
		get_collapse_info(mesh, cand.e, h, new_pos);
		Vertex v_keep = mesh.to_vertex(h);

		if (collapse_edge(cand.e)) {
			report_progress(OpType::Collapse, pq.size());

			current_tick++;
			for (auto h : mesh.halfedges(v_keep)) {
				Edge e = mesh.edge(h);
				if (collapse_versions[e] != current_tick) {
					collapse_versions[e] = current_tick;
					enqueue_candidate(pq, OpCandidate(OpType::Collapse, e));
				}
				for (auto e_out : mesh.edges(mesh.to_vertex(h))) {
					if (collapse_versions[e_out] != current_tick) {
						collapse_versions[e_out] = current_tick;
						enqueue_candidate(pq, OpCandidate(OpType::Collapse, e_out));
					}
				}
			}
		}
	}
}

void RemesherPrioLocal::flip_edges() {
	OpQueue pq;
	for (auto e : mesh.edges()) enqueue_candidate(pq, OpCandidate(OpType::Flip, e));

	while (!pq.empty()) {
		OpCandidate cand = pq.top();
		pq.pop();
		if (cand.score < 0) break;

        // Check if this is invalid or stale data
		if (mesh.is_deleted(cand.e) || cand.version != flip_versions[cand.e]) continue;

		if (flip_edge(cand.e)) {
			report_progress(OpType::Flip, pq.size());

			current_tick++;
			std::vector<Vertex> vertices={
				mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(cand.e, 0))), 
				mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(cand.e, 1)))
			};
			for(auto v_neighbor : vertices) {
				for (auto h : mesh.halfedges(v_neighbor)) {
					Edge e = mesh.edge(h);
					if (flip_versions[e] != current_tick) {
						flip_versions[e] = current_tick;
						enqueue_candidate(pq, OpCandidate(OpType::Flip, e));
					}
					for (auto e_out : mesh.edges(mesh.to_vertex(h))) {
						if (flip_versions[e_out] != current_tick) {
							flip_versions[e_out] = current_tick;
							enqueue_candidate(pq, OpCandidate(OpType::Flip, e_out));
						}
					}
				}	
			}
		}
	}
}

void RemesherPrioLocal::smooth_vertices() {
	OpQueue pq;
	for (auto v : mesh.vertices()) enqueue_candidate(pq, OpCandidate(OpType::Smooth, v));

	while (!pq.empty()) {
		OpCandidate cand = pq.top();
		pq.pop();
		if (cand.score < 0) break;

		// Check if this is invalid or stale data
		if (mesh.is_deleted(cand.v) || cand.version != smooth_versions[cand.v]) continue;

		if (smooth_vertex(cand.v)) {
			report_progress(OpType::Smooth, pq.size());

			current_tick++;
			for (auto v_neighbor : mesh.vertices(cand.v)) {
				smooth_versions[v_neighbor] = current_tick;
				enqueue_candidate(pq, OpCandidate(OpType::Smooth, v_neighbor));
			}
		}
	}
}

void RemesherPrioLocal::remesh() {
    single_iteration();
    progress_callback(true);
}

} // namespace ba
