#include "../include/benchmark_suite.hpp"

int main() {
    std::vector<long long> results;
    BenchmarkSuite::DeepDependencyBenchmark(results);
    BenchmarkSuite::Summarize("Deep Dependancy", results);
    //for (int i = 0; i < 50; ++i) {
    //     BenchmarkSuite::(1000, results);
    //}
    //BenchmarkSuite::Summarize("Scheduler Hash Benchmark", results);
   //BenchmarkSuite::DependencyGraphDemo();
}