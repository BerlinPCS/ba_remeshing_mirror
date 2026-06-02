#pragma once
#include "remesher/evaluation_strategy.h"

namespace ba {

class SplitMaxEvaluation : public EvaluationStrategy {
public:
    SplitMaxEvaluation() : EvaluationStrategy() {}

    double split_score(const Mesh& mesh, Edge e) override;
};

} // namespace ba