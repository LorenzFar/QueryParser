#pragma once
#include <arm_neon.h>
#include "../storage/Table.h"
#include "../ankerl/unordered_dense.h"

struct BrandSpec {
    int32_t  dict_idx;   
    int32_t  max_size;
    uint8_t  brand_bits;
};

class Predicate {
public:
    //static int64_t simd_filter(const std::shared_ptr<arrow::Table>& row_group, std::vector<int64_t>& selection_out);
    static void lineitem_filter_indices(
        const std::shared_ptr<arrow::Table>& rg,
        const int64_t* raw_partkey,
        const int64_t* raw_qty,
        int64_t count,
        const ankerl::unordered_dense::map<int64_t, uint8_t>& hash_table,
        std::vector<int64_t>& matching_indices
    );

    //static int64_t lineitem_filter(const std::shared_ptr<arrow::Table>& rg, const int64_t* raw_partkey, const int64_t* raw_qty,const int64_t* raw_price, const int64_t* raw_discount, int64_t count, const std::unordered_map<int64_t, uint8_t>& hash_table);
    
    //static void part_filter(const std::shared_ptr<arrow::Table>& row_group, std::unordered_map<int64_t, uint8_t>& hash_table);
    static void part_filter(const std::shared_ptr<arrow::Table>& row_group, ankerl::unordered_dense::map<int64_t, uint8_t>& hash_table);
    static bool shouldScanRowGroup(const Table& table, size_t rg);
    static bool checkBloomFilter(const std::string& probe, const std::shared_ptr<parquet::BloomFilter>& bloom_filter);

    static inline int8x16_t narrow_i32x16(const int32_t* p);
    static int32_t find_dict_index(const arrow::StringArray* dict, const std::string_view target);
    static inline uint8x16_t match_any4(int8x16_t v, int8x16_t a, int8x16_t b, int8x16_t c, int8x16_t d);
};