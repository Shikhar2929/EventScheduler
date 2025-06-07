#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "event.hpp"
#include "lock_free_queue.hpp"
#include <span>
#include "concurrent_hash_map.hpp"

#include <queue>
#include <mutex>
#include <condition_variable> 
#include <thread>
#include <atomic> 
#include <iostream> 
#include <vector>
#include <unordered_map>
#include <functional> 
#include <algorithm> 
#include <cassert> 

class Scheduler {
    public: 
        Scheduler();
        ~Scheduler();
        void scheduleEvent(Event event);
        void start();
        void stop();
        void markDone();
        void waitUntilFinished();
        //void addDependency(uint64_t from_task_id, uint64_t target_task_id);

        template<typename Fn>
        void scheduleEvent(uint64_t id, 
            Fn&& user_fn, std::span<const uint64_t> deps);

    private:
        void run();
        void alternate_run();
        void executeEvent(Event& task);
        void notifyFinished(uint64_t finished_id);        
        void ensureTaskRow(uint64_t id);

        std::atomic<bool> running;
        std::atomic<bool> doneSubmitting;
        std::atomic<size_t> tasksSubmitted{0};
        std::atomic<size_t> tasksCompleted{0};
        SeqRing<Event> event_queue;
        std::vector<std::thread> workers;
        
        ConcurrentHashMap<uint64_t, std::mutex> taskLocks;
        ConcurrentHashMap<uint64_t, std::vector<uint64_t>> subscribers;
        ConcurrentHashMap<uint64_t, std::size_t> dependencyCount;
        ConcurrentHashMap<uint64_t, std::function<void()>> functionCalls;
    };
template<typename Fn>
void Scheduler::scheduleEvent(uint64_t id, Fn&& user_fn, std::span<const uint64_t> deps) {
    ensureTaskRow(id);
    {
        std::mutex* mtx = taskLocks.find(id);
        std::lock_guard guard(*mtx);

        functionCalls.insert_or_assign(id, std::forward<Fn>(user_fn));
    }
    dependencyCount.insert_or_assign(id, deps.size());
    for (uint64_t parent : deps) {
        ensureTaskRow(parent);
        subscribers.find(parent)->push_back(id);
    }
    if (deps.size() == 0) {
        Event event{id,
            [this, id]() { (*functionCalls.find(id))(); }};

        tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
        event_queue.push(std::move(event));
    }
}
#endif