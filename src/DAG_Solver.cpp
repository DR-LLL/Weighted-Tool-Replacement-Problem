#include "DAG_Solver.h"
#include "DAG.h"
#include <iostream>

long long DAG_Solver::ComputeSolution(const TestInstance& instance) {

    DAG dag = builder_.build_from_instance(instance, K_);

    auto result = dag.compute_max_flow_min_cost();

    loadings = result.second;
    for (int i = 0; i < instance.N; i++) {
        for (auto j : instance.job_requirements[i]) {
            loadings[i].push_back(j);

        }
    }

    return static_cast<long long>(result.first);
}
