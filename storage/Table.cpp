#include "Table.h"
#include <iostream>
#include <iomanip>

Table::Table(const std::string& filename) {
    arrow::MemoryPool* pool = arrow::default_memory_pool();

    auto input_result = arrow::io::ReadableFile::Open(filename);
    if (!input_result.ok()) throw std::runtime_error(input_result.status().ToString());
    auto input = *input_result;

    auto reader_result = parquet::arrow::OpenFile(input, pool);
    if (!reader_result.ok()) throw std::runtime_error(reader_result.status().ToString());
    auto arrow_reader = std::move(*reader_result);

    auto read_status = arrow_reader->ReadTable(&table_);
    if (!read_status.ok()) throw std::runtime_error(read_status.ToString());
}

void Table::displayTable() const {
    std::cout << std::left
                << std::setw(20) << "Column Name"
                << std::setw(30) << "Type"
                << std::setw(12) << "Rows"
                << std::setw(10) << "Chunks"
                << "\n";
    std::cout << std::string(72, '-') << "\n";

    auto schema = table_->schema();
    for (size_t col_idx = 0; col_idx < table_->num_columns(); ++col_idx) {
        const auto& column = table_->column(col_idx);
        const auto& field = schema->field(col_idx);

        std::cout << std::left
                    << std::setw(20) << field->name()
                    << std::setw(30) << column->type()->ToString()
                    << std::setw(12) << column->length()
                    << std::setw(10) << column->num_chunks()
                    << "\n";
    }
    std::cout << "\n";
};

std::shared_ptr<arrow::Table> Table::getTable() const {
    return table_;
}
