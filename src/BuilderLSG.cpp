#include "BuilderLSG.h"

#include <vector>
#include <set>
#include <stdexcept>
#include <limits>
#include <iostream>

DAG LSGBuilder::build_from_instance(const TestInstance& inst, int K) const {
    const int N = inst.N;   // число работ
    const int M = inst.M;   // число инструментов
    const int C = inst.C;   // ёмкость магазина
    //std::cout << "inter2";
    if (C <= 0) {
        throw std::runtime_error("BuilderCP: capacity C must be positive");
    }

    const auto& T = inst.job_requirements;  // vector< set<int> >
    const auto& cost = inst.tool_costs;     // size M, индексы 0..M-1

    // Предполагаем, что проверки на соответствие размеров уже выполнены до вызова этой функции.

    std::vector<int> n(M + 1, 0);
    std::vector<std::vector<int>> occ(M + 1);

    // Собираем данные о появлениях инструментов
    for (int j = 0; j < N; ++j) {
        for (int t : T[j]) {
            if (t < 1 || t > M) {
                throw std::runtime_error("BuilderCP: tool id out of range");
            }

            occ[t].push_back(j);

            if (j == 0 || !T[j - 1].count(t)) {
                n[t]++;
            }
        }
    }

    // Вычисляем общую стоимость
    long long Ctot = 0;
    for (int t = 1; t <= M; ++t) {
        Ctot += 1LL * cost[t - 1] * n[t];
    }

    if (K <= 0) {
        long long Kll = Ctot + 1;
        long long maxInt = std::numeric_limits<int>::max() / 4;
        K = static_cast<int>(std::min(Kll, maxInt));
    }

    DAG dag;
    std::vector<int> box(N + 1);

    for (int j = 0; j <= N; ++j) {
        box[j] = dag.add_vertex();
    }
    const int final = dag.add_vertex();

    dag.set_source(box[0]);
    dag.set_sink(final);

    // Добавляем рёбра для требований и свободной ёмкости
    for (int j = 1; j <= N; ++j) {
        const int reqSz = static_cast<int>(T[j - 1].size());

        if (reqSz > C) {
            throw std::runtime_error("BuilderCP: |T_j| > C, instance infeasible");
        }

        if (reqSz > 0) {
            dag.add_edge(box[j - 1], box[j], -K, reqSz, false);
        }

        const int freeCap = C - reqSz;
        if (freeCap > 0) {
            dag.add_edge(box[j - 1], box[j], 0, freeCap);
        }
    }

    // Добавляем рёбра для инструментов с разрывами
    for (int t = 1; t <= M; ++t) {
        const auto& v = occ[t];
        if (v.empty()) continue;

        for (size_t k = 1; k < v.size(); ++k) {
            int prevIdx = v[k - 1];
            int curIdx  = v[k];

            if (curIdx == prevIdx + 1) continue;

            const int from = box[prevIdx + 1];
            const int to = box[curIdx];
            //std::cout << from << " --> " << to << " : " << t << std::endl;
            dag.add_edge(from, to, -cost[t - 1], 1, true, t);
        }
    }

    // Финальное ребро
    dag.add_edge(box[N], final, Ctot, 1);
    dag.add_edge(box[N], final, 0, C - 1);

    return dag;
}


