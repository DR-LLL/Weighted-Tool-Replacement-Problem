#pragma once
#include <vector>
#include <memory>
#include <limits>
#include <stdexcept>

class DAG {
public:

    struct Arc {
        int flow = 0;
        int from = -1;
        int to   = -1;
        int tool = -1;
        virtual ~Arc() = default;

        virtual int        residual_cap() const = 0;
        virtual long long cost()         const = 0;
        virtual void       augment(int add)     = 0;
        virtual bool       is_forward()   const = 0;

        int tail() const { return from; }
        int head() const { return to;   }
    };

    struct ForwardArc;
    struct BackwardArc;


    struct ForwardArc : Arc {
        int capacity      = 0;
        int weight        = 0;
        bool in_objective = false;
        BackwardArc* mate = nullptr;


        ForwardArc(int u, int v, int cap, int w, bool in_obj, int tool);

        int        residual_cap() const override;
        long long  cost()         const override;
        void       augment(int add)      override;
        bool       is_forward()   const override { return true; }
    };


    struct BackwardArc : Arc {
        ForwardArc* mate = nullptr;

        BackwardArc(int u, int v, ForwardArc* m);

        int        residual_cap() const override;
        long long  cost()         const override;
        void       augment(int add)      override;
        bool       is_forward()   const override { return false; }
    };

public:


    int  add_vertex();


    void add_edge(int from, int to, int weight = 0, int capacity = 0, bool objective = true, int tool = -1);


    void add_edge_marked(int from, int to, int weight, int capacity, bool in_objective);

    void set_source(int source);
    void set_sink(int sink);


    std::pair<int, std::vector<std::vector<int>>>  compute_max_flow_min_cost();

    // Проверка, что по прямым дугам граф ацикличен
    bool is_acyclic() const;

private:

    std::vector<std::vector<Arc*>> g_;

    std::vector<std::unique_ptr<Arc>> storage_;


    void add_arc_pair_(int u, int v, int cap, int w, bool in_objective, int tool = -1);
    bool has_cycle_dfs_(int v, std::vector<int>& vis) const;


    void init_potentials_by_topo_(std::vector<long long>& pi) const;
    bool topological_sort_forward_(std::vector<int>& order) const;

public:

    int vertex_count = 0;
    int source_index = -1;
    int sink_index   = -1;
};
