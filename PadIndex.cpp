#include <iostream>
#include <unordered_map>
#include <vector>

#include "Benchmarks.hpp"
#include "Db.hpp"
#include "Filters.hpp"
#include "Index.hpp"

int main()
{
    std::cout << " *************   loading data   ************* " << std::endl;
    loadDb();
    loadPrecalculatedFilters();
    std::cout << " ************* buildind indexes ************* " << std::endl;
    buildIndexes();
    std::cout << " **************** bechmarks ***************** " << std::endl;
    runBench();
}