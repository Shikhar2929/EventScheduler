#ifndef DEPENDENCY_CONTEXT_HPP
#define DEPENDENCY_CONTEXT_HPP

//#include "scheduler.hpp"
#include <cstdint>
class Scheduler;

class DependencyContext {
public:
    void addDependency(uint64_t target_task_id) const;

    /// If the user ever needs to inspect their own ID:
    uint64_t getCurrentId() const {
        return current_task_id_;
    }

private:
    friend class Scheduler;
    DependencyContext(Scheduler* sched, uint64_t id) noexcept;
    Scheduler*   scheduler_;     
    uint64_t     current_task_id_; 
};

#endif
