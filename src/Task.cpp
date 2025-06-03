#include "../external/Task.hpp"

//=======================
// promise_type methods
//=======================

Task Task::promise_type::get_return_object() noexcept {
    return Task{ std::coroutine_handle<promise_type>::from_promise(*this) };
}

//=======================
// Task member methods
//=======================

Task::Task(handle_type h) noexcept
    : handle_(h)
{}

Task::Task(Task&& other) noexcept
    : handle_(other.handle_)
{
    other.handle_ = nullptr;
}

// For the move operator 
Task& Task::operator=(Task&& other) noexcept {
    if (this != &other) {
        if (handle_)
            handle_.destroy();
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

Task::~Task() {
    if (handle_)
        handle_.destroy();
}

bool Task::await_ready() const noexcept {
    return !handle_ || handle_.done();
}

void Task::await_suspend(std::coroutine_handle<> continuation) noexcept {
    // Store the awaiting coroutine’s handle so we can resume it later
    handle_.promise().continuation_ = continuation;
    // Enqueue this Task’s coroutine for execution
    schedule_coroutine(handle_);
}

void Task::await_resume() {
    if (handle_.promise().exception_)
        std::rethrow_exception(handle_.promise().exception_);
}

void Task::start() noexcept {
    if (handle_)
        schedule_coroutine(handle_);
}

//=======================
// Default scheduler
//=======================

void schedule_coroutine(std::coroutine_handle<> coro) noexcept {
    // -------------------------------------------------------------------
    // By default, simply resume immediately on the calling thread.
    // To integrate with your lock-free queues, replace this body so that
    // `coro` is pushed into your scheduler’s ready-queue instead.
    // For example:
    //     my_scheduler.enqueue(coro);
    // -------------------------------------------------------------------
    coro.resume();
}
