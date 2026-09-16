#include "ResultsWriter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static std::string read(const std::filesystem::path& path) {
    std::ifstream input(path);
    require(bool(input), "Result file missing");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) try {
    require(argc == 2, "Provide a test output directory");
    const std::filesystem::path directory(argv[1]);
    std::vector<ExperimentResult> wtrp;
    for (const auto& solver : {"FLOW_PF", "LP_HiGHS", "FLOW_LSG"}) {
        wtrp.push_back({solver,"from_a.txt",10,10,5,2,40,40,0,1});
        wtrp.push_back({solver,"from_b.txt",10,10,5,2,60,60,0,3});
    }
    write_aggregated_results(wtrp, directory.string(), false);
    require(read(directory / "CatanzaroAndMeclerWTRP.csv") ==
        "Dataset,n,m,C,Privault-Finke,Crama's LP,LSG\nA2,10,10,5,2.000,2.000,2.000\n",
        "WTRP means or solver mapping incorrect");
    wtrp.back().cost = 61;
    bool mismatch = false;
    try { write_aggregated_results(wtrp, directory.string(), false); }
    catch (const std::runtime_error&) { mismatch = true; }
    require(mismatch, "Exact solver mismatch not detected");

    std::vector<ExperimentResult> ssp = {
        {"MCF_ILP","rand_a.txt",18,38,4,1,20,20,0,1},
        {"Mecler","rand_a.txt",18,38,4,10,25,23,2,1},
        {"MCF_ILP","rand_b.txt",19,40,4,1,40,40,0,1},
        {"Mecler","rand_b.txt",19,40,4,10,50,45,4,1},
    };
    write_aggregated_results(ssp, directory.string(), true);
    require(read(directory / "RandomSSP.csv") ==
        "n,ILP Avg,ILP Best,Mecler+LSG Avg,Mecler+LSG Best,Mecler+LSG SD\n11-20,30.00,30.00,37.50,34.00,3.00\n",
        "SSP means, best values, within-instance SD, or headers incorrect");
    ssp.back().runs = 1;
    write_aggregated_results(ssp, directory.string(), true);
    require(read(directory / "RandomSSP.csv").find("37.50,34.00,\n") != std::string::npos,
        "Missing repeated runs must leave SD blank");
    for (auto& row : ssp) row.cost = 0;
    write_aggregated_results(ssp, directory.string(), true);
    require(read(directory / "RandomSSP.csv").find("11-20,0.00,30.00,0.00,34.00,\n") != std::string::npos,
        "Zero averages must still preserve best-value columns");
    std::cout << "Result aggregation checks passed\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
