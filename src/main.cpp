#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <memory>
#include "GeneticSolver.cpp"
#include "HighsLPSolver.h"
#include "ISolver.h"
#include "Instance.h"
#include "BuilderPF.h"
#include "BuilderLSG.h"
#include "DAG_Solver.h"
#include "HighsMCFSolver.h"


namespace fs = std::filesystem;


static void log_message(const std::string& message) {
    std::cout << message << std::endl;
    std::ofstream log_file("log.txt", std::ios_base::app);
    if (log_file.is_open()) {
        log_file << message << std::endl;
    }
}


static std::vector<std::string> get_test_files(const std::string& directory) {
    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            files.push_back(entry.path().string());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}



static void run_pipeline_solver(const std::vector<std::string>& test_files,
                                ISolver& solver,
                                const std::string& csv_path,
                                int num_runs) {
    std::ofstream csv(csv_path, std::ios::trunc);

    std::string text =  "algorithm,test,N,M,C,total_cost,millis\n";
    csv << text;
    for (const auto& test_file : test_files) {
        try {
            TestInstance instance = TestInstance::load_from_file(test_file);

            double total_time = 0.0;
            long long total_cost = 0;

            for (int i = 0; i < num_runs; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                total_cost = solver.ComputeSolution(instance);
                auto t1 = std::chrono::high_resolution_clock::now();

                total_time += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
            }

            double avg_time = total_time / num_runs;


            csv << solver.name << "," << fs::path(test_file).filename().string() << ","
                << instance.N << "," << instance.M << "," << instance.C <<","<< total_cost << ","
                << std::fixed << std::setprecision(3) << avg_time << "\n";


            log_message("[" + solver.name + "] "
                        + fs::path(test_file).filename().string()
                        + " -> cost=" + std::to_string(total_cost)
                        + ", avg time=" + std::to_string(avg_time) + " ms");

        } catch (const std::exception& e) {
            log_message("[" + solver.name + "] ERROR " + test_file + ": " + e.what());
        }
    }

    log_message("Saved: " + csv_path);
}

int main() {
    std::ofstream log_file("log.txt", std::ios::trunc);
    log_file.close();
    log_message("=== Tool Switching Problem ===");

    const std::string tests_dir   = "tests_txt";
    const std::string results_dir = "results";
    std::filesystem::create_directory(results_dir);

    auto tests = get_test_files(tests_dir);

    PFBuilder pf;
    LSGBuilder lsg;


    int num_runs = 20;
    double timeLimit = 300; // seconds

    //solvers for SSP
    GeneticSolver Mecler(timeLimit);
    HighsMCFSolver MCF(timeLimit);

    //solvers for WTRP
    DAG_Solver LSG = DAG_Solver(lsg);
    DAG_Solver PF = DAG_Solver(pf);
    HighsLPSolver LP;



    log_message("");
    run_pipeline_solver(tests, LSG, results_dir + "/resultsLSG.csv", num_runs);
    log_message("");
    run_pipeline_solver(tests, PF, results_dir + "/resultsPrivaultFinke.csv", num_runs);


    return 0;
}
