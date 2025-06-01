#pragma once
#include "lock_free_queue.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <vector>
#include <thread>
#include <cassert>
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

// R-value implementation support
template<typename T>
void SPMC<T>::push(T&& element)
{
    size_t tail_val = tail.load(std::memory_order_relaxed);
    size_t next_tail = (tail_val + 1) % capacity;

    if (next_tail == head.load(std::memory_order_acquire)) {
        std::cout << "Queue is full & item is dropped\n";
        return;
    }

    buffer[tail_val].emplace(std::move(element));   // move instead of copy
    tail.store(next_tail, std::memory_order_release);
}

template<typename T>
std::optional<T> SPMC<T>::pop() {
    size_t head_val;
    size_t next_head;
    do {
        head_val = head.load(std::memory_order_relaxed);
        size_t tail_val = tail.load(std::memory_order_acquire);

        if (head_val == tail_val) {
            return std::nullopt;
        }

        next_head = (head_val + 1) % capacity;
    } while (!head.compare_exchange_weak(head_val, next_head, std::memory_order_acq_rel));

    std::optional<T>& opt = buffer[head_val];
    std::optional<T> item = std::move(opt);
    opt.reset();
    return item;
}

// ------- SEQ RING IMPLEMENTATION --------

template<typename T>
inline std::size_t SeqRing<T>::nextPow2(std::size_t n)
{
    assert(n >= 2);
    n--; n |= n >> 1;  n |= n >> 2;  n |= n >> 4;
    n |= n >> 8;  n |= n >> 16; n |= n >> 32;
    return ++n;
}

template<typename T>
SeqRing<T>::SeqRing(std::size_t cap)
    : capacity_(nextPow2(cap)),
      mask_    (capacity_ - 1),
      buffer_  (capacity_)
{
    for (std::size_t i = 0; i < capacity_; ++i)
        buffer_[i].seq.store(i, std::memory_order_relaxed);
}

template<typename T>
SeqRing<T>::~SeqRing()
{
    uint64_t h = head_.load(std::memory_order_relaxed);
    uint64_t t = tail_.load(std::memory_order_relaxed);
    while (h != t) {
        reinterpret_cast<T*>(&buffer_[h & mask_].storage)->~T();
        ++h;
    }
}

template<typename T>
void SeqRing<T>::push(const T& elem)   { produce(elem); }

template<typename T>
void SeqRing<T>::push(T&& elem)        { produce(std::move(elem)); }

template<typename T>
template<typename U>
inline void SeqRing<T>::produce(U&& value)
{
    while (true) {
        uint64_t tail = tail_.load(std::memory_order_relaxed);
        Cell&    cell = buffer_[tail & mask_];

        uint64_t seq  = cell.seq.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(seq) -
                        static_cast<intptr_t>(tail);

        if (diff == 0) {                          /* slot free */
            std::construct_at(
                reinterpret_cast<T*>(&cell.storage),
                std::forward<U>(value));

            cell.seq.store(tail + 1, std::memory_order_release);
            tail_.store(tail + 1, std::memory_order_relaxed);
            return;
        }

        if (diff < 0) {                           /* full – backoff */
            std::this_thread::yield();
        } /* else: consumer still reading – retry */
    }
}

template<typename T>
inline std::optional<T> SeqRing<T>::consume()
{
    while (true) {
        uint64_t head = head_.load(std::memory_order_relaxed);
        Cell&    cell = buffer_[head & mask_];

        uint64_t seq  = cell.seq.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(seq) -
                        static_cast<intptr_t>(head + 1);

        if (diff == 0) {                          /* element ready */
            if (head_.compare_exchange_weak(
                    head, head + 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed))
            {
                T* ptr = reinterpret_cast<T*>(&cell.storage);
                std::optional<T> out{ std::move(*ptr) };
                ptr->~T();

                cell.seq.store(head + capacity_,
                               std::memory_order_release);
                return out;
            }
        }
        else if (diff < 0) {                      /* empty */
            return std::nullopt;
        }
        else {                                    /* producer not done */
            std::this_thread::yield();
        }
    }
}

template<typename T>
inline std::optional<T> SeqRing<T>::pop()
{
    return consume();
}
