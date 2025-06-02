#include "../include/scheduler.hpp"
#include "../include/scope_timer.hpp"
#include <iostream> 

Scheduler::Scheduler() : running(false), doneSubmitting(false), event_queue(1024 * 1024) {}
Scheduler::~Scheduler() {
    stop();
}
void Scheduler::start() {
    running = true;
    doneSubmitting = false;

    size_t thread_count = std::thread::hardware_concurrency();
    if (thread_count == 0) thread_count = 4;

    for (size_t i = 0; i < thread_count; ++i) {
        workers.emplace_back(&Scheduler::run, this);
    }
    #ifdef TELEMETRY_ENABLED
    std::cout << "Scheduler started with " << thread_count << " threads.\n";
    #endif
}

// Stop the scheduler and join all threads
void Scheduler::stop() {
    if(!running) 
        return;
    running = false;
    for (std::thread& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    workers.clear();
    #ifdef TELEMETRY_ENABLED
    std::cout << "Scheduler stopped.\n";
    #endif
}

void Scheduler::scheduleEvent(Event event) {
    tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
    event_queue.push(std::move(event));
}

void Scheduler::markDone() {
    doneSubmitting.store(true);
}

void Scheduler::run() {
    //#ifdef TELEMETRY_ENABLED
    //std::cout << "Worker Started With " << std::this_thread::get_id() << std::endl;
    //#endif
    constexpr std::size_t BATCH_CAP = 16;
    std::array<Event, 16> buf;
    while (running) {
        std::size_t got = event_queue.pop_batch<BATCH_CAP>(buf.begin());
        if (got == 0) {
            std::this_thread::yield();
            continue;
        }
        for (std::size_t i = 0; i < got; ++i) {
            executeTask(buf[i]);
        }
        tasksCompleted.fetch_add(got, std::memory_order_relaxed);
    }
}

void Scheduler::waitUntilFinished() {
    while (tasksCompleted.load(std::memory_order_relaxed) <
           tasksSubmitted.load(std::memory_order_relaxed) && running) {
        std::this_thread::sleep_for(std::chrono::microseconds(5));
    }
}

void Scheduler::executeTask(Event& task) {
    if (task.getId() == 0) return;
    #ifdef TELEMETRY_ENABLED
        ScopeTimer t("Event " + std::to_string(task.getId()));
    #endif
    task.execute();
}