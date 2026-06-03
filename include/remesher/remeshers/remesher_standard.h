#pragma once

#include "remesher/remesher_base.h"

namespace ba {

class RemesherStandard : public Remesher {
public:
	RemesherStandard(Mesh& m, std::shared_ptr<EvaluationStrategy> evaluator) : Remesher(m, evaluator) {}

    //Base Operations
    void split_long_edges() override;
    void collapse_short_edges() override;
    void flip_edges() override;
    void smooth_vertices() override;

};

} // namespace ba