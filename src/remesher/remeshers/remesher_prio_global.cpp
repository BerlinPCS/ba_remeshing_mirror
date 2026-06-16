#include "remesher/remeshers/remesher_prio_global.h"

#include <algorithm>
#include <cmath>

namespace ba {

void RemesherPrioGlobal::enqueue_length_ops(OpQueue& pq, Edge e) {
	split_versions[e] = current_tick;
	collapse_versions[e] = current_tick;
	if (enqueue_candidate(pq, OpCandidate(OpType::Split, e))) queue_stats.queued++;
	if (enqueue_candidate(pq, OpCandidate(OpType::Collapse, e))) queue_stats.queued++;
}

void RemesherPrioGlobal::enqueue_flip(OpQueue& flip_pq, Edge e) {
	flip_versions[e] = current_tick;
	if (enqueue_candidate(flip_pq, OpCandidate(OpType::Flip, e))) queue_stats.queued++;
}

void RemesherPrioGlobal::enqueue_smooth(OpQueue& pq, Vertex v) {
	smooth_versions[v] = current_tick;
	if (enqueue_candidate(pq, OpCandidate(OpType::Smooth, v))) queue_stats.queued++;
}

void RemesherPrioGlobal::populate_queues(OpQueue& pq, OpQueue& flip_pq) {
	current_tick++;
	for (auto e : mesh.edges()) {
		enqueue_length_ops(pq, e);
		enqueue_flip(flip_pq, e);
	}
	for (auto v : mesh.vertices()) enqueue_smooth(pq, v);
}

void RemesherPrioGlobal::enqueue_affected_region(OpQueue& pq, OpQueue& flip_pq, const std::vector<Vertex>& vertices) {
	std::vector<Vertex> smooth_set;
	std::vector<Edge> length_set;
	std::vector<Edge> flip_set;
	smooth_set.reserve(16);
	length_set.reserve(64);
	flip_set.reserve(64);

	for (Vertex v : vertices) {
		if (!v.is_valid() || mesh.is_deleted(v)) continue;
		smooth_set.push_back(v);
		for (auto h : mesh.halfedges(v)) {
			const Edge e = mesh.edge(h);
			length_set.push_back(e);
			flip_set.push_back(e);

			const Vertex neighbor = mesh.to_vertex(h);
			smooth_set.push_back(neighbor);
			for (auto e_out : mesh.edges(neighbor)) {
				length_set.push_back(e_out);
				flip_set.push_back(e_out);
			}
		}
	}

	auto unique = [](auto& values) {
		std::sort(values.begin(), values.end());
		values.erase(std::unique(values.begin(), values.end()), values.end());
	};
	unique(smooth_set);
	unique(length_set);
	unique(flip_set);

	current_tick++;
	for (Vertex v : smooth_set) enqueue_smooth(pq, v);
	for (Edge e : length_set) enqueue_length_ops(pq, e);
	for (Edge e : flip_set) enqueue_flip(flip_pq, e);
}

bool RemesherPrioGlobal::is_stale(const OpCandidate& cand) const {
	switch (cand.type) {
	case OpType::Split:
		return mesh.is_deleted(cand.e) || cand.version != split_versions[cand.e];
	case OpType::Collapse:
		return mesh.is_deleted(cand.e) || cand.version != collapse_versions[cand.e];
	case OpType::Flip:
		return mesh.is_deleted(cand.e) || cand.version != flip_versions[cand.e];
	case OpType::Smooth:
		return mesh.is_deleted(cand.v) || cand.version != smooth_versions[cand.v];
	}
	return true;
}

// Account for floating point inaccuracies
// TODO: Using a named relative/absolute epsilon would make its purpose clearer.
// Currently very small as well
bool RemesherPrioGlobal::priority_changed(double old_priority, double new_priority) const {
	const double tolerance = 1e-10 * (1.0 + std::abs(old_priority));
	return std::abs(old_priority - new_priority) > tolerance;
}

void RemesherPrioGlobal::publish_queue_state(int queue_size) {
	p_ctx.update([&](ProgressState& state) {
		state.current_queue_size = queue_size;
		state.queue_stats = queue_stats;
	});
}

void RemesherPrioGlobal::single_iteration() {
	OpQueue pq;
	OpQueue flip_pq;
	OpQueue& flip_target = r_ctx.flip == FlipMode::VALENCE ? flip_pq : pq;

	queue_stats = {};
	populate_queues(pq, flip_target);

	const int initial_elements = static_cast<int>(mesh.n_edges() + mesh.n_vertices());
	const int operation_limit = std::max(1000, initial_elements * 20);
	int energy_ops_since_flip = 0;
	int stale_at_rebuild = 0;
	int popped_at_rebuild = 0;
	bool hit_operation_limit = false;

	auto check_queue = [&]() {
		// If more than half of recent pops were stale, rebuild queues
		const int recent_pops = queue_stats.popped - popped_at_rebuild;
		const int recent_stale = queue_stats.stale - stale_at_rebuild;
		if (recent_pops >= 1000 && recent_stale * 2 > recent_pops) {
			pq = OpQueue();
			flip_pq = OpQueue();
			OpQueue& rebuilt_flip_target = r_ctx.flip == FlipMode::VALENCE ? flip_pq : pq;
			populate_queues(pq, rebuilt_flip_target);
			queue_stats.popped += queue_stats.queued;
            queue_stats.queued = 0;
			queue_stats.rebuilt++;
			stale_at_rebuild = queue_stats.stale;
			popped_at_rebuild = queue_stats.popped;
		}
		publish_queue_state(static_cast<int>(pq.size() + flip_pq.size()));
	};

	while (!pq.empty() || !flip_pq.empty()) {
		if (r_ctx.flip == FlipMode::VALENCE && queue_stats.executed >= operation_limit) {
			hit_operation_limit = true;
			break;
		}

		const bool take_flip =
			!flip_pq.empty() && (pq.empty() || energy_ops_since_flip >= std::max(1, r_ctx.flip_frequency));
		OpQueue& source = take_flip ? flip_pq : pq;
		OpCandidate cand = source.top();
		source.pop();
		queue_stats.popped++;

		if (is_stale(cand)) {
			queue_stats.stale++;
			check_queue();
			continue;
		}

		const OperationEvaluation current = evaluate(cand);
		if (!current.accepted) {
			queue_stats.rejected++;
			check_queue();
			continue;
		}
		if (priority_changed(cand.score, current.priority)) {
			cand.score = current.priority;
			source.push(cand);
			queue_stats.queued++;
			check_queue();
			continue;
		}

		bool executed = false;
		std::vector<Vertex> affected;
		if (cand.type == OpType::Split) {
			const Vertex v0 = mesh.vertex(cand.e, 0);
			const Vertex v1 = mesh.vertex(cand.e, 1);
			if (split_edge(cand.e)) {
				const Vertex e_v0 = mesh.vertex(cand.e, 0);
				const Vertex e_v1 = mesh.vertex(cand.e, 1);
				affected.push_back((e_v0 != v0 && e_v0 != v1) ? e_v0 : e_v1);
				executed = true;
			}
		} else if (cand.type == OpType::Collapse) {
			Halfedge h;
			Point new_pos;
			get_collapse_info(mesh, cand.e, h, new_pos);
			const Vertex keep = mesh.to_vertex(h);
			if (collapse_edge(cand.e)) {
				affected.push_back(keep);
				executed = true;
			}
		} else if (cand.type == OpType::Smooth) {
			if (smooth_vertex(cand.v)) {
				affected.push_back(cand.v);
				executed = true;
			}
		} else if (cand.type == OpType::Flip) {
			const Vertex v0 = mesh.vertex(cand.e, 0);
			const Vertex v1 = mesh.vertex(cand.e, 1);
			const Vertex w0 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(cand.e, 0)));
			const Vertex w1 = mesh.to_vertex(mesh.next_halfedge(mesh.halfedge(cand.e, 1)));
			if (flip_edge(cand.e)) {
				affected = {v0, v1, w0, w1};
				executed = true;
			}
		}

		if (!executed) {
			queue_stats.rejected++;
			check_queue();
			continue;
		}

		queue_stats.executed++;
		if (r_ctx.flip == FlipMode::VALENCE && cand.type == OpType::Flip) 
            energy_ops_since_flip = 0;
		else energy_ops_since_flip++;
		report_progress(cand.type, static_cast<int>(pq.size() + flip_pq.size()));
		enqueue_affected_region(pq, flip_target, affected);
		check_queue();
	}

	mesh.garbage_collection();
	set_metrics();
	publish_queue_state(0);
	p_ctx.update([&](ProgressState& state) {
		if (hit_operation_limit) state.termination_reason = TerminationReason::OperationLimit;
		else state.termination_reason = TerminationReason::EmptyQueues;
	});
}

void RemesherPrioGlobal::remesh() {
	single_iteration();
	if (progress_callback) progress_callback(true);
}

} // namespace ba
