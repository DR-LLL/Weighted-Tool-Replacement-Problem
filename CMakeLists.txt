cmake_minimum_required(VERSION 3.13)

# set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)

project(WeightTRP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# --- HiGHS by pkg-config ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIGHS REQUIRED highs)

set(SOURCES
        src/main.cpp
        src/ResultsWriter.cpp
        src/GeneticSolver.cpp
        src/DAG.cpp
        src/Instance.cpp
        src/BuilderPF.cpp
        src/BuilderLSG.cpp
        src/HighsLPSolver.cpp
        src/DAG_Solver.cpp
        src/HighsMCFSolver.cpp
        src/Mecler/CommandLine.cpp
        src/Mecler/Genetic.cpp
        src/Mecler/Individual.cpp
        src/Mecler/LocalSearch.cpp
        src/Mecler/Parameters.cpp
        src/Mecler/Population.cpp
        )

add_executable(WeightTRP ${SOURCES})

target_include_directories(WeightTRP PRIVATE ${HIGHS_INCLUDE_DIRS})
target_link_directories(WeightTRP PRIVATE ${HIGHS_LIBRARY_DIRS})
target_link_libraries(WeightTRP PRIVATE ${HIGHS_LIBRARIES})

include(CTest)
if(BUILD_TESTING)
    add_executable(results_writer_test results_writer_test.cpp src/ResultsWriter.cpp)
    target_include_directories(results_writer_test PRIVATE src)
    add_test(NAME results_writer COMMAND results_writer_test "${CMAKE_CURRENT_BINARY_DIR}/test-results")
    add_test(NAME cli_help COMMAND WeightTRP --help)
    add_test(NAME cli_invalid_runs COMMAND WeightTRP --runs 0)
    set_tests_properties(cli_invalid_runs PROPERTIES WILL_FAIL TRUE)
    add_test(NAME cli_no_ssp_solver COMMAND WeightTRP --ssp --no-ilp --no-mecler)
    add_test(NAME cli_invalid_time COMMAND WeightTRP --ssp --time-limit nan)
    set_tests_properties(cli_no_ssp_solver cli_invalid_time PROPERTIES WILL_FAIL TRUE)
    add_test(NAME exact_smoke COMMAND WeightTRP --wtrp --lp
        --tests "${CMAKE_CURRENT_SOURCE_DIR}/PaperTests/CatanzaroAndMecler"
        --results "${CMAKE_CURRENT_BINARY_DIR}/test-smoke" --limit 10 --runs 1)
    add_test(NAME ssp_default_repeats COMMAND WeightTRP --ssp --no-ilp
        --tests "${CMAKE_CURRENT_SOURCE_DIR}/PaperTests/CatanzaroAndMecler"
        --results "${CMAKE_CURRENT_BINARY_DIR}/test-ssp-default" --limit 1 --time-limit 1)
    set_tests_properties(ssp_default_repeats PROPERTIES PASS_REGULAR_EXPRESSION "\\[Mecler\\].*runs=3, std=[0-9]" TIMEOUT 60)
    add_test(NAME cli_writes_log_to_results COMMAND /bin/sh -c
        "$<TARGET_FILE:WeightTRP> --wtrp --no-pf --tests '${CMAKE_CURRENT_SOURCE_DIR}/PaperTests/CatanzaroAndMecler' --results '${CMAKE_CURRENT_BINARY_DIR}/test-log-location' --limit 1 --runs 1 && test -f '${CMAKE_CURRENT_BINARY_DIR}/test-log-location/log.txt'")
    add_test(NAME wtrp_does_not_report_std COMMAND /bin/sh -c
        "$<TARGET_FILE:WeightTRP> --wtrp --no-pf --tests '${CMAKE_CURRENT_SOURCE_DIR}/PaperTests/CatanzaroAndMecler' --results '${CMAKE_CURRENT_BINARY_DIR}/test-no-wtrp-std' --limit 1 --runs 2 >/dev/null && ! grep -q 'std=' '${CMAKE_CURRENT_BINARY_DIR}/test-no-wtrp-std/log.txt'")
    add_test(NAME ssp_parallel_processes COMMAND /bin/sh -c
        "$<TARGET_FILE:WeightTRP> --ssp --no-ilp --tests '${CMAKE_CURRENT_SOURCE_DIR}/PaperTests/CatanzaroAndMecler' --results '${CMAKE_CURRENT_BINARY_DIR}/test-ssp-parallel' --limit 1 --runs 3 --processes 2 --time-limit 1 >/dev/null && grep -q 'Mecler+LSG SD' '${CMAKE_CURRENT_BINARY_DIR}/test-ssp-parallel/CatanzaroAndMeclerSSP.csv'")
    add_test(NAME cli_append_results COMMAND /bin/sh -c
        "$<TARGET_FILE:WeightTRP> --ssp --no-ilp --tests '${CMAKE_CURRENT_SOURCE_DIR}/PaperTests/CatanzaroAndMecler' --results '${CMAKE_CURRENT_BINARY_DIR}/test-append-results' --limit 1 --runs 1 --time-limit 1 >/dev/null && $<TARGET_FILE:WeightTRP> --ssp --no-ilp --tests '${CMAKE_CURRENT_SOURCE_DIR}/PaperTests/RandomTests' --results '${CMAKE_CURRENT_BINARY_DIR}/test-append-results' --min-n 11 --max-n 20 --limit 1 --runs 1 --time-limit 1 --append-results >/dev/null && test -f '${CMAKE_CURRENT_BINARY_DIR}/test-append-results/CatanzaroAndMeclerSSP.csv' && test -f '${CMAKE_CURRENT_BINARY_DIR}/test-append-results/RandomSSP.csv'")
    add_test(NAME article_order_limit COMMAND /bin/sh -c
        "$<TARGET_FILE:WeightTRP> --wtrp --no-pf --tests '${CMAKE_CURRENT_SOURCE_DIR}/PaperTests/CatanzaroAndMecler' --results '${CMAKE_CURRENT_BINARY_DIR}/test-article-order' --limit 1 --runs 1 >/dev/null && grep -q '^A1,' '${CMAKE_CURRENT_BINARY_DIR}/test-article-order/CatanzaroAndMeclerWTRP.csv'")
endif()
