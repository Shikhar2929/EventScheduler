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
    //thread_count = 4;
    if (thread_count == 0) thread_count = 4;
    for (size_t i = 0; i < thread_count; ++i) {
        workers.emplace_back(&Scheduler::run, this);
    }
    event_queue.setWorkerCount(thread_count);
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
    ensureTaskRow(event.getId());                       
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
    std::array<Event, BATCH_CAP> buf;
    while (running) {
        std::size_t got = event_queue.pop_batch<BATCH_CAP>(buf.begin());
        if (got == 0) {
            std::this_thread::yield();
            continue;
        }
        for (std::size_t i = 0; i < got; ++i) {
            executeEvent(buf[i]);
        }
        tasksCompleted.fetch_add(got, std::memory_order_relaxed);
    }
} 
void Scheduler::alternate_run() {
    //#ifdef TELEMETRY_ENABLED
    //std::cout << "Worker Started With " << std::this_thread::get_id() << std::endl;
    //#endif
    while (running) {
        auto task_opt = event_queue.pop();
        if (task_opt.has_value()) {
            executeEvent(task_opt.value());  
            tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        }
        else 
            std::this_thread::yield();
    }
}

void Scheduler::waitUntilFinished() {
    std::size_t completed  = tasksCompleted.load(std::memory_order_relaxed);
    std::size_t submitted  = tasksSubmitted.load(std::memory_order_relaxed);
    
    while (completed < submitted && running) {
        #ifdef TELEMETRY_ENABLED
        // std::cout << "TRYING TO FINISH" << std::endl;
        // std::cout << "Completed: " << completed << std::endl;
        // std::cout << "Submitted: " << submitted << std::endl;
        #endif 
        std::this_thread::sleep_for(std::chrono::microseconds(5));
        completed  = tasksCompleted.load(std::memory_order_relaxed);
        submitted  = tasksSubmitted.load(std::memory_order_relaxed);    
    }
}

void Scheduler::executeEvent(Event& event) {
    #ifdef TELEMETRY_ENABLED
    //    ScopeTimer t("Event " + std::to_string(event.getId()));
    #endif
    event.execute();
    //tasksCompleted.fetch_add(1, std::memory_order_relaxed); // TEMP FIX TO TEST IF NOTIFY IS THE ISSUE
    notifyFinished(event.getId());
}

void Scheduler::notifyFinished(uint64_t id) {
    std::vector<uint64_t> fanout;
    {
        std::mutex* m = taskLocks.find(id);
        std::lock_guard lg(*m);
        fanout.swap(*subscribers.find(id));
    }

    for (uint64_t child : fanout) {
        bool ready = false;

        {
            std::mutex* m = taskLocks.find(child);
            std::lock_guard lg(*m);
            auto cnt = dependencyCount.find(child);
            if (--(*cnt) == 0) ready = true;
        }

        if (ready) {
            Event ev{child,
                [this, child]() { (*functionCalls.find(child))(); }};
            tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
            event_queue.push(std::move(ev));
        }
    }

}

void Scheduler::ensureTaskRow(uint64_t id)
{
    taskLocks     .try_emplace(id);
    subscribers   .try_emplace(id);
    dependencyCount.try_emplace(id, std::size_t{0});
    functionCalls .try_emplace(id);
}

// DependencyContext::DependencyContext(Scheduler* sched, uint64_t id) noexcept
//     : scheduler_(sched), current_task_id_(id)
// {}

// void DependencyContext::addDependency(uint64_t target_task_id) const {
//     scheduler_->addDependency(current_task_id_, target_task_id);
// }