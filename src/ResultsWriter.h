#pragma once

#include <string>
#include <vector>

struct ExperimentResult {
    std::string solver;
    std::string test;
    int n, m, capacity, runs;
    double cost, best_cost, cost_std, millis;
};

void write_aggregated_results(const std::vector<ExperimentResult>& results,
                              const std::string& directory, bool ssp);
