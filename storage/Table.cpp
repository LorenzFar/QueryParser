#include "Table.h"
#include <iostream>
#include <stdexcept>
#include <parquet/arrow/reader.h>
#include <arrow/io/api.h>

//Constructor
Table::Table(const std::string& filename) {
    arrow::MemoryPool* pool = arrow::default_memory_pool();

    auto input_result = arrow::io::ReadableFile::Open(filename);
    if (!input_result.ok()) throw std::runtime_error(input_result.status().ToString());
    auto input = *input_result;

    parquet_reader_ = parquet::ParquetFileReader::Open(input);

    std::unique_ptr<parquet::arrow::FileReader> arrow_reader;
    PARQUET_THROW_NOT_OK(parquet::arrow::OpenFile(input, pool));
    arrow_reader_ = std::move(arrow_reader);
}

ColumnStats Table::getColumnStats(int row_group, int col) const {
    auto file_meta = parquet_reader_ -> metadata();
    auto rg_meta = file_meta -> RowGroup(row_group);
    auto col_meta = rg_meta -> ColumnChunk(col);
    auto schema = file_meta -> schema();

    ColumnStats cs;
    cs.row_group = row_group;
    cs.column = col;
    cs.col_name = schema -> Column(col) -> name();
    cs.has_stats = false;

    auto stats = col_meta -> statistics();
    if(!stats || !stats -> HasMinMax()){
        return cs;
    }
    cs.has_stats = true;

    fillMinMax(cs, stats);
    return cs;
}

std::vector<ColumnStats> Table:: getRowGroupStats(int row_group) const {
    std::vector<ColumnStats> result;
    int num_cols = numColumns();

    result.reserve(num_cols);

    for(int columns = 0; columns < num_cols; ++columns){
        result.push_back(getColumnStats(row_group, columns));
    }
    return result;
}

std::vector<ColumnStats> Table::getAllStats() const {
    std::vector<ColumnStats> result;
    int num_row_group = numRowGroups();
    int num_column = numColumns();

    result.reserve(num_row_group * num_column);

    for(int row_group = 0; row_group < num_row_group; ++row_group){
        for(int column = 0; column < num_column; ++column){
            result.push_back(getColumnStats(row_group, column));
        }
    }
    return result;
}

void Table::fillMinMax(ColumnStats& cs, const std::shared_ptr<parquet::Statistics>& stats) const {
    switch (stats->physical_type()) { 
        case parquet::Type::INT32: { 
            auto s = std::static_pointer_cast<parquet::TypedStatistics<parquet::Int32Type>>(stats); 
            cs.min_val = s->min(); 
            cs.max_val = s->max(); break; 
        } 
        case parquet::Type::INT64: { 
            auto s = std::static_pointer_cast<parquet::TypedStatistics<parquet::Int64Type>>(stats); 
            cs.min_val = s->min(); 
            cs.max_val = s->max(); break; 
        } 
        case parquet::Type::FLOAT: { 
            auto s = std::static_pointer_cast<parquet::TypedStatistics<parquet::FloatType>>(stats); 
            cs.min_val = s->min();
            cs.max_val = s->max();
            break; 
        } 
        case parquet::Type::DOUBLE: { 
            auto s = std::static_pointer_cast<parquet::TypedStatistics<parquet::DoubleType>>(stats); 
            cs.min_val = s->min();
            cs.max_val = s->max(); 
            break; 
        } 
        case parquet::Type::BYTE_ARRAY: { 
            auto s = std::static_pointer_cast<parquet::TypedStatistics<parquet::ByteArrayType>>(stats); 
            cs.min_val = s->min().ptr ? std::string(reinterpret_cast<const char*>(s->min().ptr), s->min().len) : ""; 
            cs.max_val = s->max().ptr ? std::string(reinterpret_cast<const char*>(s->max().ptr), s->max().len) : ""; 
            break; 
        } 
        default:
            cs.has_stats = false; 
    }
}

void Table::printSchema() const {
    auto schema = parquet_reader_->metadata()->schema();
    int num_cols = schema->num_columns();

    const int w_idx  = 6;
    const int w_name = 30;
    const int w_type = 20;
    const int w_log  = 20;

    const int total = w_idx + w_name + w_type + w_log + 4; 

    auto hline = [&]() {
        std::cout << std::string(total, '-') << '\n';
    };

    auto row = [&](const std::string& idx,
                   const std::string& name,
                   const std::string& type,
                   const std::string& logical) {
        std::cout << std::left
                  << std::setw(w_idx)  << idx    << " "
                  << std::setw(w_name) << name   << " "
                  << std::setw(w_type) << type   << " "
                  << std::setw(w_log)  << logical
                  << '\n';  
    };

    hline();
    row("#", "Column Name", "Physical Type", "Logical Type");
    hline();

    for (int i = 0; i < num_cols; ++i) {
        const auto* col = schema->Column(i);

        std::string phys = parquet::TypeToString(col->physical_type());

        std::string logic = "NONE";
        if (col->logical_type() && !col->logical_type()->is_none()) {
            logic = col->logical_type()->ToString();
        }

        row(std::to_string(i), col->name(), phys, logic);
    }

    hline();
    std::cout << num_cols << " column(s)  |  "
              << numRowGroups() << " row group(s)  |  "
              << parquet_reader_->metadata()->num_rows() << " total row(s)\n";
    hline();
}


int Table::numRowGroups() const {
    return parquet_reader_ -> metadata() -> num_row_groups();
}

int Table::numColumns() const {
    return parquet_reader_ -> metadata() -> num_columns();
}




