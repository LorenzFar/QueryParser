#pragma once
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/column_reader.h>
#include <iomanip>
#include <iostream>

class Table {
    
public:
    explicit Table(const std::string& filename);

    std::shared_ptr<arrow::Table> readRowGroup(int row_group, const std::vector<int>& columns);
    std::vector<int64_t> readRawInt64Column(int row_group, int column_index, int64_t num_rows);

    void printSchema() const;

    int64_t rowGroupSize(int row_group) const;
    int64_t numRows() const;
    int numRowGroups() const;
    int numColumns() const;

private:
    std::unique_ptr<parquet::ParquetFileReader> parquet_reader_;
    std::unique_ptr<parquet::arrow::FileReader> arrow_reader_;
};