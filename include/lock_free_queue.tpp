#pragma once
#include "lock_free_queue.hpp"
template<typename T>
SPMC<T>::SPMC(size_t cap)
    : buffer(cap), capacity(cap), head(0), tail(0) {}

template<typename T>
void SPMC<T>::push(const T& element) {
    size_t tail_val = tail.load(std::memory_order_relaxed);
    size_t next_tail = (tail_val + 1) % capacity;

    if (next_tail == head.load(std::memory_order_acquire)) {
        // Queue is full — drop the element
        //#ifdef TELEMETRY_ENABLED
        std::cout << "Queue is full & item is dropped" << std::endl;
        //#endif
        return;
    }

    buffer[tail_val] = element;
    tail.store(next_tail, std::memory_order_release);
}

template<typename T>
std::optional<T> SPMC<T>::pop() {
    size_t head_val = head.load(std::memory_order_relaxed);

    if (head_val == tail.load(std::memory_order_acquire)) {
        return std::nullopt;  // Queue is empty
    }
    std::optional<T>& opt = buffer[head_val];
    std::optional<T> item = std::move(opt);
    opt.reset();
    
    head.store((head_val + 1) % capacity, std::memory_order_release);
    return item;
}
