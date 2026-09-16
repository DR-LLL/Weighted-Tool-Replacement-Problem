#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
RESULTS_ROOT="${RESULTS_ROOT:-ReproducedResults}"
RUNS="${RUNS:-20}"
SSP_RUNS="${SSP_RUNS:-10}"
TIME_LIMIT="${TIME_LIMIT:-300}"
PROCESSES="${PROCESSES:-1}"

APPEND_ARG=""
run_experiment() {
  if [[ -n "$APPEND_ARG" ]]; then
    "$BUILD_DIR/WeightTRP" "$@" "$APPEND_ARG"
  else
    "$BUILD_DIR/WeightTRP" "$@"
  fi
  APPEND_ARG="--append-results"
}

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j

run_experiment --wtrp --lp \
  --tests PaperTests/CatanzaroAndMecler --results "$RESULTS_ROOT" \
  --runs "$RUNS"

run_experiment --wtrp --lp \
  --tests PaperTests/RandomTests --results "$RESULTS_ROOT" \
  --min-n 1 --max-n 500 --runs "$RUNS"

run_experiment --ssp \
  --tests PaperTests/CatanzaroAndMecler --results "$RESULTS_ROOT" \
  --runs "$SSP_RUNS" --ilp-runs 1 --time-limit "$TIME_LIMIT" \
  --processes "$PROCESSES"

run_experiment --ssp \
  --tests PaperTests/RandomTests --results "$RESULTS_ROOT" \
  --min-n 11 --max-n 100 --runs "$SSP_RUNS" --ilp-runs 1 \
  --time-limit "$TIME_LIMIT" --processes "$PROCESSES"

echo "Experiments completed: $RESULTS_ROOT"
