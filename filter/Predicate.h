#pragma once
#include "../storage/Table.h"

struct FilterResult {
    double revenue;
    int64_t count;
};

class Predicate {
public:
    //static int64_t simd_filter(const std::shared_ptr<arrow::Table>& row_group, std::vector<int64_t>& selection_out);
    static void lineitem_filter_indices(
        const std::shared_ptr<arrow::Table>& rg,
        const int64_t* raw_partkey,
        const int64_t* raw_qty,
        int64_t count,
        const std::unordered_map<int64_t, uint8_t>& hash_table,
        std::vector<int64_t>& matching_indices
    );

   //static int64_t lineitem_filter(const std::shared_ptr<arrow::Table>& rg, const int64_t* raw_partkey, const int64_t* raw_qty,const int64_t* raw_price, const int64_t* raw_discount, int64_t count, const std::unordered_map<int64_t, uint8_t>& hash_table);
    static void part_filter(const std::shared_ptr<arrow::Table>& row_group, std::unordered_map<int64_t, uint8_t>& hash_table);
    static bool shouldScanRowGroup(const Table& table, size_t rg);
    static bool checkBloomFilter(const std::string& probe, const std::shared_ptr<parquet::BloomFilter>& bloom_filter);
};