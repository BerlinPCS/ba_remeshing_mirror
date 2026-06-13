#pragma once

#include "remesher/remesher_base.h"

namespace ba {

class RemesherPrioGlobal : public Remesher {
private:
    void enqueue_length_ops(OpQueue& pq, Edge e);
    void enqueue_flip(OpQueue& flip_pq, Edge e);
    void enqueue_smooth(OpQueue& pq, Vertex v);
    void enqueue_affected_region(OpQueue& pq, OpQueue& flip_pq, const std::vector<Vertex>& vs);

public:
	RemesherPrioGlobal(Mesh& m, RemesherSettings& r_ctx, SyncState<ProgressState>& p_ctx) : Remesher(m, r_ctx, p_ctx) {}

	void single_iteration() override;
    void remesh() override;

    // Global Prio Queue doesnt use single operations, instead overriding single_iteration()
    void split_long_edges() override { /* do nothing */ } 
    void collapse_short_edges() override { /* do nothing */ } 
    void flip_edges() override { /* do nothing */ } 
    void smooth_vertices() override { /* do nothing */ } 
};

} // namespace ba
