#pragma once
#include <pmp/surface_mesh.h>
#include "core/types.h"

namespace ba {

class EvaluationStrategy {
public:
    virtual ~EvaluationStrategy() = default;

    virtual double split_score(const Mesh& mesh, Edge e);
    virtual double collapse_score(const Mesh& mesh, Edge e);
    virtual int flip_score(const Mesh& mesh, Edge e);
    virtual double smooth_score(const Mesh& mesh, Vertex v);

    EvaluationStrategy(RemesherSettings& ctx) : ctx(ctx) {}

protected:
    RemesherSettings& ctx;
};

} // namespace ba
