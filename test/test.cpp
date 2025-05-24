#include "scheduler.hpp"
#include <iostream> 

int main() {
    Scheduler scheduler;
    scheduler.start();
    scheduler.scheduleEvent(Event(1, [] {
        std::cout << "Event 1 running\n";
    }));
    scheduler.scheduleEvent(Event(2, [] {
        std::cout << "Event 2 running\n";
    }));

    std::this_thread::sleep_for(std::chrono::seconds(1));
    scheduler.stop();

    return 0;
}