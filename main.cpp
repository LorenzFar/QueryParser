#include "./storage/Table.h"
#include "./filter/Predicate.h"
#include "./ankerl/unordered_dense.h"
#include <chrono>
#include <fstream>

using Clock = std::chrono::high_resolution_clock;
using us    = std::chrono::microseconds;

struct RunResult {
    int64_t table_open;
    int64_t part_read, part_filter;
    int64_t lineitem_read, lineitem_filter;
    int64_t total;
    double  revenue;
};

RunResult run_once(const std::string& sf_path) {
    RunResult r{};

    auto start = Clock::now();

    auto t0 = Clock::now();
    Table part_table    (sf_path + "/part.parquet");
    Table lineitem_table(sf_path + "/lineitem.parquet");
    auto t1 = Clock::now();

    ankerl::unordered_dense::map<int64_t, uint8_t> hash_table;
    
    double sf = static_cast<double>(part_table.numRows()) / 200000.0;
    hash_table.reserve(static_cast<size_t>(sf * 512 * 1.5));

    for (int i = 0; i < part_table.numRowGroups(); ++i) {
        auto ta = Clock::now();
        auto rg = part_table.readRowGroup(i, {0, 3, 5, 6});
        auto tb = Clock::now();
        Predicate::part_filter(rg, hash_table);
        auto tc = Clock::now();
        r.part_read   += std::chrono::duration_cast<us>(tb - ta).count();
        r.part_filter += std::chrono::duration_cast<us>(tc - tb).count();
    }
    auto t2 = Clock::now();

    int64_t total_revenue = 0;
    std::vector<int64_t> matching_indices;

    for (int i = 0; i < lineitem_table.numRowGroups(); ++i) {
        int64_t num_rows = lineitem_table.rowGroupSize(i);

        auto ta      = Clock::now();
        auto rg      = lineitem_table.readRowGroup(i, {13, 14});
        auto partkey = lineitem_table.readRawInt64Column(i, 1, num_rows);
        auto qty     = lineitem_table.readRawInt64Column(i, 4, num_rows);
        auto tb      = Clock::now();

        Predicate::lineitem_filter_indices(rg, partkey.data(), qty.data(),
                                           num_rows, hash_table, matching_indices);
        auto tc = Clock::now();

        r.lineitem_read   += std::chrono::duration_cast<us>(tb - ta).count();
        r.lineitem_filter += std::chrono::duration_cast<us>(tc - tb).count();

        if (matching_indices.empty()) continue;

        auto price    = lineitem_table.readRawInt64Column(i, 5, num_rows);
        auto discount = lineitem_table.readRawInt64Column(i, 6, num_rows);

        for (int64_t idx : matching_indices)
            total_revenue += price[idx] * (100 - discount[idx]);
    }
    auto t3  = Clock::now();
    auto end = Clock::now();

    r.table_open      = std::chrono::duration_cast<us>(t1 - t0).count();
    r.total           = std::chrono::duration_cast<us>(end - start).count();
    r.revenue         = (double)total_revenue / 10000.0;
    r.lineitem_read   = std::chrono::duration_cast<us>(t3 - t2).count();

    return r;
}

int main(int argc, char** argv) {
    constexpr int WARMUP_RUNS = 3;
    constexpr int TIMED_RUNS  = 10;
    constexpr int TOTAL_RUNS  = WARMUP_RUNS + TIMED_RUNS;

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " --data <path> [--out <csv>]\n";
        return 1;
    }

    std::string path;
    std::string out_file = "results.csv";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--data" && i + 1 < argc) {
            path = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            out_file = argv[++i];
        }
    }

    if (path.empty()) {
        std::cerr << "--data is required\n";
        return 1;
    }

    std::ofstream csv(out_file);
    csv << "Result (Optimised),Result (DuckDB)\n";

    int64_t total_time = 0;
    double final_revenue = 0;

    for (int run = 0; run < TOTAL_RUNS; ++run) {
        try {
            auto result = run_once(path);
            if (run >= WARMUP_RUNS) {
                total_time += result.total;
                final_revenue = result.revenue;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error on run " << run << ": " << e.what() << "\n";
            break;
        }
    }

    csv << std::fixed << std::setprecision(4) << final_revenue << ",,\n";

    std::cout << std::fixed << std::setprecision(4)
              << (total_time / 1e6 / TIMED_RUNS) << "\n";
}