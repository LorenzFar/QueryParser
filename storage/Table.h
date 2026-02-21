#pragma once
#include <arrow/api.h>
#include <parquet/arrow/reader.h>
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
    bool has_stats = false;
};

class Table {
public:
    explicit Table(const std::string& filename);

    ColumnStats getColumnStats(int row_group, int column) const;
    std::vector<ColumnStats> getAllStats() const;          
    std::vector<ColumnStats> getRowGroupStats(int row_group) const; 

    std::shared_ptr<arrow::Table> readRowGroup(int row_group, const std::vector<int>& cols);

    void printSchema() const;
    void fillMinMax(ColumnStats& cs, const std::shared_ptr<parquet::Statistics>& stats) const;

    int numRowGroups() const;
    int numColumns() const;

private:
    std::shared_ptr<parquet::ParquetFileReader> parquet_reader_;
    std::unique_ptr<parquet::arrow::FileReader> arrow_reader_;

    template<typename T, typename StatsT>
    void extractTypedStats(const std::shared_ptr<parquet::Statistics>& raw, ColumnStats& out) const;
};