#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "event.hpp"
#include <queue>
#include <mutex>
#include <condition_variable> 
#include <thread>
#include <atomic> 

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

};
#endif