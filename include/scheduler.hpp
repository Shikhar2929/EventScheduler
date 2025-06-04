#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "event.hpp"
#include "lock_free_queue.hpp"
#include "dependency_context.hpp"

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
        void addDependency(uint64_t from_task_id, uint64_t target_task_id);

        template<typename Fn>
        void scheduleEvent(uint64_t id, Fn&& user_fn);

    private:
        void run();
        void executeEvent(Event& task);
        void notifyFinished(uint64_t finished_id);        

        std::atomic<bool> running;
        std::atomic<bool> doneSubmitting;
        std::atomic<size_t> tasksSubmitted;
        std::atomic<size_t> tasksCompleted;
        SeqRing<Event> event_queue;
        std::vector<std::thread> workers;
        
        std::unordered_map<uint64_t, std::mutex> taskLocks;
        std::unordered_map<uint64_t, std::vector<uint64_t>> subscribers; // vec with all of the children 
        std::unordered_map<uint64_t, size_t> dependencyCount; // parent count of current node 
        std::unordered_map<uint64_t, std::function<void(DependencyContext)>> functionCalls;
};
template<typename Fn>
void Scheduler::scheduleEvent(uint64_t id, Fn&& user_fn) {
    // Get the mutex, create a wrapped mutex
    {
        std::lock_guard<std::mutex> lk(taskLocks[id]);
        functionCalls[id] = std::forward<Fn>(user_fn);
        dependencyCount.try_emplace(id, 0);
        subscribers.try_emplace(id, 0);
    }

    Event wrapped(
        id,
        [this, id]() mutable {
            DependencyContext ctx{this, id};
            functionCalls[id](ctx); // User code with the current context
        },
        ""
    );
    tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
    event_queue.push(std::move(wrapped));
}
#endif