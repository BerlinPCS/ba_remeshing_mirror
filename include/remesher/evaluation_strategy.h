#pragma once
#include "core/types.h"
#include <pmp/surface_mesh.h>

namespace ba {

class EvaluationStrategy {
public:
	virtual ~EvaluationStrategy() = default;

	virtual OperationEvaluation split(const Mesh &mesh, Edge e) const;
	virtual OperationEvaluation collapse(const Mesh &mesh, Edge e) const;
	virtual OperationEvaluation flip(const Mesh &mesh, Edge e) const;
	virtual OperationEvaluation smooth(const Mesh &mesh, Vertex v) const;

	EvaluationStrategy(RemesherSettings &ctx) : ctx(ctx) {}

protected:
	RemesherSettings &ctx;
};

} // namespace ba
