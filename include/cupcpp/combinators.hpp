// SPDX-License-Identifier: GPL-3.0-or-later
// Cupcpp - Modern C++ Coroutine Utilities Library
// https://github.com/es-ist-leon/Cupcpp

#ifndef CUPCPP_COMBINATORS_HPP
#define CUPCPP_COMBINATORS_HPP

#include "config.hpp"
#include "task.hpp"

#include <atomic>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <optional>

namespace cupcpp {

namespace detail {

// Helper to store results in a tuple, handling void tasks
template<typename T>
struct ResultStorage {
    using type = T;

    template<typename Task>
    static void store(Task& task, std::optional<T>& result) {
        result = task.handle().promise().result();
    }
};

template<>
struct ResultStorage<void> {
    using type = std::monostate;

    template<typename Task>
    static void store(Task& task, std::optional<std::monostate>& result) {
        task.handle().promise().result(); // Check for exception
        result = std::monostate{};
    }
};

// Counter for when_all
struct WhenAllCounter {
    std::atomic<std::size_t> remaining;
    coroutine_handle<> continuation{nullptr};
    std::mutex mutex;
    std::exception_ptr exception;

    explicit WhenAllCounter(std::size_t count) : remaining(count) {}

    void on_complete() {
        auto prev = remaining.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1 && continuation) {
            continuation.resume();
        }
    }

    void set_exception(std::exception_ptr e) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!exception) {
            exception = std::move(e);
        }
    }
};

// Counter for when_any
struct WhenAnyCounter {
    std::atomic<bool> completed{false};
    std::atomic<std::size_t> completed_index{0};
    coroutine_handle<> continuation{nullptr};
    std::mutex mutex;
    std::exception_ptr exception;

    bool try_complete(std::size_t index) {
        bool expected = false;
        if (completed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            completed_index.store(index, std::memory_order_release);
            if (continuation) {
                continuation.resume();
            }
            return true;
        }
        return false;
    }
};

} // namespace detail

// ============================================================================
// when_all - Wait for all tasks to complete
// ============================================================================

// when_all for variadic tasks
template<typename... Tasks>
class WhenAllAwaiter {
    using ResultTuple = std::tuple<
        std::optional<typename detail::ResultStorage<typename std::decay_t<Tasks>::value_type>::type>...
    >;

public:
    explicit WhenAllAwaiter(Tasks&&... tasks)
        : tasks_(std::forward<Tasks>(tasks)...)
        , counter_(sizeof...(Tasks)) {}

    bool await_ready() const noexcept {
        return sizeof...(Tasks) == 0;
    }

    void await_suspend(coroutine_handle<> cont) {
        counter_.continuation = cont;

        start_tasks(std::index_sequence_for<Tasks...>{});
    }

    auto await_resume() {
        if (counter_.exception) {
            std::rethrow_exception(counter_.exception);
        }
        return extract_results(std::index_sequence_for<Tasks...>{});
    }

private:
    template<std::size_t... Is>
    void start_tasks(std::index_sequence<Is...>) {
        (start_single_task<Is>(), ...);
    }

    template<std::size_t I>
    void start_single_task() {
        auto& task = std::get<I>(tasks_);
        if (!task.valid()) {
            counter_.on_complete();
            return;
        }

        // Create wrapper coroutine
        [](auto& t, auto& result, detail::WhenAllCounter& counter) -> Task<void> {
            try {
                using TaskType = std::decay_t<decltype(t)>;
                using ValueType = typename TaskType::value_type;
                detail::ResultStorage<ValueType>::store(t, result);
            } catch (...) {
                counter.set_exception(std::current_exception());
            }
            counter.on_complete();
            co_return;
        }(task, std::get<I>(results_), counter_);

        // Start the task
        if (!task.done()) {
            task.resume();
        }
    }

    template<std::size_t... Is>
    auto extract_results(std::index_sequence<Is...>) {
        return std::make_tuple(std::move(*std::get<Is>(results_))...);
    }

    std::tuple<Tasks...> tasks_;
    ResultTuple results_;
    detail::WhenAllCounter counter_;
};

// Free function for when_all with variadic tasks
template<typename... Tasks>
auto when_all(Tasks&&... tasks) {
    return WhenAllAwaiter<Tasks...>(std::forward<Tasks>(tasks)...);
}

// when_all for vector of tasks (same type)
template<typename T>
class WhenAllVectorAwaiter {
public:
    explicit WhenAllVectorAwaiter(std::vector<Task<T>> tasks)
        : tasks_(std::move(tasks))
        , results_(tasks_.size())
        , counter_(tasks_.size()) {}

    bool await_ready() const noexcept {
        return tasks_.empty();
    }

    void await_suspend(coroutine_handle<> cont) {
        counter_.continuation = cont;

        for (std::size_t i = 0; i < tasks_.size(); ++i) {
            start_task(i);
        }
    }

    std::vector<T> await_resume() {
        if (counter_.exception) {
            std::rethrow_exception(counter_.exception);
        }

        std::vector<T> result;
        result.reserve(results_.size());
        for (auto& opt : results_) {
            result.push_back(std::move(*opt));
        }
        return result;
    }

private:
    void start_task(std::size_t index) {
        auto& task = tasks_[index];
        if (!task.valid()) {
            counter_.on_complete();
            return;
        }

        // Simple approach: resume and store result
        [](Task<T>& t, std::optional<T>& result, detail::WhenAllCounter& counter) -> Task<void> {
            try {
                result = co_await std::move(t);
            } catch (...) {
                counter.set_exception(std::current_exception());
            }
            counter.on_complete();
        }(task, results_[index], counter_);
    }

    std::vector<Task<T>> tasks_;
    std::vector<std::optional<T>> results_;
    detail::WhenAllCounter counter_;
};

// Specialization for void tasks
template<>
class WhenAllVectorAwaiter<void> {
public:
    explicit WhenAllVectorAwaiter(std::vector<Task<void>> tasks)
        : tasks_(std::move(tasks))
        , counter_(tasks_.size()) {}

    bool await_ready() const noexcept {
        return tasks_.empty();
    }

    void await_suspend(coroutine_handle<> cont) {
        counter_.continuation = cont;

        for (std::size_t i = 0; i < tasks_.size(); ++i) {
            start_task(i);
        }
    }

    void await_resume() {
        if (counter_.exception) {
            std::rethrow_exception(counter_.exception);
        }
    }

private:
    void start_task(std::size_t index) {
        auto& task = tasks_[index];
        if (!task.valid()) {
            counter_.on_complete();
            return;
        }

        [](Task<void>& t, detail::WhenAllCounter& counter) -> Task<void> {
            try {
                co_await std::move(t);
            } catch (...) {
                counter.set_exception(std::current_exception());
            }
            counter.on_complete();
        }(task, counter_);
    }

    std::vector<Task<void>> tasks_;
    detail::WhenAllCounter counter_;
};

// Free function for when_all with vector of tasks
template<typename T>
auto when_all(std::vector<Task<T>> tasks) {
    return WhenAllVectorAwaiter<T>(std::move(tasks));
}

// ============================================================================
// when_any - Wait for any task to complete
// ============================================================================

// Result type for when_any
template<typename T>
struct WhenAnyResult {
    std::size_t index;
    T value;
};

template<>
struct WhenAnyResult<void> {
    std::size_t index;
};

// when_any for vector of tasks (same type)
template<typename T>
class WhenAnyVectorAwaiter {
public:
    explicit WhenAnyVectorAwaiter(std::vector<Task<T>> tasks)
        : tasks_(std::move(tasks)) {}

    bool await_ready() const noexcept {
        return tasks_.empty();
    }

    void await_suspend(coroutine_handle<> cont) {
        counter_.continuation = cont;

        for (std::size_t i = 0; i < tasks_.size(); ++i) {
            start_task(i);
        }
    }

    WhenAnyResult<T> await_resume() {
        if (counter_.exception) {
            std::rethrow_exception(counter_.exception);
        }

        auto idx = counter_.completed_index.load(std::memory_order_acquire);
        return WhenAnyResult<T>{idx, std::move(*result_)};
    }

private:
    void start_task(std::size_t index) {
        auto& task = tasks_[index];
        if (!task.valid()) {
            return;
        }

        [](Task<T>& t, std::size_t idx, std::optional<T>& result,
           detail::WhenAnyCounter& counter) -> Task<void> {
            try {
                auto value = co_await std::move(t);
                if (counter.try_complete(idx)) {
                    result = std::move(value);
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(counter.mutex);
                if (!counter.exception) {
                    counter.exception = std::current_exception();
                }
            }
        }(task, index, result_, counter_);
    }

    std::vector<Task<T>> tasks_;
    std::optional<T> result_;
    detail::WhenAnyCounter counter_;
};

// Specialization for void tasks
template<>
class WhenAnyVectorAwaiter<void> {
public:
    explicit WhenAnyVectorAwaiter(std::vector<Task<void>> tasks)
        : tasks_(std::move(tasks)) {}

    bool await_ready() const noexcept {
        return tasks_.empty();
    }

    void await_suspend(coroutine_handle<> cont) {
        counter_.continuation = cont;

        for (std::size_t i = 0; i < tasks_.size(); ++i) {
            start_task(i);
        }
    }

    WhenAnyResult<void> await_resume() {
        if (counter_.exception) {
            std::rethrow_exception(counter_.exception);
        }

        auto idx = counter_.completed_index.load(std::memory_order_acquire);
        return WhenAnyResult<void>{idx};
    }

private:
    void start_task(std::size_t index) {
        auto& task = tasks_[index];
        if (!task.valid()) {
            return;
        }

        [](Task<void>& t, std::size_t idx, detail::WhenAnyCounter& counter) -> Task<void> {
            try {
                co_await std::move(t);
                counter.try_complete(idx);
            } catch (...) {
                std::lock_guard<std::mutex> lock(counter.mutex);
                if (!counter.exception) {
                    counter.exception = std::current_exception();
                }
            }
        }(task, index, counter_);
    }

    std::vector<Task<void>> tasks_;
    detail::WhenAnyCounter counter_;
};

// Free function for when_any with vector of tasks
template<typename T>
auto when_any(std::vector<Task<T>> tasks) {
    return WhenAnyVectorAwaiter<T>(std::move(tasks));
}

// ============================================================================
// Utility combinators
// ============================================================================

// Transform the result of a task
template<typename T, typename Func>
auto then(Task<T> task, Func&& func) -> Task<std::invoke_result_t<Func, T>> {
    auto result = co_await std::move(task);
    co_return std::forward<Func>(func)(std::move(result));
}

// Specialization for void tasks
template<typename Func>
auto then(Task<void> task, Func&& func) -> Task<std::invoke_result_t<Func>> {
    co_await std::move(task);
    co_return std::forward<Func>(func)();
}

// Handle exceptions from a task
template<typename T, typename Handler>
Task<T> catch_exception(Task<T> task, Handler&& handler) {
    try {
        co_return co_await std::move(task);
    } catch (...) {
        co_return std::forward<Handler>(handler)(std::current_exception());
    }
}

// Run a task with a timeout (requires thread pool for the timer)
template<typename T, typename Rep, typename Period>
Task<std::optional<T>> with_timeout(Task<T> task, std::chrono::duration<Rep, Period> timeout) {
    // Simple implementation using atomic flag
    auto completed = std::make_shared<std::atomic<bool>>(false);
    auto result = std::make_shared<std::optional<T>>();

    // Start the task
    auto task_wrapper = [](Task<T> t, std::shared_ptr<std::atomic<bool>> done,
                          std::shared_ptr<std::optional<T>> res) -> Task<void> {
        try {
            auto value = co_await std::move(t);
            if (!done->exchange(true, std::memory_order_acq_rel)) {
                *res = std::move(value);
            }
        } catch (...) {
            done->store(true, std::memory_order_release);
            throw;
        }
    }(std::move(task), completed, result);

    // Start timer thread
    std::thread timer([timeout, completed]() {
        std::this_thread::sleep_for(timeout);
        completed->store(true, std::memory_order_release);
    });
    timer.detach();

    // Wait for task (this is simplified - real impl would need proper cancellation)
    co_await std::move(task_wrapper);

    co_return std::move(*result);
}

// Retry a task on failure
template<typename TaskFactory, typename = std::enable_if_t<
    std::is_invocable_v<TaskFactory>>>
auto retry(TaskFactory&& factory, std::size_t max_attempts,
           std::chrono::milliseconds delay = std::chrono::milliseconds{0})
    -> Task<typename std::invoke_result_t<TaskFactory>::value_type>
{
    using ResultType = typename std::invoke_result_t<TaskFactory>::value_type;

    std::exception_ptr last_exception;

    for (std::size_t attempt = 0; attempt < max_attempts; ++attempt) {
        try {
            co_return co_await factory();
        } catch (...) {
            last_exception = std::current_exception();

            if (attempt + 1 < max_attempts && delay.count() > 0) {
                std::this_thread::sleep_for(delay);
            }
        }
    }

    if (last_exception) {
        std::rethrow_exception(last_exception);
    }

    // Should never reach here
    throw std::runtime_error("retry: all attempts failed");
}

// Execute tasks sequentially and collect results
template<typename T>
Task<std::vector<T>> sequence(std::vector<Task<T>> tasks) {
    std::vector<T> results;
    results.reserve(tasks.size());

    for (auto& task : tasks) {
        results.push_back(co_await std::move(task));
    }

    co_return results;
}

// Execute tasks sequentially (void tasks)
inline Task<void> sequence(std::vector<Task<void>> tasks) {
    for (auto& task : tasks) {
        co_await std::move(task);
    }
}

// Race: return the first successful result (skip failures)
template<typename T>
Task<std::optional<T>> race(std::vector<Task<T>> tasks) {
    for (auto& task : tasks) {
        try {
            co_return co_await std::move(task);
        } catch (...) {
            // Continue to next task
        }
    }
    co_return std::nullopt;
}

} // namespace cupcpp

#endif // CUPCPP_COMBINATORS_HPP
