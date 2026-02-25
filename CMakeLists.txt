cmake_minimum_required(VERSION 3.10)

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
