// event.hpp
#ifndef EVENT_HPP
#define EVENT_HPP

#include <functional>
#include <cstdint>
#include <string>

class Event {
public:
    using Callback = std::function<void()>;

    Event(uint64_t id, Callback cb, const std::string& name = "")
        : event_id(id), callback(std::move(cb)), event_name(name) {}

    void execute() const {
        if (callback) callback();
    }

    uint64_t getId() const { return event_id; }
    std::string getName() const { return event_name; }

private:
    uint64_t event_id;
    Callback callback;
    std::string event_name;
};

#endif
