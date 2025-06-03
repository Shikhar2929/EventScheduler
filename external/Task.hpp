#ifndef TASK_HPP
#define TASK_HPP

#include <coroutine>
#include <exception>

/// Schedule a coroutine for execution.
/// Default implementation resumes immediately; replace in Task.cpp
/// to enqueue into your event/scheduler.
void schedule_coroutine(std::coroutine_handle<> coro) noexcept;

class Task {
public:
    struct promise_type {
        std::exception_ptr exception_;                
        std::coroutine_handle<> continuation_ = nullptr;

        Task get_return_object() noexcept;

        // Do not run the coroutine body immediately—suspend until scheduled
        std::suspend_always initial_suspend() noexcept { return {}; }

        // The Final Awaiter object is to resume tasks when the task completes
        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            template<typename Promise>
            void await_suspend(std::coroutine_handle<Promise> h) noexcept {
                if (h.promise().continuation_)
                    h.promise().continuation_.resume();
            }
            void await_resume() noexcept {}
        };
        FinalAwaiter final_suspend() noexcept { return {}; }

        // Capture any uncaught exception
        void unhandled_exception() noexcept {
            exception_ = std::current_exception();
        }
        void return_void() noexcept {}
    };

    using handle_type = std::coroutine_handle<promise_type>;

    // Constructors / Destructor
    explicit Task(handle_type h) noexcept;
    Task(Task&& other) noexcept;
    Task& operator=(Task&& other) noexcept;
    ~Task();

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> continuation) noexcept;
    void await_resume();

    // Fire-and-forget: enqueue without awaiting
    void start() noexcept;

private:
    handle_type handle_;
};

#endif
