#pragma once

#include "remesher/phase_based_remesher.h"

namespace ba {

class RemesherStandard : public PhaseBasedRemesher {
protected:
    void split_long_edges() override;
    void collapse_short_edges() override;
    void flip_edges() override;
    void smooth_vertices() override;

public:
	using PhaseBasedRemesher::PhaseBasedRemesher;
};

} // namespace ba
