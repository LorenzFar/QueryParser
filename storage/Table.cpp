#include "Table.h"

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




