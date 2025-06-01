#include "../include/scheduler.hpp"
#include "../include/event.hpp"
#include "../include/scope_timer.hpp"
#include <iostream>
#include <chrono>
#include <atomic>
#include <thread>
#include <vector>
#include <random>

constexpr size_t NUM_EVENTS = 1'000'000;
constexpr size_t DATA_SIZE = 1024 * 1024; // 1M integers
std::vector<int> array(DATA_SIZE, 42);
std::atomic<size_t> globalSum = 0;
const size_t MOD  = 997;
void basicScheduler() {
    std::atomic<size_t> completed = 0;
    Scheduler scheduler;
    scheduler.start();
    {
        ScopeTimer t("Total Task Scheduling + Execution");

        for (size_t i = 1; i <= NUM_EVENTS; ++i) {
            scheduler.scheduleEvent(Event(i, [&completed, i]() {
                size_t local = 0;
                local += array[(i * 75431) % DATA_SIZE];
                globalSum.fetch_add(local, std::memory_order_relaxed);
                completed.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        // Wait until all tasks are completed
        while (completed.load(std::memory_order_relaxed) < NUM_EVENTS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    scheduler.stop();
}

void basicBenchmark() {
    std::atomic<size_t> completed = 0;
    {
        ScopeTimer t("Total Task Scheduling + Execution, Benchmark Version");
        for (size_t i = 1; i <= NUM_EVENTS; ++i) {
            size_t local = 0;
            local += array[(i * 75431) % DATA_SIZE];
            globalSum.fetch_add(local, std::memory_order_relaxed);
            completed.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void schedulerHash(std::vector<long long>& results) {
    std::atomic<size_t> completed = 0;
    globalSum.store(0);
    Scheduler scheduler;
    scheduler.start();
    {
        ScopeTimer t("Total Task Scheduling + Execution", &results);

        constexpr size_t CHUNK_SIZE = 10000;
        constexpr size_t NUM_CHUNKS = DATA_SIZE / CHUNK_SIZE;

        for (size_t i = 0; i < NUM_CHUNKS; ++i) {
            scheduler.scheduleEvent(Event(i + 1, [i, &completed]() {
                size_t localSum = 0;
                size_t base = i * CHUNK_SIZE;
                //Random hashind done
                for (size_t j = 0; j < CHUNK_SIZE; ++j) {
                    int val = array[base + j];
                    localSum += val * val + 17;
                    localSum ^= (localSum << 3);
                }

                globalSum.fetch_add(localSum % MOD, std::memory_order_relaxed);
                completed.fetch_add(1, std::memory_order_relaxed);  
            }));
        }

        while (completed.load(std::memory_order_relaxed) < NUM_CHUNKS) {
            std::this_thread::yield();
        }
        std::cout << "Global Sum: " << globalSum << std::endl;
    }
    scheduler.stop();
}
void benchmarkHash(std::vector<long long>& results) {
    ScopeTimer t("Total Task Scheduling + Execution, Benchmark Version", &results);
    globalSum.store(0);
    constexpr size_t CHUNK_SIZE = 10000;
    constexpr size_t NUM_CHUNKS = DATA_SIZE / CHUNK_SIZE;
    bool overflow = false;
    for (size_t i = 0; i < NUM_CHUNKS; ++i) {
        size_t localSum = 0;
        size_t base = i * CHUNK_SIZE;

        for (size_t j = 0; j < CHUNK_SIZE; ++j) {
            int val = array[base + j];
            localSum += val * val + 17;
            localSum ^= (localSum << 3);
        }
        uint64_t result = globalSum.fetch_add(localSum % MOD, std::memory_order_relaxed);
    }
    std::cout << globalSum << std::endl;
    //overflow ? std::cout << "Overflow Occurred!\n" : std::cout<<"No Overflow Occurred!\n";
}

/*
void benchmark_std_function(std::vector<long long>& results) {
    ScopeTimer t("std::function Event Benchmark", &results);

    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        std::function<void()> fn = [] {};
        Event e1(i, fn);  // This version uses std::function-based Event
        e1.execute();
    }
}*/


void benchmark_custom_event(std::vector<long long>& results) {
    ScopeTimer t("Inline Custom Event Benchmark", &results);

    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        Event e2(i, [] {});  // This version uses your inline storage
        e2.execute();
    }
}

void summarize(const std::string& label, const std::vector<long long>& data) {
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
void baselineBenchmark(std::vector<long long>& results) {
    globalSum.store(0);
    {
        ScopeTimer timer("Task Alone Benchmark", &results);
        for (size_t i = 1; i <= NUM_EVENTS; ++i) {
            volatile size_t x = 0;
            for (int j = 0; j < 100; ++j) x += j * j;
            globalSum.fetch_add(x);
        }
    }
    std::cout << "Global Sum: " << globalSum << std::endl;
}
void benchmarkEventScheduler(std::vector<long long>& results) {
    globalSum.store(0);
    std::atomic<size_t> completed = 0;
    Scheduler scheduler;
    scheduler.start();

    {
        ScopeTimer timer("Event Scheduler Benchmark", &results);

        for (size_t i = 1; i <= NUM_EVENTS; ++i) {
            scheduler.scheduleEvent(Event(i, [&completed]() {
                volatile size_t x = 0;
                for (int j = 0; j < 100; ++j) x += j * j;
                globalSum.fetch_add(x);
                completed.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        while (completed.load(std::memory_order_relaxed) < NUM_EVENTS) {
            std::this_thread::yield();
        }
    }
    std::cout << "Global Sum: " << globalSum << std::endl;
    scheduler.stop();
}


int main() {
    //schedulerHash();    
    //benchmarkHash();
    
    std::vector<long long> trials;
    
    for (int i = 0; i < 40; ++i) {
        schedulerHash(trials);        
    }
    summarize("Benchmark Event Scheduler", trials);
    
    return 0;
}
