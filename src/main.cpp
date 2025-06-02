#include "../include/benchmark_suite.hpp"

int main() {
    std::vector<long long> results;
    for (int i = 0; i < 5000; ++i) {
        BenchmarkSuite::SchedulerHash(results);
    }
    BenchmarkSuite::Summarize("Scheduler Hash Benchmark", results);
}