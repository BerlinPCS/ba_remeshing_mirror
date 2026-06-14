#pragma once

#include "remesher/remesher_base.h"

namespace ba {

class RemesherPrioGlobal : public Remesher {
private:
    QueueStats queue_stats;
    int processed_candidates = 0;

    void enqueue_length_ops(OpQueue& pq, Edge e);
    void enqueue_flip(OpQueue& flip_pq, Edge e);
    void enqueue_smooth(OpQueue& pq, Vertex v);
    void enqueue_affected_region(OpQueue& pq, OpQueue& flip_pq, const std::vector<Vertex>& vs);
    void populate_queues(OpQueue& pq, OpQueue& flip_pq);
    void publish_queue_state(int queue_size);
    bool is_stale(const OpCandidate& cand) const;
    bool priority_changed(double old_priority, double new_priority) const;

public:
	RemesherPrioGlobal(Mesh& m, RemesherSettings& r_ctx, SyncState<ProgressState>& p_ctx) : Remesher(m, r_ctx, p_ctx) {}

	void single_iteration() override;
    void remesh() override;
};

} // namespace ba
