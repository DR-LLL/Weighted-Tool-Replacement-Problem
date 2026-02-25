#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <vector>
#include <string>
#include <set>

struct TestInstance {
    int M = 0;  // number of tools
    int N = 0;  // number of jobs
    int C = 0;  // capacity
    std::vector<long long> tool_costs;
    std::vector<std::set<int>> job_requirements;

    static TestInstance load_from_file(const std::string& filename);
};

#endif // INSTANCE_HPP