#include "./storage/Table.h"
#include <iostream>

int main() {
    try {
        Table customerTable("customer.parquet");
        customerTable.displayTable();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
