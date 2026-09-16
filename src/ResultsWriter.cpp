#include "ResultsWriter.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace {
struct Group {
    std::string label;
    int n = 0, m = 0, capacity = 0;
    std::map<std::string, std::vector<const ExperimentResult*>> solvers;
};

std::string dataset_label(int n, int m, int c) {
    static const std::map<std::tuple<int, int, int>, std::string> labels = {
        {{10,10,4},"A1"}, {{10,10,5},"A2"}, {{10,10,6},"A3"}, {{10,10,7},"A4"},
        {{15,20,6},"B1"}, {{15,20,8},"B2"}, {{15,20,10},"B3"}, {{15,20,12},"B4"},
        {{30,40,15},"C1"}, {{30,40,17},"C2"}, {{30,40,20},"C3"}, {{30,40,25},"C4"},
        {{40,60,20},"D1"}, {{40,60,22},"D2"}, {{40,60,25},"D3"}, {{40,60,30},"D4"},
        {{50,75,25},"F1.1"}, {{50,75,30},"F1.2"}, {{50,75,35},"F1.3"}, {{50,75,40},"F1.4"},
        {{60,90,35},"F2.1"}, {{60,90,40},"F2.2"}, {{60,90,45},"F2.3"}, {{60,90,50},"F2.4"},
        {{70,105,40},"F3.1"}, {{70,105,45},"F3.2"}, {{70,105,50},"F3.3"}, {{70,105,55},"F3.4"},
    };
    const auto found = labels.find({n,m,c});
    if (found != labels.end()) return found->second;
    return "N" + std::to_string(n) + "_M" + std::to_string(m) + "_C" + std::to_string(c);
}

double average(const std::vector<const ExperimentResult*>& rows, double ExperimentResult::*field) {
    double total = 0;
    for (const auto* row : rows) total += row->*field;
    return total / rows.size();
}

std::string number(double value, int precision = 3) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string metric(const Group& group, const std::string& solver,
                   double ExperimentResult::*field, int precision = 3) {
    const auto found = group.solvers.find(solver);
    if (found == group.solvers.end()) return "";
    return number(average(found->second, field), precision);
}
} // namespace

void write_aggregated_results(const std::vector<ExperimentResult>& results,
                              const std::string& directory, bool ssp) {
    // Numeric keys keep random size ranges ordered independently of filenames.
    using Key = std::tuple<int, int, int>;
    std::map<bool, std::map<Key, Group>> families;
    std::map<std::string, double> exact_costs;
    for (const auto& result : results) {
        if (!ssp) {
            const auto inserted = exact_costs.emplace(result.test, result.cost);
            if (!inserted.second && std::abs(inserted.first->second - result.cost) > 1e-6)
                throw std::runtime_error("Exact solvers disagree on " + result.test);
        }
        const bool random = result.test.rfind("rand_", 0) == 0;
        const int width = ssp ? 10 : 50;
        const int lo = ((result.n - 1) / width) * width + 1;
        Key key = random ? Key{lo, 0, 0} : Key{result.n, result.m, result.capacity};
        auto& group = families[random][key];
        group.label = random ? std::to_string(lo) + "-" + std::to_string(lo + width - 1)
                             : dataset_label(result.n, result.m, result.capacity);
        group.n = result.n;
        group.m = result.m;
        group.capacity = result.capacity;
        group.solvers[result.solver].push_back(&result);
    }
    std::filesystem::create_directories(directory);
    for (const auto& family : families) {
        const bool random = family.first;
        const auto path = std::filesystem::path(directory) /
            ((random ? std::string("Random") : std::string("CatanzaroAndMecler")) +
             (ssp ? "SSP.csv" : "WTRP.csv"));
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Cannot open output: " + path.string());
        out << (random ? "n," : "Dataset,n,m,C,")
            << (ssp ? "ILP Avg,ILP Best,Mecler+LSG Avg,Mecler+LSG Best,Mecler+LSG SD\n"
                    : "Privault-Finke,Crama's LP,LSG\n");
        for (const auto& entry : family.second) {
            const auto& group = entry.second;
            out << group.label << ',';
            if (!random) out << group.n << ',' << group.m << ',' << group.capacity << ',';
            if (!ssp) {
                out << metric(group, "FLOW_PF", &ExperimentResult::millis) << ','
                    << metric(group, "LP_HiGHS", &ExperimentResult::millis) << ','
                    << metric(group, "FLOW_LSG", &ExperimentResult::millis) << '\n';
                continue;
            }
            out << metric(group, "MCF_ILP", &ExperimentResult::cost, 2) << ','
                << metric(group, "MCF_ILP", &ExperimentResult::best_cost, 2) << ','
                << metric(group, "Mecler", &ExperimentResult::cost, 2) << ','
                << metric(group, "Mecler", &ExperimentResult::best_cost, 2) << ',';
            const auto mecler = group.solvers.find("Mecler");
            bool repeated = mecler != group.solvers.end();
            if (repeated) {
                for (const auto* row : mecler->second) {
                    repeated &= row->runs > 1 && std::isfinite(row->cost_std);
                }
            }
            // Mean of within-instance sample SDs, not SD across different instances.
            if (repeated) out << metric(group, "Mecler", &ExperimentResult::cost_std, 2);
            out << '\n';
        }
        out.flush();
        if (!out) throw std::runtime_error("Cannot write output: " + path.string());
    }
}
