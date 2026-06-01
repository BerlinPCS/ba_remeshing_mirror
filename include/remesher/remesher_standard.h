#pragma once

#include "remesher/remesher_base.h"

namespace ba {

class RemesherStandard : public Remesher {
public:
	RemesherStandard(Mesh& m) : Remesher(m) {}

    //Base Operations
    int split_long_edges() override;
    int collapse_short_edges() override;
    int flip_edges() override;
    int smooth_vertices() override;

};

} // namespace ba