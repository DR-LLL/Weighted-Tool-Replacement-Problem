#pragma once
#include "IBuildGraph.h"
#include "string"
class PFBuilder final : public IBuildGraph {
public:
    DAG build_from_instance(const TestInstance& instance, int K = 0) const override;
    PFBuilder(){name = "PF";}
};
