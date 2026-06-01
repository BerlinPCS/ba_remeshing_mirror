#pragma once

#include <queue>
#include <vector>
#include "remesher/remesher_base.h"

namespace ba {

class RemesherPrioGlobal : public Remesher {
private:
    int flip_frequency = 5;

    void enqueue_neighborhood(Vertex v, OpQueue& pq);

public:
	RemesherPrioGlobal(Mesh& m) : Remesher(m) {}

	void single_iteration() override;

    // Global Prio Queue doesnt use single operations, instead overriding single_iteration()
    int split_long_edges() override { return 0; }
    int collapse_short_edges() override { return 0; }
    int flip_edges() override { return 0; }
    int smooth_vertices() override { return 0; }

    void set_flip_frequency(int f) { flip_frequency = f; }
    int get_flip_frequency() const { return flip_frequency; }
};

} // namespace ba
