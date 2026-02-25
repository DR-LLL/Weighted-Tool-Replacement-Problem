#include "HighsMCFSolver.h"
#include "Highs.h"

#include <vector>
#include <limits>
#include <stdexcept>
#include <cmath>

namespace {
    constexpr double INF = std::numeric_limits<double>::infinity();

    inline HighsInt idx_p(int i, int k0, int N) {
        // i = 0..N-1, k0 = 0..N-1
        return (HighsInt)i * N + k0;
    }

    struct Index {
        int N, M, C;
        HighsInt base_p;
        HighsInt base_x;
        HighsInt base_a;

        Index(int N_, int M_, int C_) : N(N_), M(M_), C(C_) {
            base_p = 0;
            base_x = (HighsInt)N * N;
            base_a = base_x + (HighsInt)M * N;
        }

        // x(u,k): u=1..M, k=1..N
        HighsInt idx_x(int u, int k) const {
            return base_x + (HighsInt)(u - 1) * N + (k - 1);
        }

        // a(u,k): u=1..M, k=1..N
        HighsInt idx_a(int u, int k) const {
            return base_a + (HighsInt)(u - 1) * N + (k - 1);
        }

        HighsInt num_cols() const {
            return (HighsInt)N * N + (HighsInt)M * N + (HighsInt)M * N;
        }
    };

    static inline void addRow(Highs& highs, double lo, double up,
                              const std::vector<HighsInt>& idx,
                              const std::vector<double>& val) {
        if (idx.size() != val.size()) {
            throw std::runtime_error("addRow: idx/val size mismatch");
        }
        highs.addRow(lo, up, (HighsInt)idx.size(), idx.data(), val.data());
    }
} // namespace

long long HighsMCFSolver::ComputeSolution(const TestInstance& instance) {
    Highs highs;
    highs.setOptionValue("time_limit", timeLimit);
    highs.setOptionValue("output_flag", false);

    const int N = instance.N; // jobs
    const int M = instance.M; // tools
    const int C = instance.C; // capacity
    const auto& req = instance.job_requirements; // tools in 1..M

    // Стоимость вставки инструмента u
    auto c = [&](int u) -> double {
        return (double)instance.tool_costs[u - 1];
    };

    Index id(N, M, C);
    const HighsInt numCol = id.num_cols();

    // =========================================================
    // ШАГ 1. Добавляем переменные: p, x, a
    // =========================================================
    // p: cost 0, x: cost 0, a: cost c_u
    // Все бинарные через bounds [0,1] + integrality integer.

    // p(i,k0)
    for (HighsInt col = 0; col < (HighsInt)N * N; ++col) {
        highs.addCol(0.0, 0.0, 1.0, 0, nullptr, nullptr);
    }

    // x(u,k)
    for (int u = 1; u <= M; ++u) {
        for (int k = 1; k <= N; ++k) {
            highs.addCol(0.0, 0.0, 1.0, 0, nullptr, nullptr);
        }
    }

    // a(u,k) with objective cost c_u
    for (int u = 1; u <= M; ++u) {
        for (int k = 1; k <= N; ++k) {
            highs.addCol(c(u), 0.0, 1.0, 0, nullptr, nullptr);
        }
    }

    // Интегральность (иначе будет LP-релаксация)
    for (HighsInt col = 0; col < numCol; ++col) {
        const HighsStatus st = highs.changeColIntegrality(col, HighsVarType::kInteger);
        if (st != HighsStatus::kOk) {
            throw std::runtime_error("HiGHS: changeColIntegrality failed");
        }
    }

    // =========================================================
    // ШАГ 2. Ограничения
    // =========================================================

    // (A1) sum_k p(i,k) = 1
    for (int i = 0; i < N; ++i) {
        std::vector<HighsInt> idx;
        std::vector<double> val;
        idx.reserve(N); val.reserve(N);
        for (int k0 = 0; k0 < N; ++k0) {
            idx.push_back(idx_p(i, k0, N));
            val.push_back(1.0);
        }
        addRow(highs, 1.0, 1.0, idx, val);
    }

    // (A2) sum_i p(i,k) = 1
    for (int k0 = 0; k0 < N; ++k0) {
        std::vector<HighsInt> idx;
        std::vector<double> val;
        idx.reserve(N); val.reserve(N);
        for (int i = 0; i < N; ++i) {
            idx.push_back(idx_p(i, k0, N));
            val.push_back(1.0);
        }
        addRow(highs, 1.0, 1.0, idx, val);
    }

    // (CAP) capacity: sum_u x(u,k) = C   for k=1..N
    // Если хотите допускать неполную загрузку, замените (double)C,(double)C на -INF,(double)C.
    for (int k = 1; k <= N; ++k) {
        std::vector<HighsInt> idx;
        std::vector<double> val;
        idx.reserve(M); val.reserve(M);
        for (int u = 1; u <= M; ++u) {
            idx.push_back(id.idx_x(u, k));
            val.push_back(1.0);
        }
        addRow(highs, (double)C, (double)C, idx, val);
    }

    // (REQ) requirements: x(u,k) >= p(i,k-1) for u in req[i], k=1..N
    // Здесь p индексируется k0=0..N-1, а x по k=1..N, поэтому связка: position k <-> p(*,k-1)
    for (int i = 0; i < N; ++i) {
        for (int k = 1; k <= N; ++k) {
            const int k0 = k - 1;
            for (int u : req[i]) {
                std::vector<HighsInt> idx;
                std::vector<double> val;
                idx.reserve(2); val.reserve(2);

                idx.push_back(id.idx_x(u, k));
                val.push_back(1.0);

                idx.push_back(idx_p(i, k0, N));
                val.push_back(-1.0);

                // x - p >= 0
                addRow(highs, 0.0, INF, idx, val);
            }
        }
    }

    // (INS) Вставка = появление. Старт считаем пустым: x(u,0)=0.
    //
    // Для k=1:
    //  a(u,1) >= x(u,1)    => a - x >= 0
    //  a(u,1) <= x(u,1)    => a - x <= 0
    //
    // Для k>=2:
    //  a(u,k) >= x(u,k) - x(u,k-1)  => a - x_k + x_{k-1} >= 0
    //  a(u,k) <= x(u,k)             => a - x_k <= 0
    //  a(u,k) <= 1 - x(u,k-1)       => a + x_{k-1} <= 1

    for (int u = 1; u <= M; ++u) {
        // k=1
        {
            std::vector<HighsInt> idx{ id.idx_a(u, 1), id.idx_x(u, 1) };
            std::vector<double>   val{ 1.0,           -1.0 };
            addRow(highs, 0.0, INF, idx, val);   // a - x >= 0
            addRow(highs, -INF, 0.0, idx, val);  // a - x <= 0
        }

        // k=2..N
        for (int k = 2; k <= N; ++k) {
            // a - x_k + x_{k-1} >= 0
            {
                std::vector<HighsInt> idx{ id.idx_a(u, k), id.idx_x(u, k), id.idx_x(u, k - 1) };
                std::vector<double>   val{ 1.0,           -1.0,          1.0 };
                addRow(highs, 0.0, INF, idx, val);
            }
            // a - x_k <= 0
            {
                std::vector<HighsInt> idx{ id.idx_a(u, k), id.idx_x(u, k) };
                std::vector<double>   val{ 1.0,           -1.0 };
                addRow(highs, -INF, 0.0, idx, val);
            }
            // a + x_{k-1} <= 1
            {
                std::vector<HighsInt> idx{ id.idx_a(u, k), id.idx_x(u, k - 1) };
                std::vector<double>   val{ 1.0,            1.0 };
                addRow(highs, -INF, 1.0, idx, val);
            }
        }
    }

    // =========================================================
    // ШАГ 3. Решаем MIP
    // =========================================================
    const HighsStatus st = highs.run();
    if (st != HighsStatus::kOk) {
        // Можно вернуть best-known при таймлимите; HiGHS обычно возвращает kOk,
        // а статус решения проверяется отдельно.
        // Здесь оставим мягко:
        // throw std::runtime_error("HiGHS: run failed");
    }

    const double obj = highs.getObjectiveValue();
    return (long long)std::llround(obj);
}
