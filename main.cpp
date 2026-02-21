#include "./storage/Table.h"
#include <iostream>

int main() {
    try {
        Table table = Table("lineitem.parquet"); 
        //std::cout << table.numRowGroups() << "\n";
        table.printSchema();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
