#ifndef LOCKFREE_QUEUE
#define LOCKFREE_QUEUE
#include <iostream> 
#include <vector> 
#include <atomic> 
#include <optional> 

template<typename T> 
class LockFreeQueue {
    public:
        virtual void push(const T& element) = 0;
        virtual std::optional<T> pop() = 0;
        //const, not modifying the object
        virtual ~LockFreeQueue() = default;    
};

// Class for "Single Producer, Multiple Consumer Lock Free Queue"
//
// When capacity is exceeded, drops will occur
// Simple Circular Buffer Implementation
template<typename T>
class SPMC : public LockFreeQueue<T> {    
    public: 
        explicit SPMC(size_t capacity);
        void push(const T& element) override;
        std::optional<T> pop() override;
    private: 
        std::vector<std::optional<T>> buffer;
        const size_t capacity;
        alignas(64) std::atomic<size_t> head;
        alignas(64) std::atomic<size_t> tail;
};
#include "lock_free_queue.tpp"

#endif
