#include "Table.h"
#include <iostream>
#include <stdexcept>
#include <parquet/column_reader.h>
#include <parquet/arrow/reader.h>
#include <arrow/io/api.h>

Table::Table(const std::string& filename) {
    arrow::MemoryPool* pool = arrow::default_memory_pool();

    auto input_result = arrow::io::ReadableFile::Open(filename);
    if (!input_result.ok())
        throw std::runtime_error(input_result.status().ToString());
    
    std::shared_ptr<arrow::io::RandomAccessFile> input = *input_result;

    parquet_reader_ = parquet::ParquetFileReader::Open(input);

    parquet::ArrowReaderProperties props;

    //For Lineitem table
    props.set_read_dictionary(14, true);
    props.set_read_dictionary(13, true);

    //For Part table
    props.set_read_dictionary(3, true);
    props.set_read_dictionary(6, true);

    parquet::arrow::FileReaderBuilder builder;
    PARQUET_THROW_NOT_OK(builder.Open(input));
    builder.memory_pool(pool);
    builder.properties(props);
    PARQUET_THROW_NOT_OK(builder.Build(&arrow_reader_));
}

std::vector<int64_t> Table::readRawInt64Column(int row_group, int column_index, int64_t num_rows) {
    auto rg_reader = parquet_reader_->RowGroup(row_group);
    auto col_reader = std::static_pointer_cast<parquet::Int64Reader>(
        rg_reader->Column(column_index)
    );
    std::vector<int64_t> values(num_rows);
    int64_t values_read = 0;
    col_reader->ReadBatch(num_rows, nullptr, nullptr, values.data(), &values_read);
    values.resize(values_read);
    return values;
}

std::shared_ptr<arrow::Table> Table::readRowGroup(int row_group, const std::vector<int>& columns) {
    std::shared_ptr<arrow::Table> table;
    arrow::Status st = arrow_reader_->RowGroup(row_group)->ReadTable(columns, &table);
    if (!st.ok())
        throw std::runtime_error(st.ToString());
    return table;
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
    cs.has_bloom = col_meta->bloom_filter_offset().has_value();

    //Fill bloom filter
    if (cs.has_bloom){
        auto& bloom_reader = parquet_reader_ -> GetBloomFilterReader();
        auto row_bloom_reader = bloom_reader.RowGroup(row_group);
        auto bloom_filter = row_bloom_reader->GetColumnBloomFilter(col);

        if (bloom_filter) {
            cs.bloom_filter = std::shared_ptr<parquet::BloomFilter>(std::move(bloom_filter));
            cs.has_bloom = true;
        } else {
            cs.has_bloom = false;
        }
    }

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

int64_t Table::rowGroupSize(int row_group) const {
    return parquet_reader_->metadata()->RowGroup(row_group)->num_rows();
}

int Table::numRowGroups() const {
    return parquet_reader_ -> metadata() -> num_row_groups();
}

int Table::numColumns() const {
    return parquet_reader_ -> metadata() -> num_columns();
}




