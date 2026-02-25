#include "HighsLPSolver.h"

#include <vector>
#include <stdexcept>
#include <cmath>    // std::llround
#include <limits>

struct Interval {
    int tool;      // 0-based tool index
    int startJob;  // 0-based job index (конец предыдущего блока)
    int endJob;    // 0-based job index (начало следующего блока)
};

long long HighsLPSolver::ComputeSolution(const TestInstance& inst) {
    const int numJobs  = inst.N;
    const int numTools = inst.M;
    const int C        = inst.C;

    if (numJobs <= 0 || numTools <= 0 || C <= 0) {
        return 0;
    }

    const auto& T    = inst.job_requirements; // vector< set<int> >, |T| = N
    const auto& cost = inst.tool_costs;       // size M, 0-based

    if ((int)T.size() != numJobs) {
        throw std::runtime_error("HighsLPSolver: job_requirements size mismatch with N");
    }
    if ((int)cost.size() != numTools) {
        throw std::runtime_error("HighsLPSolver: tool_costs size mismatch with M");
    }

    // === 1. Считаем n_t и occ[t] ТАК ЖЕ, как в BuilderCP ===
    //
    // n_t: число блоков подряд идущих использований инструмента t.
    // Здесь инструменты в T хранятся как 1-based, поэтому аккуратно работаем с индексами.
    std::vector<int> n(numTools, 0);                   // 0-based по t
    std::vector<std::vector<int>> occ(numTools);       // occ[t] = список работ (0-based), где нужен t

    for (int j = 0; j < numJobs; ++j) {
        for (int t1based : T[j]) {
            if (t1based < 1 || t1based > numTools) {
                throw std::runtime_error("HighsLPSolver: tool id out of range");
            }
            int t = t1based - 1; // 0-based индекс инструмента

            // сохраняем появление
            occ[t].push_back(j);

            // считаем только "первое появление" блока:
            // либо первая работа, либо в предыдущей работе t не требовался
            if (j == 0 || !T[j - 1].count(t1based)) {
                n[t]++;
            }
        }
    }

    // Ctot = sum_t c_t * n_t (аналогично BuilderCP)
    long long Ctot = 0;
    for (int t = 0; t < numTools; ++t) {
        Ctot += 1LL * cost[t] * n[t];
    }

    // === 2. Строим интервалы (только между БЛОКАМИ, как в BuilderCP) ===
    //
    // Для каждого инструмента t:
    //   occ[t] = { j0, j1, j2, ... } (0-based, отсортировано по j)
    //   Если между соседними появлениями есть разрыв (curIdx > prevIdx + 1),
    //   то создаём интервал (prevIdx, curIdx).
    //
    // Это соответствует рёбрам переноса из BuilderCP.
    std::vector<Interval> intervals;
    intervals.reserve(numJobs * numTools); // грубая верхняя оценка

    for (int t = 0; t < numTools; ++t) {
        const auto& v = occ[t];
        if (v.empty()) continue;

        for (size_t k = 1; k < v.size(); ++k) {
            int prevIdx = v[k - 1]; // конец блока
            int curIdx  = v[k];     // начало следующего блока

            // если работы подряд, это один блок, интервала нет
            if (curIdx == prevIdx + 1) {
                continue;
            }

            Interval e;
            e.tool     = t;
            e.startJob = prevIdx;   // последний job блока
            e.endJob   = curIdx;    // первый job следующего блока
            intervals.push_back(e);
        }
    }

    const int numVars = static_cast<int>(intervals.size());
    if (numVars == 0) {
        // Нет ни одного разрыва -> нечего "проносить" -> экономии нет
        // Минимальная стоимость = Ctot
        return Ctot;
    }

    // === 3. Для каждого job i строим список интервалов, которые его "накрывают" ===
    //
    // Интервал (startJob, endJob) "покрывает" работы i = startJob+1, ..., endJob-1
    std::vector<std::vector<int>> job_to_edges(numJobs);

    for (int e = 0; e < numVars; ++e) {
        const Interval& in = intervals[e];
        for (int i = in.startJob + 1; i < in.endJob; ++i) {
            job_to_edges[i].push_back(e);
        }
    }

    // === 4. Строим LP для HiGHS ===
    //
    // Переменные x_e :
    //   0 <= x_e <= 1
    //   cost(e) = cost[tool(e)]  (мы МАКСИМИЗИРУЕМ экономию — сколько блоков вставок сняли)
    std::vector<double> col_cost(numVars);
    std::vector<double> col_lower(numVars, 0.0);
    std::vector<double> col_upper(numVars, 1.0);

    for (int e = 0; e < numVars; ++e) {
        int t = intervals[e].tool;
        col_cost[e] = static_cast<double>(cost[t]);
    }

    // Ограничения по работам:
    //   sum_{e: i in e} x_e <= C - |T_i|
    const int numRows = numJobs;
    std::vector<double> row_lower(numRows);
    std::vector<double> row_upper(numRows);

    for (int i = 0; i < numJobs; ++i) {
        int freeCap = C - static_cast<int>(T[i].size());
        if (freeCap < 0) {
            throw std::runtime_error("HighsLPSolver: |T_i| > C, instance infeasible");
        }
        row_lower[i] = -std::numeric_limits<double>::infinity();
        row_upper[i] = static_cast<double>(freeCap);
    }

    // Матрица A в формате "по столбцам"
    std::vector<std::vector<int>> col_rows(numVars);
    for (int i = 0; i < numJobs; ++i) {
        for (int e : job_to_edges[i]) {
            col_rows[e].push_back(i);
        }
    }

    std::vector<HighsInt> astart(numVars + 1);
    std::vector<HighsInt> aindex;
    std::vector<double>   avalue;

    aindex.reserve(numVars * 4);
    avalue.reserve(numVars * 4);

    HighsInt nnz = 0;
    for (int e = 0; e < numVars; ++e) {
        astart[e] = nnz;
        for (int row : col_rows[e]) {
            aindex.push_back(row);
            avalue.push_back(1.0);
            ++nnz;
        }
    }
    astart[numVars] = nnz;

    // === 5. Отдаём модель в HiGHS ===
    Highs highs;

    // Глушим вывод HiGHS
    highs.setOptionValue("output_flag", false);
    highs.setOptionValue("log_to_console", false);

    HighsLp lp;
    lp.num_col_   = numVars;
    lp.num_row_   = numRows;
    lp.col_cost_  = col_cost;
    lp.col_lower_ = col_lower;
    lp.col_upper_ = col_upper;
    lp.row_lower_ = row_lower;
    lp.row_upper_ = row_upper;

    lp.a_matrix_.start_ = astart;
    lp.a_matrix_.index_ = aindex;
    lp.a_matrix_.value_ = avalue;

    lp.sense_ = ObjSense::kMaximize; // максимизируем экономию

    HighsStatus status = highs.passModel(lp);
    if (status != HighsStatus::kOk) {
        throw std::runtime_error("HiGHS: passModel failed");
    }

    status = highs.run();
    if (status != HighsStatus::kOk) {
        throw std::runtime_error("HiGHS: solve failed");
    }

    const HighsModelStatus model_status = highs.getModelStatus();
    if (model_status != HighsModelStatus::kOptimal) {
        throw std::runtime_error("HiGHS: model status is not optimal");
    }

    const double obj = highs.getObjectiveValue(); // maxSavings (double)

    long long maxSavings = std::llround(obj);   // должно быть целое
    long long totalCost  = Ctot - maxSavings;   // как ты и написал: Ctot - значение солвера

    return totalCost;
}
