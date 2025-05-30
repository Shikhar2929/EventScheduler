#include "../include/scheduler.hpp"
#include "../include/scope_timer.hpp"
#include <iostream> 

Scheduler::Scheduler() : running(false), event_queue(1024 * 1024) {}
Scheduler::~Scheduler() {
    stop();
}
void Scheduler::start() {
    running = true;
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

void Scheduler::scheduleEvent(const Event& event) {
    event_queue.push(event);
}

void Scheduler::run() {
    #ifdef TELEMETRY_ENABLED
    std::cout << "Worker Started With " << std::this_thread::get_id() << std::endl;
    #endif
    while (running) {
        auto task_opt = event_queue.pop();
        if (task_opt.has_value())
            executeTask(task_opt.value());  
        else 
            std::this_thread::yield();
        /*
        Event task(0, nullptr);

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [this] {
                return !event_queue.empty() || !running;
            });

            if (!running && event_queue.empty()) return;

            task = std::move(event_queue.front());
            event_queue.pop();
        }
        executeTask(task);
        */
    }
}
void Scheduler::executeTask(Event& task) {
    if (task.getId() == 0) return;
    #ifdef TELEMETRY_ENABLED
        ScopeTimer t("Event " + std::to_string(task.getId()));
    #endif
    task.execute();
}