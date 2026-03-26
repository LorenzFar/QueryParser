#!/bin/bash
set -e

echo "=== Building ==="
clang++ -std=c++17 -O3 -march=native -flto \
    main.cpp storage/Table.cpp filter/Predicate.cpp \
    -I/opt/homebrew/include \
    -L/opt/homebrew/lib -larrow -lparquet \
    -o main

echo "=== Running Optimised Benchmark ==="
./main

echo "=== Running DuckDB Benchmark ==="
python3 run_benchmark.py

echo "=== Done ==="