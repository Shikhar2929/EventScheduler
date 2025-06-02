#ifndef LOCKFREE_QUEUE
#define LOCKFREE_QUEUE
#include <iostream> 
#include <vector> 
#include <atomic> 
#include <optional> 

template<typename T> 
class LockFreeQueue {
    public:
        virtual void push(T&& element) = 0;
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
        void push(const T& element);
        void push(T&& element) override;
        std::optional<T> pop() override;
    private: 
        std::vector<std::optional<T>> buffer;
        const size_t capacity;
        alignas(64) std::atomic<size_t> head;
        alignas(64) std::atomic<size_t> tail;
};

// Class for Improved LFQ performance, removing optionals to reduce cache misses 

template <typename T>
class SeqRing final : public LockFreeQueue<T>
{
public:
    explicit SeqRing(std::size_t capacity);
    void push(T&& element) override; // move
    std::optional<T> pop() override;

    template<std::size_t Capacity, typename OutputIt>
    std::size_t pop_batch(OutputIt out);

    ~SeqRing() override;

private:
    struct alignas(64) Counter { std::atomic<uint64_t> value{0}; };

    struct Cell {
        std::atomic<uint64_t> seq;
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
    };

    /* helpers */
    static std::size_t nextPow2(std::size_t n);
    template<typename U>
    void produce(U&& element);
    std::optional<T> consume();
    /* data members */
    const std::size_t      capacity_;   // power of two
    const std::size_t      mask_;       // capacity_‑1
    std::vector<Cell>      buffer_;

    alignas(64) std::atomic<uint64_t> head_{0};   // written by consumers
    alignas(64) std::atomic<uint64_t> tail_{0};   // written by producer
};

#include "lock_free_queue.tpp"

#endif
