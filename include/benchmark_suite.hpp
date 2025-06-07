// benchmark_suite.hpp
#pragma once

#include "../include/scheduler.hpp"
#include "../include/event.hpp"
#include "../include/scope_timer.hpp"
#include <iostream>
#include <chrono>
#include <atomic>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
static std::mutex coutMutex;

class BenchmarkSuite {
public:
    static constexpr size_t NUM_EVENTS = 1'000'000;
    static constexpr size_t DATA_SIZE = 1024 * 1024;
    static constexpr size_t CHUNK_SIZE = 10000;
    static constexpr size_t NUM_CHUNKS = DATA_SIZE / CHUNK_SIZE;
    static const size_t MOD = 997;

private:
    static inline std::vector<int> array = std::vector<int>(DATA_SIZE, 42);
    static inline std::atomic<size_t> globalSum = 0;
    static inline Scheduler scheduler;
    static inline bool schedulerStarted = false;


public:
    static void InitScheduler() {
        if (!schedulerStarted) {
            scheduler.start();
            schedulerStarted = true;
        }
    }

    static void ShutdownScheduler() {
        if (schedulerStarted) {
            scheduler.stop();
            schedulerStarted = false;
        }
    }

    static void BasicScheduler() {
        std::atomic<size_t> completed = 0;
        InitScheduler();
        {
            ScopeTimer t("Basic Scheduler Benchmark");

            for (size_t i = 1; i <= NUM_EVENTS; ++i) {
                scheduler.scheduleEvent(Event(i, [&completed, i]() {
                    size_t local = 0;
                    local += array[(i * 75431) % DATA_SIZE];
                    globalSum.fetch_add(local, std::memory_order_relaxed);
                    completed.fetch_add(1, std::memory_order_relaxed);
                }));
            }

            while (completed.load(std::memory_order_relaxed) < NUM_EVENTS) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    static void BasicBenchmark() {
        std::atomic<size_t> completed = 0;
        {
            ScopeTimer t("Basic Benchmark Version");
            for (size_t i = 1; i <= NUM_EVENTS; ++i) {
                size_t local = 0;
                local += array[(i * 75431) % DATA_SIZE];
                globalSum.fetch_add(local, std::memory_order_relaxed);
                completed.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    static void SchedulerHash(std::vector<long long>& results) {
        globalSum.store(0);
        InitScheduler();
        {
            ScopeTimer t("Scheduler Hash Benchmark With Alternate Wait For Completion", &results);

            for (size_t i = 0; i < NUM_CHUNKS; ++i) {
                scheduler.scheduleEvent(Event(i + 1, [i]() {
                    size_t localSum = 0;
                    size_t base = i * CHUNK_SIZE;
                    for (size_t j = 0; j < CHUNK_SIZE; ++j) {
                        int val = array[base + j];
                        localSum += val * val + 17;
                        localSum ^= (localSum << 3);
                    }
                    globalSum.fetch_add(localSum % MOD, std::memory_order_relaxed);
                }));
            }
            scheduler.markDone();
            scheduler.waitUntilFinished();
            std::cout << "Global Sum: " << globalSum << std::endl;
        }
    }

    static void BenchmarkHash(std::vector<long long>& results) {
        ScopeTimer t("Hash Benchmark Version", &results);
        globalSum.store(0);
        for (size_t i = 0; i < NUM_CHUNKS; ++i) {
            size_t localSum = 0;
            size_t base = i * CHUNK_SIZE;
            for (size_t j = 0; j < CHUNK_SIZE; ++j) {
                int val = array[base + j];
                localSum += val * val + 17;
                localSum ^= (localSum << 3);
            }
            globalSum.fetch_add(localSum % MOD, std::memory_order_relaxed);
        }
        std::cout << globalSum << std::endl;
    }

    static void CustomEventBenchmark(std::vector<long long>& results) {
        ScopeTimer t("Inline Custom Event Benchmark", &results);
        for (size_t i = 0; i < NUM_EVENTS; ++i) {
            Event e(i, [] {});
            e.execute();
        }
    }

    static void BaselineBenchmark(std::vector<long long>& results) {
        globalSum.store(0);
        {
            ScopeTimer t("Task Alone Benchmark", &results);
            for (size_t i = 1; i <= NUM_EVENTS; ++i) {
                volatile size_t x = 0;
                for (int j = 0; j < 100; ++j) x += j * j;
                globalSum.fetch_add(x);
            }
        }
        std::cout << "Global Sum: " << globalSum << std::endl;
    }

    static void EventSchedulerBenchmark(std::vector<long long>& results) {
        globalSum.store(0);
        InitScheduler();
        {
            ScopeTimer t("Event Scheduler Benchmark", &results);

            for (size_t i = 1; i <= NUM_EVENTS; ++i) {
                scheduler.scheduleEvent(Event(i, [&]() {
                    volatile size_t x = 0;
                    for (int j = 0; j < 100; ++j) x += j * j;
                    globalSum.fetch_add(x);
                }));
            }
            scheduler.markDone();
            scheduler.waitUntilFinished();
        }
        std::cout << "Global Sum: " << globalSum << std::endl;
    }

    static void MultiplyMatrices(const std::vector<std::vector<int>>& A,
        const std::vector<std::vector<int>>& B,
        std::vector<std::vector<int>>& C) {
        size_t N = A.size();
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
                int sum = 0;
                for (size_t k = 0; k < N; ++k) {
                    sum += A[i][k] * B[k][j];
                    C[i][j] = sum;
                }
            }
        }
    }

    static void MatrixMultiplicationBenchmark(const int N, std::vector<long long>& results) {
        std::vector<std::vector<int>> A(N, std::vector<int>(N));
        std::vector<std::vector<int>> B(N, std::vector<int>(N));
        std::vector<std::vector<int>> C(N, std::vector<int>(N, 0));

        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(1, 100);

        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < N; ++j) {
            A[i][j] = dist(rng);
            B[i][j] = dist(rng);
            }
        }

        {
            ScopeTimer t("Matrix Multiplication Benchmark", &results);
            MultiplyMatrices(A, B, C);
        }
    }   
    
    static void MatrixMultiplicationSchedulerBenchmark(const int N, std::vector<long long>& results) {
        std::vector<std::vector<int>> A(N, std::vector<int>(N));
        std::vector<std::vector<int>> B(N, std::vector<int>(N));
        std::vector<std::vector<int>> C(N, std::vector<int>(N, 0));
    
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(1, 100);
        InitScheduler();
    
        {
            ScopeTimer t("Matrix Multiplication Scheduler Benchmark", &results);
            for (size_t i = 0; i < N; ++i) {
                auto& row = C[i];
                scheduler.scheduleEvent(Event(i, [i, &A, &B, &C]() {
                    size_t N = A.size();
                    auto &row = C[i];
                    for (size_t j = 0; j < N; ++j) {
                        int sum = 0;
                        for (size_t k = 0; k < N; ++k) {
                            sum += A[i][k] * B[k][j];
                        }
                        row[j] = sum;
                    }
                }));
            }
            scheduler.markDone();
            scheduler.waitUntilFinished();
        }
    }   

    static void Summarize(const std::string& label, const std::vector<long long>& data) {
        if (data.empty()) return;
        long long total = std::accumulate(data.begin(), data.end(), 0LL);
        long long min_v = *std::min_element(data.begin(), data.end());
        long long max_v = *std::max_element(data.begin(), data.end());
        double avg = static_cast<double>(total) / data.size();

        std::cout << "[Stats] " << label << "\n"
                  << "  Avg: " << avg << " µs\n"
                  << "  Min: " << min_v << " µs\n"
                  << "  Max: " << max_v << " µs\n";
    }
    static void DependencyGraphDemo() {
        InitScheduler();
        scheduler.scheduleEvent(
            1,
            [] {
                std::lock_guard<std::mutex> lk(coutMutex);
                std::cout << "[A] running (id=1)\n";
            }, {});

        // B (ID = 2) ─ after A
        scheduler.scheduleEvent(
            2,
            [] {
                std::lock_guard<std::mutex> lk(coutMutex);
                std::cout << "[B] running (id=2) after A\n";
            },
            std::array<uint64_t, 1>{1});

        // D (ID = 4) ─ after A
        scheduler.scheduleEvent(
            4,
            [] {
                std::lock_guard<std::mutex> lk(coutMutex);
                std::cout << "[D] running (id=4) after A\n";
            },
            std::array<uint64_t, 1>{1});

        // C (ID = 3) ─ after B and D
        scheduler.scheduleEvent(
            3,
            [] {
                std::lock_guard<std::mutex> lk(coutMutex);
                std::cout << "[C] running (id=3) after B and D\n";
            },
            std::array<uint64_t, 2>{2, 4});

        scheduler.markDone();
        scheduler.waitUntilFinished();
    }
    static void DeepDependencyBenchmark(std::vector<long long>& results) {
        constexpr size_t LEVELS = 100;
        constexpr size_t EVENTS_PER_LEVEL = 50;
        size_t current_id = 1;
    
        InitScheduler();
    
        {
            ScopeTimer t("Deep Dependency Benchmark", &results);
            std::vector<size_t> previous_level_ids;
    
            // First level — no dependencies
            std::vector<size_t> current_level_ids;
            for (size_t i = 0; i < EVENTS_PER_LEVEL; ++i) {
                size_t id = current_id++;
                current_level_ids.push_back(id);
    
                scheduler.scheduleEvent(
                    id,
                    [id] {
                        volatile size_t x = id;
                        for (int j = 0; j < 100; ++j) x ^= (x << 1);
                    },
                    {}
                );
            }
    
            // Remaining levels
            for (size_t level = 1; level < LEVELS; ++level) {
                previous_level_ids = std::move(current_level_ids);
                current_level_ids.clear();
    
                // Convert to uint64_t list
                std::vector<uint64_t> deps(previous_level_ids.begin(), previous_level_ids.end());    
                for (size_t i = 0; i < EVENTS_PER_LEVEL; ++i) {
                    size_t id = current_id++;
                    current_level_ids.push_back(id);
    
                    scheduler.scheduleEvent(
                        id,
                        [id] {
                            volatile size_t x = id;
                            for (int j = 0; j < 100; ++j) x ^= (x << 1);
                        },
                        {deps.begin(), deps.end()}  // now valid initializer_list<uint64_t>
                    );
                }
            }
    
            scheduler.markDone();
            scheduler.waitUntilFinished();
        }
    }
    
    

};
