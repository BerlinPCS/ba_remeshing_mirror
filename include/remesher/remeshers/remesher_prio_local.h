#pragma once

#include "remesher/remesher_base.h"

namespace ba {

class RemesherPrioLocal : public Remesher {

public:
	RemesherPrioLocal(Mesh& m, std::shared_ptr<EvaluationStrategy> evaluator) : Remesher(m, evaluator) {}

    //Base Operations
    void split_long_edges() override;
    void collapse_short_edges() override;
    void flip_edges() override;
    void smooth_vertices() override;

    void remesh() override;
};

} // namespace ba
