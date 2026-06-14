#pragma once

#include "remesher/remesher_base.h"

namespace ba {

class PhaseBasedRemesher : public Remesher {
protected:
    virtual void split_long_edges() = 0;
    virtual void collapse_short_edges() = 0;
    virtual void flip_edges() = 0;
    virtual void smooth_vertices() = 0;

public:
    using Remesher::Remesher;

    void single_iteration() override {
        split_long_edges();
        collapse_short_edges();
        flip_edges();
        smooth_vertices();
        set_metrics();
        mesh.garbage_collection();
    }

    void run_debug_phase(OpType type) {
        switch (type) {
            case OpType::Split: split_long_edges(); break;
            case OpType::Collapse: collapse_short_edges(); break;
            case OpType::Flip: flip_edges(); break;
            case OpType::Smooth: smooth_vertices(); break;
        }
        set_metrics();
    }
};

} // namespace ba
