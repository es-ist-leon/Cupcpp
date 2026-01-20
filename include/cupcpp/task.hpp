// SPDX-License-Identifier: GPL-3.0-or-later
// Cupcpp - Modern C++ Coroutine Utilities Library
// https://github.com/es-ist-leon/Cupcpp

#ifndef CUPCPP_TASK_HPP
#define CUPCPP_TASK_HPP

#include "config.hpp"

#include <exception>
#include <type_traits>
#include <utility>
#include <variant>
#include <optional>
#include <atomic>
#include <functional>

namespace cupcpp {

// Forward declarations
template<typename T = void>
class Task;

template<typename T>
class TaskPromise;

template<>
class TaskPromise<void>;

namespace detail {

// Base class for awaiter to wait on a coroutine
struct TaskAwaiterBase {
    coroutine_handle<> continuation_{nullptr};

    bool await_ready() const noexcept { return false; }

    template<typename Promise>
    void await_suspend(coroutine_handle<Promise> awaiting) noexcept {
        continuation_ = awaiting;
    }
};

// Result storage for non-void types
template<typename T>
struct TaskResult {
    std::variant<std::monostate, T, std::exception_ptr> result_;

    bool has_value() const noexcept {
        return std::holds_alternative<T>(result_);
    }

    bool has_exception() const noexcept {
        return std::holds_alternative<std::exception_ptr>(result_);
    }

    template<typename U>
    void set_value(U&& value) {
        result_.template emplace<T>(std::forward<U>(value));
    }

    void set_exception(std::exception_ptr ptr) noexcept {
        result_.template emplace<std::exception_ptr>(std::move(ptr));
    }

    T& get() & {
        if (has_exception()) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        return std::get<T>(result_);
    }

    T&& get() && {
        if (has_exception()) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        return std::move(std::get<T>(result_));
    }

    const T& get() const& {
        if (has_exception()) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        return std::get<T>(result_);
    }
};

// Result storage for void type
template<>
struct TaskResult<void> {
    std::optional<std::exception_ptr> exception_;

    bool has_value() const noexcept {
        return !exception_.has_value();
    }

    bool has_exception() const noexcept {
        return exception_.has_value();
    }

    void set_value() noexcept {}

    void set_exception(std::exception_ptr ptr) noexcept {
        exception_ = std::move(ptr);
    }

    void get() const {
        if (exception_) {
            std::rethrow_exception(*exception_);
        }
    }
};

} // namespace detail

// Promise type for Task<T>
template<typename T>
class TaskPromise {
public:
    using value_type = T;
    using handle_type = coroutine_handle<TaskPromise>;

    TaskPromise() = default;

    Task<T> get_return_object() noexcept;

    suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept {
        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }

            coroutine_handle<> await_suspend(handle_type h) noexcept {
                auto& promise = h.promise();
                if (promise.continuation_) {
                    return promise.continuation_;
                }
                return cupcpp::noop_coroutine();
            }

            void await_resume() noexcept {}
        };
        return FinalAwaiter{};
    }

    void unhandled_exception() noexcept {
        result_.set_exception(std::current_exception());
    }

    template<typename U>
    void return_value(U&& value) {
        result_.set_value(std::forward<U>(value));
    }

    T& result() & {
        return result_.get();
    }

    T&& result() && {
        return std::move(result_).get();
    }

    void set_continuation(coroutine_handle<> cont) noexcept {
        continuation_ = cont;
    }

private:
    detail::TaskResult<T> result_;
    coroutine_handle<> continuation_{nullptr};
};

// Promise type specialization for void
template<>
class TaskPromise<void> {
public:
    using value_type = void;
    using handle_type = coroutine_handle<TaskPromise>;

    TaskPromise() = default;

    Task<void> get_return_object() noexcept;

    suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept {
        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }

            coroutine_handle<> await_suspend(handle_type h) noexcept {
                auto& promise = h.promise();
                if (promise.continuation_) {
                    return promise.continuation_;
                }
                return cupcpp::noop_coroutine();
            }

            void await_resume() noexcept {}
        };
        return FinalAwaiter{};
    }

    void unhandled_exception() noexcept {
        result_.set_exception(std::current_exception());
    }

    void return_void() noexcept {}

    void result() const {
        result_.get();
    }

    void set_continuation(coroutine_handle<> cont) noexcept {
        continuation_ = cont;
    }

private:
    detail::TaskResult<void> result_;
    coroutine_handle<> continuation_{nullptr};
};

// Main Task class - a lazy coroutine that produces a single value
template<typename T>
class Task {
public:
    using promise_type = TaskPromise<T>;
    using handle_type = coroutine_handle<promise_type>;
    using value_type = T;

    // Constructors
    Task() noexcept : handle_(nullptr) {}

    explicit Task(handle_type h) noexcept : handle_(h) {}

    // Move-only semantics
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Check if task is valid
    CUPCPP_NODISCARD bool valid() const noexcept {
        return handle_ != nullptr;
    }

    CUPCPP_NODISCARD explicit operator bool() const noexcept {
        return valid();
    }

    // Check if task has completed
    CUPCPP_NODISCARD bool done() const noexcept {
        return handle_ && handle_.done();
    }

    // Awaiter for co_await support
    auto operator co_await() const& noexcept {
        struct Awaiter {
            handle_type handle_;

            bool await_ready() const noexcept {
                return !handle_ || handle_.done();
            }

            coroutine_handle<> await_suspend(coroutine_handle<> awaiting) noexcept {
                handle_.promise().set_continuation(awaiting);
                return handle_;
            }

            decltype(auto) await_resume() {
                CUPCPP_ASSERT(handle_);
                return handle_.promise().result();
            }
        };
        return Awaiter{handle_};
    }

    auto operator co_await() && noexcept {
        struct Awaiter {
            handle_type handle_;

            bool await_ready() const noexcept {
                return !handle_ || handle_.done();
            }

            coroutine_handle<> await_suspend(coroutine_handle<> awaiting) noexcept {
                handle_.promise().set_continuation(awaiting);
                return handle_;
            }

            decltype(auto) await_resume() {
                CUPCPP_ASSERT(handle_);
                return std::move(handle_.promise()).result();
            }
        };
        return Awaiter{handle_};
    }

    // Manual resume (for running without an event loop)
    void resume() {
        CUPCPP_ASSERT(handle_);
        CUPCPP_ASSERT(!handle_.done());
        handle_.resume();
    }

    // Get the underlying handle
    handle_type handle() const noexcept {
        return handle_;
    }

    // Release ownership of the handle
    handle_type release() noexcept {
        auto h = handle_;
        handle_ = nullptr;
        return h;
    }

private:
    handle_type handle_;
};

// Implement get_return_object after Task is defined
template<typename T>
Task<T> TaskPromise<T>::get_return_object() noexcept {
    return Task<T>{handle_type::from_promise(*this)};
}

inline Task<void> TaskPromise<void>::get_return_object() noexcept {
    return Task<void>{handle_type::from_promise(*this)};
}

// Utility function to synchronously run a task to completion
template<typename T>
T sync_wait(Task<T> task) {
    CUPCPP_ASSERT(task.valid());

    struct SyncWaitPromise {
        struct promise_type {
            std::variant<std::monostate, T, std::exception_ptr> result;
            coroutine_handle<> waiting{nullptr};

            SyncWaitPromise get_return_object() {
                return SyncWaitPromise{coroutine_handle<promise_type>::from_promise(*this)};
            }

            suspend_never initial_suspend() noexcept { return {}; }
            suspend_always final_suspend() noexcept { return {}; }

            void unhandled_exception() {
                result.template emplace<std::exception_ptr>(std::current_exception());
            }

            template<typename U>
            void return_value(U&& value) {
                result.template emplace<T>(std::forward<U>(value));
            }
        };

        coroutine_handle<promise_type> handle;

        T get() {
            if (std::holds_alternative<std::exception_ptr>(handle.promise().result)) {
                std::rethrow_exception(std::get<std::exception_ptr>(handle.promise().result));
            }
            return std::move(std::get<T>(handle.promise().result));
        }

        ~SyncWaitPromise() {
            if (handle) {
                handle.destroy();
            }
        }
    };

    auto sync_task = [](Task<T> t) -> SyncWaitPromise {
        co_return co_await std::move(t);
    }(std::move(task));

    while (!sync_task.handle.done()) {
        sync_task.handle.resume();
    }

    return sync_task.get();
}

// Specialization for void
inline void sync_wait(Task<void> task) {
    CUPCPP_ASSERT(task.valid());

    struct SyncWaitPromise {
        struct promise_type {
            std::optional<std::exception_ptr> exception;

            SyncWaitPromise get_return_object() {
                return SyncWaitPromise{coroutine_handle<promise_type>::from_promise(*this)};
            }

            suspend_never initial_suspend() noexcept { return {}; }
            suspend_always final_suspend() noexcept { return {}; }

            void unhandled_exception() {
                exception = std::current_exception();
            }

            void return_void() noexcept {}
        };

        coroutine_handle<promise_type> handle;

        void get() {
            if (handle.promise().exception) {
                std::rethrow_exception(*handle.promise().exception);
            }
        }

        ~SyncWaitPromise() {
            if (handle) {
                handle.destroy();
            }
        }
    };

    auto sync_task = [](Task<void> t) -> SyncWaitPromise {
        co_await std::move(t);
    }(std::move(task));

    while (!sync_task.handle.done()) {
        sync_task.handle.resume();
    }

    sync_task.get();
}

// Helper to create a ready task
template<typename T>
Task<T> make_ready_task(T value) {
    co_return std::move(value);
}

inline Task<void> make_ready_task() {
    co_return;
}

// Helper to create a task that throws
template<typename T, typename E>
Task<T> make_exceptional_task(E&& exception) {
    throw std::forward<E>(exception);
    co_return T{}; // Never reached
}

template<typename E>
Task<void> make_exceptional_task(E&& exception) {
    throw std::forward<E>(exception);
    co_return; // Never reached
}

} // namespace cupcpp

#endif // CUPCPP_TASK_HPP
