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
std::atomic<size_t> global_sum = 0;

int main() {
    // Shared memory block to simulate cache-thrashing access

    // Atomic counter to track completions
    std::atomic<size_t> completed = 0;
    Scheduler scheduler;
    scheduler.start();
    {
        ScopeTimer t("Total Task Scheduling + Execution");

        for (size_t i = 1; i <= NUM_EVENTS; ++i) {
            scheduler.scheduleEvent(Event(i, [&completed, i]() {
                size_t local = 0;
                local += array[(i * 75431) % DATA_SIZE];
                global_sum.fetch_add(local, std::memory_order_relaxed);
                completed.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        // Wait until all tasks are completed
        while (completed.load(std::memory_order_relaxed) < NUM_EVENTS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    scheduler.stop();
    //std::cout <<"Global Sum: " << global_sum << std::endl;

    return 0;
}
