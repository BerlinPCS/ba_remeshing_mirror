#pragma once

#include "remesher/remesher_base.h"

namespace ba {

class RemesherPrioLocal : public Remesher {

public:
	RemesherPrioLocal(Mesh& m, RemesherSettings& r_ctx, SyncState<ProgressState>& p_ctx) : Remesher(m, r_ctx, p_ctx) {}

    //Base Operations
    void split_long_edges() override;
    void collapse_short_edges() override;
    void flip_edges() override;
    void smooth_vertices() override;

    void remesh() override;
};

} // namespace ba
