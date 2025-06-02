#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "event.hpp"
#include "lock_free_queue.hpp"
#include <queue>
#include <mutex>
#include <condition_variable> 
#include <thread>
#include <atomic> 
#include <iostream> 

class Scheduler {
    public: 
        Scheduler();
        ~Scheduler();
        void scheduleEvent(Event event);
        void start();
        void stop();
        void markDone();
        void waitUntilFinished();

    private:
        void run();
        void executeTask(Event& task);

        std::atomic<bool> running;
        std::atomic<bool> doneSubmitting;
        std::atomic<size_t> tasksSubmitted;
        std::atomic<size_t> tasksCompleted;
        SeqRing<Event> event_queue;
        std::vector<std::thread> workers;

};
#endif