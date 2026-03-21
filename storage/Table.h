#pragma once
#include <arrow/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/bloom_filter_reader.h>
#include <parquet/bloom_filter.h>
#include <parquet/statistics.h>
#include <arrow/io/api.h>
#include <memory>
#include <iomanip>
#include <string>
#include <vector>

struct ColumnStats {
    int row_group;
    int column;
    std::string col_name;
    std::variant<int32_t, int64_t, float, double, std::string> min_val;
    std::variant<int32_t, int64_t, float, double, std::string> max_val;
    std::shared_ptr<parquet::BloomFilter> bloom_filter;
    bool has_stats = false;
    bool has_bloom = false;
};

class Table {
    
public:
    explicit Table(const std::string& filename);

    ColumnStats getColumnStats(int row_group, int column) const;
    std::vector<ColumnStats> getAllStats() const;          
    std::vector<ColumnStats> getRowGroupStats(int row_group) const; 

    std::shared_ptr<arrow::Table> readRowGroup(int row_group, const std::vector<int>& columns);

    std::vector<int64_t> readRawInt64Column(int row_group, int column_index, int64_t num_rows);

    void printSchema() const;
    void fillMinMax(ColumnStats& cs, const std::shared_ptr<parquet::Statistics>& stats) const;
    int64_t rowGroupSize(int row_group) const;

    int numRowGroups() const;
    int numColumns() const;

private:
    std::unique_ptr<parquet::ParquetFileReader> parquet_reader_;
    std::unique_ptr<parquet::arrow::FileReader> arrow_reader_;
};