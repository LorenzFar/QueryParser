#pragma once
#include <arrow/api.h>
#include <parquet/arrow/reader.h>
#include <arrow/io/api.h>
#include <memory>
#include <string>

class Table {
public:
    explicit Table(const std::string& filename);

    void displayTable() const;

    std::shared_ptr<arrow::Table> getTable() const;

private:
    std::shared_ptr<arrow::Table> table_;
};
