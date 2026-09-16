#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <cctype>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <system_error>
#include <tuple>
#include <sys/wait.h>
#include <unistd.h>

#include "GeneticSolver.h"
#include "HighsLPSolver.h"
#include "ISolver.h"
#include "Instance.h"
#include "BuilderPF.h"
#include "BuilderLSG.h"
#include "DAG_Solver.h"
#include "HighsMCFSolver.h"
#include "ResultsWriter.h"
#include "Mecler/LocalSearch.h"


namespace fs = std::filesystem;

static fs::path g_log_path = "log.txt";

static void log_message(const std::string& message) {
    std::cout << message << std::endl;
    std::ofstream log_file(g_log_path, std::ios_base::app);
    if (log_file.is_open()) {
        log_file << message << std::endl;
    }
}


struct RunConfig {
    std::string tests_dir = "PaperTests/CatanzaroAndMecler";
    std::string results_dir = "ReproducedResults";
    int num_runs = 20;
    int ilp_runs = 1;
    int limit = 0;
    int min_n = 1;
    int max_n = 0;
    int processes = 1;
    bool append_results = false;
    bool include_lp = false;
    bool include_pf = true;
    bool run_wtrp = true;
    bool run_ssp = false;
    bool include_ilp = true;
    bool include_mecler = true;
    double time_limit = 300.0;
};

static void print_usage(const char* program) {
    std::cout
        << "Usage: " << program << " [--wtrp|--ssp|--all] [--tests DIR] [--results DIR]\n"
        << "       [--runs N] [--limit N] [--time-limit SEC] [--lp] [--no-pf]\n"
        << "       [--no-ilp] [--no-mecler] [--ilp-runs N] [--processes N]\n"
        << "       [--min-n N] [--max-n N] [--append-results]\n"
        << "Defaults: --wtrp --tests PaperTests/CatanzaroAndMecler --results ReproducedResults --time-limit 300\n"
        << "Default repeats: WTRP/all 20, SSP Mecler 3, ILP 1. Override with --runs / --ilp-runs.\n"
        << "Use --processes N to run repeated SSP Mecler evaluations in parallel child processes.\n";
}

static RunConfig parse_args(int argc, char** argv) {
    RunConfig cfg;
    bool runs_set = false;
    if (fs::exists("tests_txt")) {
        cfg.tests_dir = "tests_txt";
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto need_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + name);
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "--tests") {
            cfg.tests_dir = need_value(arg);
        } else if (arg == "--results") {
            cfg.results_dir = need_value(arg);
        } else if (arg == "--runs") {
            cfg.num_runs = std::stoi(need_value(arg));
            runs_set = true;
        } else if (arg == "--limit") {
            cfg.limit = std::stoi(need_value(arg));
        } else if (arg == "--ilp-runs") {
            cfg.ilp_runs = std::stoi(need_value(arg));
        } else if (arg == "--processes") {
            cfg.processes = std::stoi(need_value(arg));
        } else if (arg == "--append-results") {
            cfg.append_results = true;
        } else if (arg == "--min-n") {
            cfg.min_n = std::stoi(need_value(arg));
        } else if (arg == "--max-n") {
            cfg.max_n = std::stoi(need_value(arg));
        } else if (arg == "--lp") {
            cfg.include_lp = true;
        } else if (arg == "--no-pf") {
            cfg.include_pf = false;
        } else if (arg == "--wtrp") {
            cfg.run_wtrp = true;
            cfg.run_ssp = false;
        } else if (arg == "--ssp") {
            cfg.run_wtrp = false;
            cfg.run_ssp = true;
        } else if (arg == "--all") {
            cfg.run_wtrp = true;
            cfg.run_ssp = true;
        } else if (arg == "--no-ilp") {
            cfg.include_ilp = false;
        } else if (arg == "--no-mecler") {
            cfg.include_mecler = false;
        } else if (arg == "--time-limit") {
            cfg.time_limit = std::stod(need_value(arg));
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (cfg.num_runs <= 0 || cfg.ilp_runs <= 0) {
        throw std::runtime_error("--runs and --ilp-runs must be positive");
    }
    if (cfg.processes <= 0) {
        throw std::runtime_error("--processes must be positive");
    }
    if (cfg.limit < 0) {
        throw std::runtime_error("--limit must be non-negative");
    }
    if (cfg.min_n < 1 || cfg.max_n < 0 || (cfg.max_n && cfg.max_n < cfg.min_n)) {
        throw std::runtime_error("Invalid --min-n / --max-n range");
    }
    if (!std::isfinite(cfg.time_limit) || cfg.time_limit <= 0.0) {
        throw std::runtime_error("--time-limit must be positive");
    }
    if (!cfg.run_wtrp && !cfg.run_ssp) {
        throw std::runtime_error("At least one of --wtrp or --ssp must be enabled");
    }
    if (cfg.run_ssp && !cfg.include_ilp && !cfg.include_mecler) {
        throw std::runtime_error("At least one SSP solver must be enabled");
    }
    if (cfg.run_ssp && !cfg.run_wtrp && !runs_set) {
        cfg.num_runs = 3;
    }
    return cfg;
}

static int article_group_rank(const TestInstance& instance) {
    static const std::map<std::tuple<int, int, int>, int> ranks = {
        {{10,10,4},1}, {{10,10,5},2}, {{10,10,6},3}, {{10,10,7},4},
        {{15,20,6},5}, {{15,20,8},6}, {{15,20,10},7}, {{15,20,12},8},
        {{30,40,15},9}, {{30,40,17},10}, {{30,40,20},11}, {{30,40,25},12},
        {{40,60,20},13}, {{40,60,22},14}, {{40,60,25},15}, {{40,60,30},16},
        {{50,75,25},17}, {{50,75,30},18}, {{50,75,35},19}, {{50,75,40},20},
        {{60,90,35},21}, {{60,90,40},22}, {{60,90,45},23}, {{60,90,50},24},
        {{70,105,40},25}, {{70,105,45},26}, {{70,105,50},27}, {{70,105,55},28},
    };
    const auto found = ranks.find({instance.N, instance.M, instance.C});
    if (found != ranks.end()) return found->second;
    return 1000000 + instance.N;
}

static int article_instance_rank(const std::string& path) {
    const std::string filename = fs::path(path).filename().string();
    const std::size_t marker = filename.find("_M");
    const std::size_t end = marker == std::string::npos ? filename.size() : marker;
    std::size_t begin = end;
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(filename[begin - 1]))) {
        --begin;
    }
    if (begin == end) return 1000000;
    int value = std::stoi(filename.substr(begin, end - begin));
    if (value >= 1000) value %= 1000;
    return value == 0 ? 1000000 : value;
}

struct TestFile {
    std::string path;
    TestInstance instance;
};

static std::vector<std::string> get_test_files(const std::string& directory, int limit,
                                              int min_n, int max_n) {
    std::vector<TestFile> selected;
    if (!fs::exists(directory)) {
        throw std::runtime_error("Tests directory does not exist: " + directory);
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            const auto path = entry.path().string();
            const TestInstance instance = TestInstance::load_from_file(path);
            if (min_n > 1 || max_n > 0) {
                const int n = instance.N;
                if (n < min_n || (max_n > 0 && n > max_n)) continue;
            }
            selected.push_back({path, instance});
        }
    }
    std::sort(selected.begin(), selected.end(), [](const TestFile& a, const TestFile& b) {
        const bool a_random = fs::path(a.path).filename().string().rfind("rand_", 0) == 0;
        const bool b_random = fs::path(b.path).filename().string().rfind("rand_", 0) == 0;
        if (a_random != b_random) return a_random < b_random;
        if (a_random) {
            if (a.instance.N != b.instance.N) return a.instance.N < b.instance.N;
            if (a.instance.M != b.instance.M) return a.instance.M < b.instance.M;
            if (a.instance.C != b.instance.C) return a.instance.C < b.instance.C;
        } else {
            const int a_rank = article_group_rank(a.instance);
            const int b_rank = article_group_rank(b.instance);
            if (a_rank != b_rank) return a_rank < b_rank;
            const int a_instance_rank = article_instance_rank(a.path);
            const int b_instance_rank = article_instance_rank(b.path);
            if (a_instance_rank != b_instance_rank) return a_instance_rank < b_instance_rank;
        }
        return fs::path(a.path).filename().string() < fs::path(b.path).filename().string();
    });
    if (selected.empty()) {
        throw std::runtime_error("No .txt instances in " + directory);
    }

    if (limit > 0 && static_cast<int>(selected.size()) > limit) {
        selected.resize(limit);
    }

    std::vector<std::string> files;
    files.reserve(selected.size());
    for (const auto& file : selected) {
        files.push_back(file.path);
    }
    return files;
}



static double sample_std(const std::vector<double>& values, double mean) {
    if (values.size() < 2) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    double sq_sum = 0.0;
    for (double value : values) {
        const double delta = static_cast<double>(value) - mean;
        sq_sum += delta * delta;
    }
    return std::sqrt(sq_sum / (values.size() - 1));
}

struct SingleRunResult {
    double cost;
    double millis;
    unsigned int seed;
};

struct ChildRun {
    pid_t pid;
    int fd;
    int run;
};

static unsigned int child_seed(int run) {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return static_cast<unsigned int>(now)
        ^ (static_cast<unsigned int>(getpid()) << 16)
        ^ static_cast<unsigned int>(run * 2654435761u);
}

static bool write_all(int fd, const void* data, std::size_t size) {
    const auto* ptr = static_cast<const char*>(data);
    while (size > 0) {
        const ssize_t written = write(fd, ptr, size);
        if (written < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        ptr += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

static bool read_all(int fd, void* data, std::size_t size) {
    auto* ptr = static_cast<char*>(data);
    while (size > 0) {
        const ssize_t count = read(fd, ptr, size);
        if (count < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (count == 0) return false;
        ptr += count;
        size -= static_cast<std::size_t>(count);
    }
    return true;
}

static SingleRunResult compute_single_run(ISolver& solver, const TestInstance& instance,
                                          int run_number) {
    const unsigned int seed = child_seed(run_number);
    std::srand(seed);
    reseed_mecler_random(seed);

    auto t0 = std::chrono::high_resolution_clock::now();
    const double total_cost = solver.ComputeSolution(instance);
    auto t1 = std::chrono::high_resolution_clock::now();
    const double run_time =
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
    return {total_cost, run_time, seed};
}

static ChildRun start_child_run(ISolver& solver, const TestInstance& instance, int run_number) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        throw std::system_error(errno, std::generic_category(), "pipe");
    }

    const pid_t pid = fork();
    if (pid < 0) {
        const int saved_errno = errno;
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::system_error(saved_errno, std::generic_category(), "fork");
    }

    if (pid == 0) {
        close(pipefd[0]);
        try {
            const SingleRunResult result = compute_single_run(solver, instance, run_number);
            const bool ok = write_all(pipefd[1], &result, sizeof(result));
            close(pipefd[1]);
            _exit(ok ? 0 : 1);
        } catch (...) {
            close(pipefd[1]);
            _exit(1);
        }
    }

    close(pipefd[1]);
    return {pid, pipefd[0], run_number};
}

static SingleRunResult read_child_result(const ChildRun& child, int status) {
    SingleRunResult result{};
    const bool got_result = read_all(child.fd, &result, sizeof(result));
    close(child.fd);

    if (!got_result || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error("child process failed for run " + std::to_string(child.run));
    }
    return result;
}

static std::size_t find_child(const std::vector<ChildRun>& children, pid_t pid) {
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (children[i].pid == pid) return i;
    }
    throw std::runtime_error("unknown child process finished");
}

static bool run_pipeline_solver(const std::vector<std::string>& test_files,
                                ISolver& solver,
                                std::vector<ExperimentResult>& results,
                                const std::string& results_dir,
                                bool ssp,
                                int num_runs,
                                int processes,
                                bool record_cost_std = false) {
    bool success = true;
    for (const auto& test_file : test_files) {
        try {
            TestInstance instance = TestInstance::load_from_file(test_file);

            double total_time = 0.0;
            std::vector<double> costs;
            costs.reserve(num_runs);
            const std::string test_name = fs::path(test_file).filename().string();
            std::size_t summary_index = results.size();

            auto record_run = [&](int run, const SingleRunResult& run_result) {
                costs.push_back(run_result.cost);
                total_time += run_result.millis;

                const double avg_time = total_time / costs.size();
                const double avg_cost = std::accumulate(costs.begin(), costs.end(), 0.0) / costs.size();
                const double best_cost = *std::min_element(costs.begin(), costs.end());
                const double cost_std = record_cost_std
                    ? sample_std(costs, avg_cost)
                    : std::numeric_limits<double>::quiet_NaN();
                ExperimentResult current{solver.name, test_name, instance.N, instance.M, instance.C,
                                         static_cast<int>(costs.size()), avg_cost, best_cost,
                                         cost_std, avg_time};
                if (costs.size() == 1) {
                    results.push_back(current);
                } else {
                    results[summary_index] = current;
                }
                write_aggregated_results(results, results_dir, ssp);
            };

            if (processes == 1) {
                for (int run = 1; run <= num_runs; ++run) {
                    log_message("[" + solver.name + "] "
                                + test_name
                                + " run " + std::to_string(run) + "/" + std::to_string(num_runs)
                                + " started in the parent process");
                    const auto run_result = compute_single_run(solver, instance, run);
                    record_run(run, run_result);
                    log_message("[" + solver.name + "] "
                                + test_name
                                + " run " + std::to_string(run) + "/" + std::to_string(num_runs)
                                + " finished: cost=" + std::to_string(run_result.cost)
                                + ", time=" + std::to_string(run_result.millis) + " ms");
                }
            } else {
                int next_run = 1;
                std::vector<ChildRun> children;
                children.reserve(std::min(processes, num_runs));

                auto start_next = [&]() {
                    if (next_run <= num_runs) {
                        const int run = next_run++;
                        children.push_back(start_child_run(solver, instance, run));
                        const auto& child = children.back();
                        log_message("[" + solver.name + "] "
                                    + test_name
                                    + " run " + std::to_string(run) + "/" + std::to_string(num_runs)
                                    + " started in child pid=" + std::to_string(child.pid)
                                    + "; active processes=" + std::to_string(children.size())
                                    + "/" + std::to_string(processes));
                    }
                };

                while (next_run <= num_runs && static_cast<int>(children.size()) < processes) {
                    start_next();
                }

                while (!children.empty()) {
                    int status = 0;
                    pid_t finished = 0;
                    do {
                        finished = waitpid(-1, &status, 0);
                    } while (finished < 0 && errno == EINTR);
                    if (finished < 0) {
                        throw std::system_error(errno, std::generic_category(), "waitpid");
                    }

                    const std::size_t child_index = find_child(children, finished);
                    const ChildRun child = children[child_index];
                    children.erase(children.begin() + static_cast<long>(child_index));
                    const auto run_result = read_child_result(child, status);
                    record_run(child.run, run_result);
                    log_message("[" + solver.name + "] "
                                + test_name
                                + " run " + std::to_string(child.run) + "/" + std::to_string(num_runs)
                                + " finished from child pid=" + std::to_string(child.pid)
                                + ": cost=" + std::to_string(run_result.cost)
                                + ", time=" + std::to_string(run_result.millis)
                                + " ms; active processes="
                                + std::to_string(children.size()) + "/" + std::to_string(processes));
                    start_next();
                }
            }

            const double avg_time = total_time / costs.size();
            const double avg_cost = std::accumulate(costs.begin(), costs.end(), 0.0) / costs.size();
            const double cost_std = record_cost_std
                ? sample_std(costs, avg_cost)
                : std::numeric_limits<double>::quiet_NaN();


            log_message("[" + solver.name + "] "
                        + test_name
                        + " -> avg cost=" + std::to_string(avg_cost)
                        + ", runs=" + std::to_string(num_runs)
                        + (record_cost_std
                            ? ", std=" + (num_runs > 1 ? std::to_string(cost_std) : "N/A")
                            : "")
                        + ", avg time=" + std::to_string(avg_time) + " ms");

        } catch (const std::exception& e) {
            success = false;
            log_message("[" + solver.name + "] ERROR " + test_file + ": " + e.what());
        }
    }

    return success;
}

int main(int argc, char** argv) try {
    RunConfig cfg;
    try {
        cfg = parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << "\n";
        print_usage(argv[0]);
        return 2;
    }

    std::filesystem::create_directories(cfg.results_dir);
    g_log_path = fs::path(cfg.results_dir) / "log.txt";
    std::ofstream log_file(g_log_path, cfg.append_results ? std::ios::app : std::ios::trunc);
    log_file.close();
    log_message("=== Tool Switching Problem ===");

    auto tests = get_test_files(cfg.tests_dir, cfg.limit, cfg.min_n, cfg.max_n);
    log_message("Tests directory: " + cfg.tests_dir);
    log_message("Results directory: " + cfg.results_dir);
    log_message("Tests selected: " + std::to_string(tests.size()));

    PFBuilder pf;
    LSGBuilder lsg;

    //solvers for SSP
    GeneticSolver Mecler(cfg.time_limit);
    HighsMCFSolver MCF(cfg.time_limit);

    //solvers for WTRP
    DAG_Solver LSG = DAG_Solver(lsg);
    DAG_Solver PF = DAG_Solver(pf);
    HighsLPSolver LP;
    bool success = true;
    std::vector<ExperimentResult> wtrp_results, ssp_results;



    if (cfg.run_wtrp) {
        log_message("");
        success &= run_pipeline_solver(tests, LSG, wtrp_results, cfg.results_dir, false, cfg.num_runs, 1);

        if (cfg.include_pf) {
            log_message("");
            success &= run_pipeline_solver(tests, PF, wtrp_results, cfg.results_dir, false, cfg.num_runs, 1);
        }

        if (cfg.include_lp) {
            log_message("");
            success &= run_pipeline_solver(tests, LP, wtrp_results, cfg.results_dir, false, cfg.num_runs, 1);
        }
    }

    if (cfg.run_ssp) {
        if (cfg.include_ilp) {
            log_message("");
            success &= run_pipeline_solver(tests, MCF, ssp_results, cfg.results_dir, true, cfg.ilp_runs, 1);
        }
        if (cfg.include_mecler) {
            log_message("");
            success &= run_pipeline_solver(tests, Mecler, ssp_results, cfg.results_dir, true,
                                           cfg.num_runs, cfg.processes, true);
        }
    }


    if (!success) return 1;
    if (cfg.run_wtrp) write_aggregated_results(wtrp_results, cfg.results_dir, false);
    if (cfg.run_ssp) write_aggregated_results(ssp_results, cfg.results_dir, true);
    log_message("Saved aggregated results to " + cfg.results_dir);
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Experiment error: " << e.what() << '\n';
    return 1;
}
