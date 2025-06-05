#include "../include/benchmark_suite.hpp"

int main() {
    std::vector<long long> results;
    for (int i = 0; i < 1; i++) {
        BenchmarkSuite::DependencyGraphDemo();
        std::cout << "-------\n";
    }
    /*for (int i = 0; i < 5; ++i) {
        BenchmarkSuite::MatrixMultiplicationBenchmark(1000, results);
    }
    BenchmarkSuite::Summarize("Scheduler Hash Benchmark", results);*/
}