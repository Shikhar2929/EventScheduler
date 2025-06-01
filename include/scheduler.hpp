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
        void scheduleEvent(const Event& event);
        void start();
        void stop();

    private:
        void run();
        void executeTask(Event& task);
        std::atomic<bool> running;
        SeqRing<Event> event_queue;
        //std::condition_variable cv;
        std::vector<std::thread> workers;

};
#endif