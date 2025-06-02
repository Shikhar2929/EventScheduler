// event.hpp
#ifndef EVENT_HPP
#define EVENT_HPP

#include <functional>
#include <cstdint>
#include <string>
#include <cstring>

class OldEvent {
public:
    using Callback = std::function<void()>;

    OldEvent(uint64_t id, Callback cb, const std::string& name = "")
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

class Event {
    public:
        Event() noexcept = default;

        template<typename Fn>   
        Event(uint64_t id, Fn &&fn, const std::string& name = "")
            : event_id(id), event_name(name) {
            static_assert(sizeof(Fn) <= sizeof(storage), "Callable too large for inline storage");
            static_assert(std::is_trivially_copyable_v<Fn>, "Callable must be trivially copyable");
            
            new (&storage) Fn(std::forward<Fn>(fn));
    
            invoke = [](void* ptr) {
                (*reinterpret_cast<Fn*>(ptr))();
            };
            destroy = [](void* ptr) {
                reinterpret_cast<Fn*>(ptr)->~Fn();
            };
        }
    
        Event(const Event&) = delete;
        Event& operator=(const Event&) = delete;
    
        Event(Event&& other) noexcept
            : event_id(other.event_id),
              event_name(std::move(other.event_name)),
              invoke(other.invoke),
              destroy(other.destroy) {
            std::memcpy(&storage, &other.storage, sizeof(storage));
            other.invoke = nullptr;
            other.destroy = nullptr;
        }
    
        Event& operator=(Event&& other) noexcept {
            if (this != &other) {
                reset();
    
                event_id = other.event_id;
                event_name = std::move(other.event_name);
                invoke = other.invoke;
                destroy = other.destroy;
                std::memcpy(&storage, &other.storage, sizeof(storage));
    
                other.invoke = nullptr;
                other.destroy = nullptr;
            }
            return *this;
        }
    
        ~Event() {
            reset();
        }
    
        void execute() const {
            if (invoke) invoke(const_cast<void*>(reinterpret_cast<const void*>(&storage)));
        }
    
        uint64_t getId() const { return event_id; }
        std::string getName() const { return event_name; }
    
    
    private:
        void reset() {
            if (destroy) destroy(&storage);
            invoke = nullptr;
            destroy = nullptr;
        }
    
        uint64_t event_id = 0;
        std::string event_name;
    
        // Function pointer for calling the stored callable
        void (*invoke)(void*) = nullptr;
    
        // Function pointer for properly destroying the stored callable
        void (*destroy)(void*) = nullptr;
    
        // Inline storage for the callable (stack-based)
        static constexpr std::size_t StorageSize = 64;
        alignas(alignof(std::max_align_t)) char storage[StorageSize];
    
    };
    
#endif