#!/bin/bash
set -e

SF_PATH=""
OUT_FILE="results.csv"

# Parse args
while [[ $# -gt 0 ]]; do
    case $1 in
        --data) SF_PATH="$2"; shift 2;;
        --out)  OUT_FILE="$2"; shift 2;;
        *) echo "Unknown argument: $1"; exit 1;;
    esac
done

if [[ -z "$SF_PATH" ]]; then
    echo "--data <sf_path> is required"
    exit 1
fi

# Compile C++ benchmark
clang++ -std=c++17 -O3 -march=native -flto \
    main.cpp storage/Table.cpp filter/Predicate.cpp \
    -I/opt/homebrew/include \
    -L/opt/homebrew/lib -larrow -lparquet \
    -o main

# Run benchmarks
cpp_time=$(./main --data "$SF_PATH" --out "$OUT_FILE")
duck_time=$(python3 run_benchmark.py "$SF_PATH" "$OUT_FILE")

sf_label=$(basename "$SF_PATH")

# Print summary table
echo "SF | C++ Time (s) | DuckDB Time (s)"
echo "---|--------------|----------------"
printf "%-4s | %-12s | %s\n" "$sf_label" "$cpp_time" "$duck_time"

echo "All benchmarks completed. CSV: $OUT_FILE"