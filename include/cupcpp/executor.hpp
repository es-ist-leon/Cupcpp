// SPDX-License-Identifier: GPL-3.0-or-later
// Cupcpp - Modern C++ Coroutine Utilities Library
// https://github.com/es-ist-leon/Cupcpp

#ifndef CUPCPP_EXECUTOR_HPP
#define CUPCPP_EXECUTOR_HPP

#include "config.hpp"
#include "task.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

namespace cupcpp {

// ============================================================================
// ThreadPool - Execute coroutines on a pool of worker threads
// ============================================================================

class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads = std::thread::hardware_concurrency())
        : stopped_(false) {
        if (num_threads == 0) {
            num_threads = 1;
        }

        workers_.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ~ThreadPool() {
        stop();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Schedule a coroutine handle to be resumed on the pool
    void schedule(coroutine_handle<> handle) {
        if (!handle) return;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopped_) return;
            tasks_.push_back(handle);
        }
        cv_.notify_one();
    }

    // Schedule a callable to be executed on the pool
    template<typename F>
    void execute(F&& func) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopped_) return;
            callbacks_.push_back(std::forward<F>(func));
        }
        cv_.notify_one();
    }

    // Submit a task and get a future for the result
    template<typename F, typename R = std::invoke_result_t<F>>
    std::future<R> submit(F&& func) {
        auto promise = std::make_shared<std::promise<R>>();
        auto future = promise->get_future();

        execute([p = std::move(promise), f = std::forward<F>(func)]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    f();
                    p->set_value();
                } else {
                    p->set_value(f());
                }
            } catch (...) {
                p->set_exception(std::current_exception());
            }
        });

        return future;
    }

    // Awaiter to switch execution to the thread pool
    auto schedule_on() {
        struct ScheduleAwaiter {
            ThreadPool& pool;

            bool await_ready() const noexcept { return false; }

            void await_suspend(coroutine_handle<> handle) noexcept {
                pool.schedule(handle);
            }

            void await_resume() const noexcept {}
        };
        return ScheduleAwaiter{*this};
    }

    // Stop the pool and wait for all tasks to complete
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopped_) return;
            stopped_ = true;
        }
        cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // Check if pool is stopped
    CUPCPP_NODISCARD bool is_stopped() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return stopped_;
    }

    // Get number of worker threads
    CUPCPP_NODISCARD std::size_t size() const noexcept {
        return workers_.size();
    }

    // Get number of pending tasks
    CUPCPP_NODISCARD std::size_t pending() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size() + callbacks_.size();
    }

private:
    void worker_loop() {
        while (true) {
            coroutine_handle<> task{nullptr};
            std::function<void()> callback;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                cv_.wait(lock, [this] {
                    return stopped_ || !tasks_.empty() || !callbacks_.empty();
                });

                if (stopped_ && tasks_.empty() && callbacks_.empty()) {
                    return;
                }

                if (!tasks_.empty()) {
                    task = tasks_.front();
                    tasks_.pop_front();
                } else if (!callbacks_.empty()) {
                    callback = std::move(callbacks_.front());
                    callbacks_.pop_front();
                }
            }

            if (task) {
                task.resume();
            } else if (callback) {
                callback();
            }
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::deque<coroutine_handle<>> tasks_;
    std::deque<std::function<void()>> callbacks_;
    bool stopped_;
};

// ============================================================================
// InlineExecutor - Executes tasks immediately on the calling thread
// ============================================================================

class InlineExecutor {
public:
    void schedule(coroutine_handle<> handle) {
        if (handle) {
            handle.resume();
        }
    }

    template<typename F>
    void execute(F&& func) {
        func();
    }

    auto schedule_on() {
        struct ScheduleAwaiter {
            bool await_ready() const noexcept { return true; }
            void await_suspend(coroutine_handle<>) const noexcept {}
            void await_resume() const noexcept {}
        };
        return ScheduleAwaiter{};
    }
};

// ============================================================================
// ManualExecutor - Queue tasks to be run manually (useful for testing)
// ============================================================================

class ManualExecutor {
public:
    void schedule(coroutine_handle<> handle) {
        if (handle) {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push_back(handle);
        }
    }

    template<typename F>
    void execute(F&& func) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.push_back(std::forward<F>(func));
    }

    auto schedule_on() {
        struct ScheduleAwaiter {
            ManualExecutor& executor;

            bool await_ready() const noexcept { return false; }

            void await_suspend(coroutine_handle<> handle) noexcept {
                executor.schedule(handle);
            }

            void await_resume() const noexcept {}
        };
        return ScheduleAwaiter{*this};
    }

    // Run one pending task
    bool run_one() {
        coroutine_handle<> task{nullptr};
        std::function<void()> callback;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!tasks_.empty()) {
                task = tasks_.front();
                tasks_.pop_front();
            } else if (!callbacks_.empty()) {
                callback = std::move(callbacks_.front());
                callbacks_.pop_front();
            }
        }

        if (task) {
            task.resume();
            return true;
        } else if (callback) {
            callback();
            return true;
        }

        return false;
    }

    // Run all pending tasks
    std::size_t run_all() {
        std::size_t count = 0;
        while (run_one()) {
            ++count;
        }
        return count;
    }

    // Check if there are pending tasks
    CUPCPP_NODISCARD bool has_pending() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return !tasks_.empty() || !callbacks_.empty();
    }

    // Get number of pending tasks
    CUPCPP_NODISCARD std::size_t pending() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size() + callbacks_.size();
    }

private:
    mutable std::mutex mutex_;
    std::deque<coroutine_handle<>> tasks_;
    std::deque<std::function<void()>> callbacks_;
};

// ============================================================================
// Scheduling awaiters
// ============================================================================

// Resume on a specific executor
template<typename Executor>
auto resume_on(Executor& executor) {
    struct ResumeOnAwaiter {
        Executor& executor;

        bool await_ready() const noexcept { return false; }

        void await_suspend(coroutine_handle<> handle) noexcept {
            executor.schedule(handle);
        }

        void await_resume() const noexcept {}
    };
    return ResumeOnAwaiter{executor};
}

// Yield control to allow other coroutines to run
inline auto yield() {
    struct YieldAwaiter {
        bool await_ready() const noexcept { return false; }

        void await_suspend(coroutine_handle<> handle) noexcept {
            handle.resume(); // Immediately resume, but gives other tasks a chance
        }

        void await_resume() const noexcept {}
    };
    return YieldAwaiter{};
}

// Sleep for a duration (blocking - use with thread pool)
template<typename Rep, typename Period>
auto sleep_for(std::chrono::duration<Rep, Period> duration) {
    struct SleepAwaiter {
        std::chrono::duration<Rep, Period> duration;

        bool await_ready() const noexcept {
            return duration.count() <= 0;
        }

        void await_suspend(coroutine_handle<> handle) noexcept {
            std::thread([d = duration, h = handle]() mutable {
                std::this_thread::sleep_for(d);
                h.resume();
            }).detach();
        }

        void await_resume() const noexcept {}
    };
    return SleepAwaiter{duration};
}

// ============================================================================
// Global thread pool
// ============================================================================

namespace detail {
    inline ThreadPool& get_global_pool() {
        static ThreadPool pool;
        return pool;
    }
}

// Get the global thread pool
inline ThreadPool& global_pool() {
    return detail::get_global_pool();
}

// Schedule on the global thread pool
inline auto schedule() {
    return global_pool().schedule_on();
}

// ============================================================================
// Task execution on executors
// ============================================================================

// Run a task on an executor and wait for completion
template<typename T, typename Executor>
T run_on(Task<T> task, Executor& executor) {
    struct RunOnPromise {
        struct promise_type {
            std::variant<std::monostate, T, std::exception_ptr> result;
            std::mutex mutex;
            std::condition_variable cv;
            bool done{false};

            RunOnPromise get_return_object() {
                return RunOnPromise{coroutine_handle<promise_type>::from_promise(*this)};
            }

            suspend_never initial_suspend() noexcept { return {}; }

            auto final_suspend() noexcept {
                struct FinalAwaiter {
                    promise_type& promise;

                    bool await_ready() const noexcept { return false; }

                    void await_suspend(coroutine_handle<>) noexcept {
                        std::lock_guard<std::mutex> lock(promise.mutex);
                        promise.done = true;
                        promise.cv.notify_all();
                    }

                    void await_resume() const noexcept {}
                };
                return FinalAwaiter{*this};
            }

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
            std::unique_lock<std::mutex> lock(handle.promise().mutex);
            handle.promise().cv.wait(lock, [this] { return handle.promise().done; });

            if (std::holds_alternative<std::exception_ptr>(handle.promise().result)) {
                std::rethrow_exception(std::get<std::exception_ptr>(handle.promise().result));
            }
            return std::move(std::get<T>(handle.promise().result));
        }

        ~RunOnPromise() {
            if (handle) {
                handle.destroy();
            }
        }
    };

    auto wrapper = [](Task<T> t, Executor& exec) -> RunOnPromise {
        co_await exec.schedule_on();
        co_return co_await std::move(t);
    }(std::move(task), executor);

    return wrapper.get();
}

// Specialization for void tasks
template<typename Executor>
void run_on(Task<void> task, Executor& executor) {
    struct RunOnPromise {
        struct promise_type {
            std::optional<std::exception_ptr> exception;
            std::mutex mutex;
            std::condition_variable cv;
            bool done{false};

            RunOnPromise get_return_object() {
                return RunOnPromise{coroutine_handle<promise_type>::from_promise(*this)};
            }

            suspend_never initial_suspend() noexcept { return {}; }

            auto final_suspend() noexcept {
                struct FinalAwaiter {
                    promise_type& promise;

                    bool await_ready() const noexcept { return false; }

                    void await_suspend(coroutine_handle<>) noexcept {
                        std::lock_guard<std::mutex> lock(promise.mutex);
                        promise.done = true;
                        promise.cv.notify_all();
                    }

                    void await_resume() const noexcept {}
                };
                return FinalAwaiter{*this};
            }

            void unhandled_exception() {
                exception = std::current_exception();
            }

            void return_void() noexcept {}
        };

        coroutine_handle<promise_type> handle;

        void get() {
            std::unique_lock<std::mutex> lock(handle.promise().mutex);
            handle.promise().cv.wait(lock, [this] { return handle.promise().done; });

            if (handle.promise().exception) {
                std::rethrow_exception(*handle.promise().exception);
            }
        }

        ~RunOnPromise() {
            if (handle) {
                handle.destroy();
            }
        }
    };

    auto wrapper = [](Task<void> t, Executor& exec) -> RunOnPromise {
        co_await exec.schedule_on();
        co_await std::move(t);
    }(std::move(task), executor);

    wrapper.get();
}

} // namespace cupcpp

#endif // CUPCPP_EXECUTOR_HPP
