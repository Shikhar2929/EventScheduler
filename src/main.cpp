#include "../include/benchmark_suite.hpp"

int main() {
    std::vector<long long> results;
    for (int i = 0; i < 40; ++i) {
        BenchmarkSuite::SchedulerHash(results);
    }
    BenchmarkSuite::Summarize("Scheduler Hash Benchmark", results);
}