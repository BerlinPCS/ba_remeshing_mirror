#pragma once
#include "remesher/evaluation_strategy.h"

namespace ba {

class SplitAvgEvaluation : public EvaluationStrategy {
public:
    SplitAvgEvaluation() : EvaluationStrategy() {}

    double split_score(const Mesh& mesh, Edge e) override;
};

} // namespace ba