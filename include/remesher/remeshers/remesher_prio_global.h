#pragma once

#include "remesher/remesher_base.h"

namespace ba {

class RemesherPrioGlobal : public Remesher {
private:
    int flip_frequency = 5;
    bool separate_flip_queue = true;

    void enqueue_length_ops(OpQueue& pq, Edge e);
    void enqueue_flip(OpQueue& flip_pq, Edge e);
    void enqueue_smooth(OpQueue& pq, Vertex v);
    void enqueue_affected_region(OpQueue& pq, OpQueue& flip_pq, const std::vector<Vertex>& vs);

public:
	RemesherPrioGlobal(Mesh& m, std::shared_ptr<EvaluationStrategy> evaluator) : Remesher(m, evaluator) {}

	void single_iteration() override;
    void remesh() override;

    // Global Prio Queue doesnt use single operations, instead overriding single_iteration()
    void split_long_edges() override { /* do nothing */ } 
    void collapse_short_edges() override { /* do nothing */ } 
    void flip_edges() override { /* do nothing */ } 
    void smooth_vertices() override { /* do nothing */ } 

    void set_flip_frequency(int f) { flip_frequency = f; }
    int get_flip_frequency() const { return flip_frequency; }
    void set_separate_flip_queue(bool b) { separate_flip_queue = b; }
    bool get_separate_flip_queue() const { return separate_flip_queue; }
};

} // namespace ba
