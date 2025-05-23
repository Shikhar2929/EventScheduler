#include "scheduler.hpp"
#include <iostream> 

Scheduler::Scheduler() : running(false) {}
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

    std::cout << "Scheduler started with " << thread_count << " threads.\n";
}

// Stop the scheduler and join all threads
void Scheduler::stop() {
    if(!running) 
        return;
    running = false;
    cv.notify_all();  // Wake up all workers to exit

    for (std::thread& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    workers.clear();
    std::cout << "Scheduler stopped.\n";
}

void Scheduler::scheduleEvent(const Event& event) {
    //Create a new scope to acquire the lock
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        event_queue.push(event);    
    }
    cv.notify_all();
}

void Scheduler::run() {
    while (running) {
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

        if (task.getId() != 0) {
            task.execute();
        }
    }
}