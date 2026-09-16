# Reducing WTRP to Min-Cost Flow on a Linear-Size Graph

This repository contains the implementation and experiment pipeline for the
paper **"Reducing the Weighted Tool Replacement Problem to Min-Cost Flow on a
Linear-Size Graph"**.

## Abstract

The Weighted Tool Replacement Problem (WTRP) asks how to manage a limited tool
magazine for a fixed sequence of jobs when each tool has its own insertion
cost. The main algorithm in this repository reduces fixed-sequence WTRP to a
minimum-cost flow problem on a directed acyclic graph with linear-size structure
in the number of job requirements. The repository also includes baselines based
on the Privault-Finke construction and Crama's LP formulation, plus a weighted
job sequencing and tool switching (SSP) experiment where the LSG evaluator is
embedded into a Mecler-style hybrid genetic search.

## Instance Format

All benchmark instances use the same text format. Empty lines and lines whose
first non-space character is `#` are ignored.

```text
M N C
COST c_1 c_2 ... c_M
JOBS N
k_1 t_1,1 t_1,2 ... t_1,k_1
k_2 t_2,1 t_2,2 ... t_2,k_2
...
k_N t_N,1 t_N,2 ... t_N,k_N
```

- `M`: number of tools.
- `N`: number of jobs.
- `C`: magazine capacity.
- `COST`: insertion costs for tools `1..M`.
- `JOBS N`: repeats the number of jobs as a consistency check.
- Each following job line starts with `k_i`, the number of tools required by
  job `i`, followed by `k_i` 1-based tool ids.

For fixed-sequence WTRP experiments, the `N` job lines are interpreted in the
given order. For SSP experiments, the same file describes the job set and tool
requirements, but the job order is part of the optimization.

## Requirements

- A C++17 compiler.
- CMake 3.13 or newer.
- HiGHS installed and discoverable through `pkg-config`.

On macOS with Homebrew:

```sh
brew install cmake pkg-config highs
```

Quick HiGHS check:

```sh
pkg-config --modversion highs
```

## Build

```sh
git clone https://github.com/DR-LLL/Weighted-Tool-Replacement-Problem.git
cd Weighted-Tool-Replacement-Problem
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Reproduce the Experiments

Run the complete experiment pipeline:

```sh
scripts/run_article_experiments.sh
```

The script builds the executable and recomputes all aggregate experiment tables
from the article into `ReproducedResults/`:

```text
ReproducedResults/CatanzaroAndMeclerWTRP.csv
ReproducedResults/RandomWTRP.csv
ReproducedResults/CatanzaroAndMeclerSSP.csv
ReproducedResults/RandomSSP.csv
```

The reference CSV files in `PaperResults/` contain the values reported in the
article. The SSP reference tables include the standard-deviation column requested
for repeated Mecler+LSG runs.

## Experimental Environment

The article reports that the computational experiments were executed on an
Apple MacBook Air (M1, 2020) with an 8-core ARM64 CPU, 16 GB RAM, and a 512 GB
SSD, running macOS Ventura 13.2.1. The paper specifies single-threaded runs
and compilation with `-O3`. HiGHS uses one thread, while other solver settings retain
their defaults.
