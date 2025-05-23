#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "event.hpp"
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
        void startTelemetry();


    private:
        void run();
        std::queue<Event> event_queue;
        std::mutex queue_mutex;
        std::condition_variable cv;
        std::atomic<bool> running;
        std::vector<std::thread> workers;

};
#endif