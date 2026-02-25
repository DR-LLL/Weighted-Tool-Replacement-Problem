#include "BuilderPF.h"
#include <vector>
#include <utility>
#include <algorithm>
#include <cassert>
using namespace std;

DAG PFBuilder::build_from_instance(const TestInstance& instance, int K) const {
    DAG dag;

    const int M = instance.M;
    const int N = instance.N;
    const int C = instance.C;
    const auto& jobs = instance.job_requirements;

    vector<pair<int,int>> reqs;
    reqs.reserve(1024);
    for (int r = 0; r < N; ++r) {
        vector<int> tools(jobs[r].begin(), jobs[r].end());
        sort(tools.begin(), tools.end());
        for (int tool : tools) reqs.emplace_back(r + 1, tool);
    }
    const int R = (int)reqs.size();

    int source = dag.add_vertex();
    for (int i = 0; i < C; ++i) dag.add_vertex();
    for (int i = 0; i < R; ++i) dag.add_vertex();
    for (int i = 0; i < R; ++i) dag.add_vertex();
    int sink = dag.add_vertex();

    dag.set_source(source);
    dag.set_sink(sink);

    assert((int)instance.tool_costs.size() == M);
    auto D = [&](int i, int j)->int { return (i==j) ? 0 : (int)instance.tool_costs[j-1]; };

    if (K <= 0) {
        long long mx = 0;
        for (auto v: instance.tool_costs) mx = max(mx, v);
        long long KK = 1LL * R * max(1LL, mx) + 1;
        if (KK > INT_MAX/2) KK = INT_MAX/2;
        K = (int)KK;
    }

    // source -> slots
    for (int k = 0; k < C; ++k) dag.add_edge(source, 1 + k, 0, 1);

    // slots -> each request
    for (int k = 0; k < C; ++k) {
        int slot_node = 1 + k;
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
