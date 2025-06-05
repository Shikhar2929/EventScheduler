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
            executeEvent(buf[i]);
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

void Scheduler::executeEvent(Event& event) {
    #ifdef TELEMETRY_ENABLED
        ScopeTimer t("Event " + std::to_string(task.getId()));
    #endif
    event.execute();
    notifyFinished(event.getId());
}

void Scheduler::addDependency(uint64_t source_id, uint64_t target_id) {
    if (source_id == target_id) {
        // self dependency check
        return;
    }

    // Use ascending order locking to prevent any deadlocks 
    uint64_t first  = std::min(source_id, target_id);
    uint64_t second = std::max(source_id, target_id);

    // Acquire the locks (in ID order)
    std::lock_guard<std::mutex> lk1(taskLocks[first]);
    std::lock_guard<std::mutex> lk2(taskLocks[second]);

    // Record that “from_task_id” is waiting on “target_task_id”
    subscribers[target_id].push_back(source_id);
    dependencyCount[source_id] += 1;
}

void Scheduler::notifyFinished(uint64_t finished_id) {
    std::vector<uint64_t> notifyList;
    {
        std::lock_guard<std::mutex> lk(taskLocks[finished_id]);
        notifyList.swap(subscribers[finished_id]);
    }

    for (uint64_t sub : notifyList) {
        bool enqueue = false;
        {
            std::lock_guard<std::mutex> lk(taskLocks[sub]);
            assert(dependencyCount[sub] > 0 && "subscribe without entry?");
            dependencyCount[sub] -= 1;
            if (dependencyCount[sub] == 0) {
                // All of sub_id’s declared dependencies are now satisfied
                dependencyCount.erase(sub);    // clean up
                enqueue = true;
            }
        }

        if (enqueue) {
            std::function<void(DependencyContext)> fn;
            {
                std::lock_guard<std::mutex> lk(taskLocks[sub]);
                fn = functionCalls[sub];
            }
            Event wrapped(
                sub,
                [this, sub]() mutable {
                    DependencyContext ctx{this, sub};
                    functionCalls[sub](ctx);
                },
                ""
            );
            tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
            event_queue.push(std::move(wrapped));
        }
    }
}

DependencyContext::DependencyContext(Scheduler* sched, uint64_t id) noexcept
    : scheduler_(sched), current_task_id_(id)
{}

void DependencyContext::addDependency(uint64_t target_task_id) const {
    scheduler_->addDependency(current_task_id_, target_task_id);
}