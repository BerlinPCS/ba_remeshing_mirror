#include "remesher/remeshers/remesher_prio_global.h"

namespace ba {

void RemesherPrioGlobal::enqueue_length_ops(OpQueue& pq, Edge e) {
	current_tick++;
	split_versions[e] = current_tick;
	collapse_versions[e] = current_tick;
	enqueue_candidate(pq, OpCandidate(OpType::Split, e));
	enqueue_candidate(pq, OpCandidate(OpType::Collapse, e));
}

void RemesherPrioGlobal::enqueue_flip(OpQueue& flip_pq, Edge e) {
	current_tick++;
	flip_versions[e] = current_tick;
	enqueue_candidate(flip_pq, OpCandidate(OpType::Flip, e));
}

void RemesherPrioGlobal::enqueue_smooth(OpQueue& pq, Vertex v) {
	current_tick++;
	smooth_versions[v] = current_tick;
	enqueue_candidate(pq, OpCandidate(OpType::Smooth, v));
}

void RemesherPrioGlobal::enqueue_affected_region(OpQueue& pq, OpQueue& flip_pq, const std::vector<Vertex>& vs) {
	std::vector<Vertex> smooth_set;
	smooth_set.reserve(10);
	std::vector<Edge> length_set;
	length_set.reserve(60);
	std::vector<Edge> flip_set;
	flip_set.reserve(60);

	for (Vertex v : vs) {
		if (!v.is_valid() || mesh.is_deleted(v)) continue;
		smooth_set.push_back(v);
		for (auto h : mesh.halfedges(v)) {
			Edge e = mesh.edge(h);
			length_set.push_back(e);
			flip_set.push_back(e);
			
			Vertex to_v = mesh.to_vertex(h);
			smooth_set.push_back(to_v);
			
			for (auto e_out : mesh.edges(to_v)) {
				flip_set.push_back(e_out);
				length_set.push_back(e_out);
			}
		}
	}

	std::sort(smooth_set.begin(), smooth_set.end());
	smooth_set.erase(std::unique(smooth_set.begin(), smooth_set.end()), smooth_set.end());
	std::sort(length_set.begin(), length_set.end());
	length_set.erase(std::unique(length_set.begin(), length_set.end()), length_set.end());
	std::sort(flip_set.begin(), flip_set.end());
	flip_set.erase(std::unique(flip_set.begin(), flip_set.end()), flip_set.end());

	for (Vertex v : smooth_set) enqueue_smooth(pq, v);
	for (Edge e : length_set) enqueue_length_ops(pq, e);
	for (Edge e : flip_set) enqueue_flip(flip_pq, e);
}

void RemesherPrioGlobal::single_iteration() {
	OpQueue pq, real_flip_pq;
	OpQueue& flip_pq = r_ctx.separate_flip_queue ? real_flip_pq : pq;
	for (auto e : mesh.edges()) {
		enqueue_length_ops(pq, e);
		enqueue_flip(flip_pq, e);
	}
	for (auto v : mesh.vertices()) enqueue_smooth(pq, v);

	while (!pq.empty() || !flip_pq.empty()) {
		OpCandidate cand;
		int ops = p_ctx.load().metrics.operations;
		if (!flip_pq.empty() && (pq.empty() || ops % r_ctx.flip_frequency == 0)) {
			cand = flip_pq.top();
			flip_pq.pop();
		} else {
			cand = pq.top();
			pq.pop();
		}
		if (cand.score < 0) break;

		int queue_size = pq.size() + flip_pq.size();

		if (cand.type == OpType::Split) {
			if (mesh.is_deleted(cand.e) || cand.version != split_versions[cand.e]) continue;
			
			Vertex v0 = mesh.vertex(cand.e, 0);
			Vertex v1 = mesh.vertex(cand.e, 1);

			if (split_edge(cand.e)) {
				report_progress(OpType::Split, queue_size);
				Vertex e_v0 = mesh.vertex(cand.e, 0);
				Vertex e_v1 = mesh.vertex(cand.e, 1);
				Vertex v_mid = (e_v0 != v0 && e_v0 != v1) ? e_v0 : e_v1;
				enqueue_affected_region(pq, flip_pq, {v_mid});
			}
		} else if (cand.type == OpType::Collapse) {
			if (mesh.is_deleted(cand.e) || cand.version != collapse_versions[cand.e]) continue;

			Halfedge h;
			Point new_pos;
			get_collapse_info(mesh, cand.e, h, new_pos);
			Vertex v_keep = mesh.to_vertex(h);

			if (collapse_edge(cand.e)) {
				report_progress(OpType::Collapse, queue_size);
				enqueue_affected_region(pq, flip_pq, {v_keep});
			}
		} else if (cand.type == OpType::Smooth) {
			if (mesh.is_deleted(cand.v) || cand.version != smooth_versions[cand.v]) continue;
			if (smooth_vertex(cand.v)) {
				report_progress(OpType::Smooth, queue_size);
				enqueue_affected_region(pq, flip_pq, {cand.v});
			}
		} else if (cand.type == OpType::Flip) {
			if (mesh.is_deleted(cand.e) || cand.version != flip_versions[cand.e]) continue;
			if (flip_edge(cand.e)) {
				report_progress(OpType::Flip, queue_size);
				enqueue_affected_region(pq, flip_pq, {mesh.vertex(cand.e, 0), mesh.vertex(cand.e, 1)});
			}
		}
	}
	mesh.garbage_collection();
}

void RemesherPrioGlobal::remesh() {
	single_iteration();
	progress_callback(true);
}

} // namespace ba
