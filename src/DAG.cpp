#include "DAG.h"
#include <queue>
#include <algorithm>
#include <iostream>



DAG::ForwardArc::ForwardArc(int u, int v, int cap, int w, bool in_obj, int tool2) {
    from         = u;
    to           = v;
    capacity     = cap;
    weight       = w;
    in_objective = in_obj;
    flow         = 0;
    mate         = nullptr;

    tool = tool2;


}

int DAG::ForwardArc::residual_cap() const {
    return capacity - flow;
}

long long DAG::ForwardArc::cost() const {
    return static_cast<long long>(weight);
}

void DAG::ForwardArc::augment(int add) {
    flow += add;
}

DAG::BackwardArc::BackwardArc(int u, int v, ForwardArc* m) {
    from = u;
    to   = v;
    mate = m;
}

int DAG::BackwardArc::residual_cap() const {
    return mate->flow;
}

long long DAG::BackwardArc::cost() const {
    return -static_cast<long long>(mate->weight);
}

void DAG::BackwardArc::augment(int add) {
    mate->flow -= add;
}



int DAG::add_vertex() {
    if ((int)g_.size() <= vertex_count) {
        g_.push_back({});
    }
    return vertex_count++;
}

void DAG::set_source(int s) {
    if (s < 0 || s >= vertex_count) throw std::out_of_range("bad source");
    source_index = s;
}

void DAG::set_sink(int t) {
    if (t < 0 || t >= vertex_count) throw std::out_of_range("bad sink");
    sink_index = t;
}

void DAG::add_arc_pair_(int u, int v, int cap, int w, bool in_objective, int tool) {

    auto fwd = std::make_unique<ForwardArc>(u, v, cap, w, in_objective, tool);
    auto bwd = std::make_unique<BackwardArc>(v, u, fwd.get());
    fwd->mate = bwd.get();

    g_[u].push_back(fwd.get());
    g_[v].push_back(bwd.get());

    storage_.push_back(std::move(fwd));
    storage_.push_back(std::move(bwd));
}

void DAG::add_edge(int from, int to, int weight, int capacity, bool objective, int tool) {
    if (from < 0 || from >= vertex_count) throw std::out_of_range("from");
    if (to   < 0 || to   >= vertex_count) throw std::out_of_range("to");


    bool in_objective = objective;

    add_arc_pair_(from, to, capacity, weight, in_objective, tool);
}

void DAG::add_edge_marked(int from, int to, int weight, int capacity, bool in_objective) {
    if (from < 0 || from >= vertex_count) throw std::out_of_range("from");
    if (to   < 0 || to   >= vertex_count) throw std::out_of_range("to");

    add_arc_pair_(from, to, capacity, weight, in_objective);
}



bool DAG::has_cycle_dfs_(int v, std::vector<int>& vis) const {
    vis[v] = 1;
    for (Arc* a : g_[v]) {
        if (!a->is_forward()) continue;
        int u = a->head();
        if (vis[u] == 0) {
            if (has_cycle_dfs_(u, vis)) return true;
        } else if (vis[u] == 1) {
            return true;
        }
    }
    vis[v] = 2;
    return false;
}

bool DAG::is_acyclic() const {
    std::vector<int> vis(vertex_count, 0);
    for (int v = 0; v < vertex_count; ++v) {
        if (vis[v] == 0) {
            if (has_cycle_dfs_(v, vis)) return false;
        }
    }
    return true;
}



bool DAG::topological_sort_forward_(std::vector<int>& order) const {
    order.clear();
    order.reserve(vertex_count);

    std::vector<int> indeg(vertex_count, 0);
    for (int u = 0; u < vertex_count; ++u) {
        for (Arc* a : g_[u]) {
            if (a->is_forward()) {
                ++indeg[a->head()];
            }
        }
    }

    std::queue<int> q;
    for (int i = 0; i < vertex_count; ++i) {
        if (indeg[i] == 0) q.push(i);
    }

    while (!q.empty()) {
        int u = q.front(); q.pop();
        order.push_back(u);
        for (Arc* a : g_[u]) {
            if (!a->is_forward()) continue;
            int v = a->head();
            if (--indeg[v] == 0) q.push(v);
        }
    }

    return (int)order.size() == vertex_count;
}



void DAG::init_potentials_by_topo_(std::vector<long long>& pi) const {
    const long long INF = (1LL << 60);
    pi.assign(vertex_count, 0);

    std::vector<int> topo;
    if (!topological_sort_forward_(topo)) {
        return;
    }

    std::vector<long long> dist(vertex_count, INF);
    if (source_index < 0 || source_index >= vertex_count) return;
    dist[source_index] = 0;

    for (int u : topo) {
        if (dist[u] == INF) continue;
        for (Arc* a : g_[u]) {
            if (!a->is_forward()) continue;
            auto fwd = static_cast<ForwardArc*>(a);
            if (fwd->residual_cap() <= 0) continue;
            int v = a->head();
            long long nd = dist[u] + a->cost();
            if (nd < dist[v]) dist[v] = nd;
        }
    }

    for (int v = 0; v < vertex_count; ++v) {
        if (dist[v] < INF) pi[v] = dist[v];
    }
}



std::pair<int, std::vector<std::vector<int>>> DAG::compute_max_flow_min_cost() {

    if (source_index == -1 || sink_index == -1)
        throw std::logic_error("source/sink not set");
    if (!is_acyclic())
        throw std::logic_error("input graph must be a DAG (по прямым дугам)");

    using ll = long long;
    const ll INF = (1LL << 60);
    const int N  = vertex_count;


    std::vector<ll> pi(N, 0), dist(N);
    init_potentials_by_topo_(pi);

    std::vector<Arc*> parent(N, nullptr);

    auto dijkstra_with_potentials = [&]() -> bool {
        std::fill(dist.begin(),   dist.end(),   INF);
        std::fill(parent.begin(), parent.end(), nullptr);
        dist[source_index] = 0;

        using P = std::pair<ll,int>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
        pq.push({0, source_index});

        while (!pq.empty()) {
            auto [d,u] = pq.top(); pq.pop();
            if (d != dist[u]) continue;

            for (Arc* a : g_[u]) {
                if (a->residual_cap() <= 0) continue;
                int v  = a->head();
                ll rc  = a->cost() + pi[u] - pi[v];
                ll nd  = d + rc;
                if (nd < dist[v]) {
                    dist[v]   = nd;
                    parent[v] = a;
                    pq.push({nd, v});
                }
            }
        }

        if (dist[sink_index] == INF) return false;
        for (int v = 0; v < N; ++v) {
            if (dist[v] < INF) { pi[v] += dist[v]; }
        }


        return true;
    };

    auto bottleneck = [&]() -> int {
        int add = std::numeric_limits<int>::max();
        for (Arc* a = parent[sink_index]; a; a = parent[a->tail()]) {
            add = std::min(add, a->residual_cap());
        }
        return (add == std::numeric_limits<int>::max()) ? 0 : add;
    };

    auto augment_and_account = [&](int add, long long& result_cost) {
        for (Arc* a = parent[sink_index]; a; a = parent[a->tail()]) {
            a->augment(add);

            if (a->is_forward()) {
                auto f = static_cast<ForwardArc*>(a);
                if (f->in_objective) {
                    result_cost += 1LL * add * f->weight;
                }
            } else {
                auto b = static_cast<BackwardArc*>(a);
                auto f = b->mate;
                if (f->in_objective) {
                    result_cost -= 1LL * add * f->weight;
                }
            }
        }
    };

    long long result_cost = 0;
    while (dijkstra_with_potentials()) {
        int add = bottleneck();
        if (add <= 0) break;
        augment_and_account(add, result_cost);
    }

    if (result_cost > (ll)std::numeric_limits<int>::max()
        || result_cost < (ll)std::numeric_limits<int>::min())
        throw std::overflow_error("result doesn't fit into int");

    std::vector<std::vector<int>> loadings(N);
    bool needToPrint = false;
    bool needToFindLoadings = true;

    if (needToFindLoadings) {

        for (auto arcs : g_) {
            for (auto arc : arcs) {

                if (arc->is_forward() && (arc->tool != -1) && ((arc->flow) > 0)) {
                    for (int i = arc->from; i < arc->to; i++) {
                        loadings[i].push_back(arc->tool);
                    }
                }
            }
        }
    }


    return std::make_pair(static_cast<int>(result_cost), loadings);
}

