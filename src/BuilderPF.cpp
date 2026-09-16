#include "BuilderPF.h"
#include <vector>
#include <utility>
#include <algorithm>
#include <limits>
#include <stdexcept>
using namespace std;

DAG PFBuilder::build_from_instance(const TestInstance& instance, int K) const {
    DAG dag;

    const int M = instance.M;
    const int N = instance.N;
    const int C = instance.C;
    const auto& jobs = instance.job_requirements;

    if (N < 0 || M <= 0 || C <= 0) {
        throw runtime_error("PFBuilder: invalid instance dimensions");
    }
    if ((int)jobs.size() != N) {
        throw runtime_error("PFBuilder: job_requirements size mismatch with N");
    }
    if ((int)instance.tool_costs.size() != M) {
        throw runtime_error("PFBuilder: tool_costs size mismatch with M");
    }

    vector<pair<int,int>> reqs;
    reqs.reserve(1024);
    for (int r = 0; r < N; ++r) {
        if ((int)jobs[r].size() > C) {
            throw runtime_error("PFBuilder: |J_i| > C, instance infeasible");
        }
        vector<int> tools(jobs[r].begin(), jobs[r].end());
        sort(tools.begin(), tools.end());
        for (int tool : tools) {
            if (tool < 1 || tool > M) {
                throw runtime_error("PFBuilder: tool id out of range");
            }
            reqs.emplace_back(r + 1, tool);
        }
    }
    const int R = (int)reqs.size();

    int source = dag.add_vertex();
    for (int i = 0; i < C; ++i) dag.add_vertex();
    for (int i = 0; i < R; ++i) dag.add_vertex();
    for (int i = 0; i < R; ++i) dag.add_vertex();
    int sink = dag.add_vertex();

    dag.set_source(source);
    dag.set_sink(sink);

    auto D = [&](int i, int j)->int { return (i==j) ? 0 : (int)instance.tool_costs[j-1]; };

    if (K <= 0) {
        long long mx = 0;
        for (auto v: instance.tool_costs) mx = max(mx, v);
        long long KK = 1LL * R * max(1LL, mx) + 1;
        if (KK > std::numeric_limits<int>::max()/2) KK = std::numeric_limits<int>::max()/2;
        K = (int)KK;
    }

    // source -> slots
    for (int k = 0; k < C; ++k) dag.add_edge(source, 1 + k, 0, 1);

    // slots -> each request; slot -> sink models an unused virtual slot.
    for (int k = 0; k < C; ++k) {
        int slot_node = 1 + k;
        dag.add_edge(slot_node, sink, 0, 1);
        for (int i = 0; i < R; ++i) {
            int t = reqs[i].second;
            dag.add_edge(slot_node, 1 + C + i, (int)instance.tool_costs[t - 1], 1);
        }
    }

    // req -> copy (-K)
    for (int i = 0; i < R; ++i)
        dag.add_edge(1 + C + i, 1 + C + R + i, -K, 1, false);

    // copy -> sink (0)
    for (int i = 0; i < R; ++i)
        dag.add_edge(1 + C + R + i, sink, 0, 1);

    // копия -> запросы последующих job-групп
    vector<int> job_of(R), tool_of(R);
    for (int i = 0; i < R; ++i) { job_of[i] = reqs[i].first; tool_of[i] = reqs[i].second; }

    for (int i = 0; i < R; ++i) {
        int ti = tool_of[i];
        int u  = 1 + C + R + i;
        for (int j = i+1; j < R; ++j) {
            if (job_of[j] <= job_of[i]) continue;
            int tj = tool_of[j];
            int v  = 1 + C + j;
            dag.add_edge(u, v, D(ti, tj), 1);
        }
    }
    return dag;
}
