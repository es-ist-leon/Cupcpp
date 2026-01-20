// SPDX-License-Identifier: GPL-3.0-or-later
// Cupcpp - Modern C++ Coroutine Utilities Library
// https://github.com/es-ist-leon/Cupcpp

#ifndef CUPCPP_GENERATOR_HPP
#define CUPCPP_GENERATOR_HPP

#include "config.hpp"

#include <exception>
#include <iterator>
#include <type_traits>
#include <utility>
#include <memory>

namespace cupcpp {

// Forward declarations
template<typename T>
class Generator;

template<typename T>
class GeneratorPromise;

// Promise type for Generator
template<typename T>
class GeneratorPromise {
public:
    using value_type = std::remove_reference_t<T>;
    using reference = std::conditional_t<std::is_reference_v<T>, T, T&>;
    using pointer = value_type*;
    using handle_type = coroutine_handle<GeneratorPromise>;

    GeneratorPromise() = default;

    Generator<T> get_return_object() noexcept;

    // Start suspended - lazy evaluation
    suspend_always initial_suspend() noexcept { return {}; }

    // Suspend at the end to allow final iteration check
    suspend_always final_suspend() noexcept { return {}; }

    // Yielding values
    suspend_always yield_value(value_type& value) noexcept {
        current_value_ = std::addressof(value);
        return {};
    }

    suspend_always yield_value(value_type&& value) noexcept {
        current_value_ = std::addressof(value);
        return {};
    }

    void unhandled_exception() {
        exception_ = std::current_exception();
    }

    void return_void() noexcept {}

    reference value() const noexcept {
        return *current_value_;
    }

    void rethrow_if_exception() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    template<typename U>
    suspend_never await_transform(U&&) = delete; // Generators cannot co_await

private:
    pointer current_value_{nullptr};
    std::exception_ptr exception_{nullptr};
};

// Iterator for Generator
template<typename T>
class GeneratorIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = typename GeneratorPromise<T>::value_type;
    using reference = typename GeneratorPromise<T>::reference;
    using pointer = typename GeneratorPromise<T>::pointer;

    GeneratorIterator() noexcept = default;

    explicit GeneratorIterator(coroutine_handle<GeneratorPromise<T>> handle) noexcept
        : handle_(handle) {}

    reference operator*() const noexcept {
        return handle_.promise().value();
    }

    pointer operator->() const noexcept {
        return std::addressof(operator*());
    }

    GeneratorIterator& operator++() {
        CUPCPP_ASSERT(handle_);
        CUPCPP_ASSERT(!handle_.done());
        handle_.resume();
        if (handle_.done()) {
            handle_.promise().rethrow_if_exception();
        }
        return *this;
    }

    GeneratorIterator operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    bool operator==(const GeneratorIterator& other) const noexcept {
        // Both are end iterators if handle is done or null
        bool this_is_end = !handle_ || handle_.done();
        bool other_is_end = !other.handle_ || other.handle_.done();
        return this_is_end && other_is_end;
    }

    bool operator!=(const GeneratorIterator& other) const noexcept {
        return !(*this == other);
    }

private:
    coroutine_handle<GeneratorPromise<T>> handle_{nullptr};
};

// Sentinel type for Generator (more efficient than comparing iterators)
struct GeneratorSentinel {};

template<typename T>
bool operator==(const GeneratorIterator<T>& it, GeneratorSentinel) noexcept {
    return !it.handle_ || it.handle_.done();
}

template<typename T>
bool operator==(GeneratorSentinel s, const GeneratorIterator<T>& it) noexcept {
    return it == s;
}

template<typename T>
bool operator!=(const GeneratorIterator<T>& it, GeneratorSentinel s) noexcept {
    return !(it == s);
}

template<typename T>
bool operator!=(GeneratorSentinel s, const GeneratorIterator<T>& it) noexcept {
    return !(it == s);
}

// Main Generator class - a synchronous coroutine that yields values lazily
template<typename T>
class Generator {
public:
    using promise_type = GeneratorPromise<T>;
    using handle_type = coroutine_handle<promise_type>;
    using iterator = GeneratorIterator<T>;
    using sentinel = GeneratorSentinel;
    using value_type = typename promise_type::value_type;
    using reference = typename promise_type::reference;

    // Constructors
    Generator() noexcept = default;

    explicit Generator(handle_type h) noexcept : handle_(h) {}

    // Move-only semantics
    Generator(Generator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    ~Generator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Check if generator is valid
    CUPCPP_NODISCARD bool valid() const noexcept {
        return handle_ != nullptr;
    }

    CUPCPP_NODISCARD explicit operator bool() const noexcept {
        return valid();
    }

    // Check if generator has completed
    CUPCPP_NODISCARD bool done() const noexcept {
        return !handle_ || handle_.done();
    }

    // Range interface
    iterator begin() {
        if (handle_) {
            handle_.resume();
            if (handle_.done()) {
                handle_.promise().rethrow_if_exception();
            }
        }
        return iterator{handle_};
    }

    sentinel end() noexcept {
        return sentinel{};
    }

    // Manual iteration interface
    bool next() {
        CUPCPP_ASSERT(handle_);
        handle_.resume();
        if (handle_.done()) {
            handle_.promise().rethrow_if_exception();
            return false;
        }
        return true;
    }

    reference value() const {
        CUPCPP_ASSERT(handle_);
        return handle_.promise().value();
    }

    // Get the underlying handle
    handle_type handle() const noexcept {
        return handle_;
    }

private:
    handle_type handle_{nullptr};
};

// Implement get_return_object after Generator is defined
template<typename T>
Generator<T> GeneratorPromise<T>::get_return_object() noexcept {
    return Generator<T>{handle_type::from_promise(*this)};
}

// Recursive Generator for yielding from nested generators (co_yield elements of another generator)
template<typename T>
class RecursiveGenerator;

template<typename T>
class RecursiveGeneratorPromise {
public:
    using value_type = std::remove_reference_t<T>;
    using reference = std::conditional_t<std::is_reference_v<T>, T, T&>;
    using pointer = value_type*;
    using handle_type = coroutine_handle<RecursiveGeneratorPromise>;

    RecursiveGeneratorPromise() = default;

    RecursiveGenerator<T> get_return_object() noexcept;

    suspend_always initial_suspend() noexcept { return {}; }
    suspend_always final_suspend() noexcept { return {}; }

    suspend_always yield_value(value_type& value) noexcept {
        current_value_ = std::addressof(value);
        return {};
    }

    suspend_always yield_value(value_type&& value) noexcept {
        current_value_ = std::addressof(value);
        return {};
    }

    // Yield all elements from another generator
    auto yield_value(RecursiveGenerator<T>&& gen) noexcept {
        struct YieldAllAwaiter {
            RecursiveGenerator<T> nested;
            std::exception_ptr exception;

            bool await_ready() noexcept {
                return !nested.handle_;
            }

            coroutine_handle<> await_suspend(handle_type h) noexcept {
                auto& promise = h.promise();
                promise.root_ = (promise.root_ != nullptr) ? promise.root_ : h;
                nested.handle_.promise().parent_ = h;
                nested.handle_.promise().root_ = promise.root_;
                return nested.handle_;
            }

            void await_resume() {
                if (exception) {
                    std::rethrow_exception(exception);
                }
            }
        };
        return YieldAllAwaiter{std::move(gen)};
    }

    void unhandled_exception() {
        exception_ = std::current_exception();
    }

    void return_void() noexcept {}

    reference value() const noexcept {
        return *current_value_;
    }

    void rethrow_if_exception() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    handle_type parent_{nullptr};
    handle_type root_{nullptr};

private:
    pointer current_value_{nullptr};
    std::exception_ptr exception_{nullptr};
};

// Iterator for RecursiveGenerator
template<typename T>
class RecursiveGeneratorIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = typename RecursiveGeneratorPromise<T>::value_type;
    using reference = typename RecursiveGeneratorPromise<T>::reference;
    using pointer = typename RecursiveGeneratorPromise<T>::pointer;

    RecursiveGeneratorIterator() noexcept = default;

    explicit RecursiveGeneratorIterator(coroutine_handle<RecursiveGeneratorPromise<T>> handle) noexcept
        : handle_(handle) {}

    reference operator*() const noexcept {
        return handle_.promise().value();
    }

    pointer operator->() const noexcept {
        return std::addressof(operator*());
    }

    RecursiveGeneratorIterator& operator++() {
        CUPCPP_ASSERT(handle_);
        handle_.resume();
        // Navigate back up the tree when nested generators complete
        while (handle_.done()) {
            handle_.promise().rethrow_if_exception();
            auto parent = handle_.promise().parent_;
            if (!parent) {
                break;
            }
            handle_ = parent;
            handle_.resume();
        }
        return *this;
    }

    bool operator==(const RecursiveGeneratorIterator& other) const noexcept {
        bool this_is_end = !handle_ || handle_.done();
        bool other_is_end = !other.handle_ || other.handle_.done();
        return this_is_end && other_is_end;
    }

    bool operator!=(const RecursiveGeneratorIterator& other) const noexcept {
        return !(*this == other);
    }

private:
    coroutine_handle<RecursiveGeneratorPromise<T>> handle_{nullptr};

    friend class RecursiveGenerator<T>;
};

// Recursive Generator - supports co_yield of nested generators
template<typename T>
class RecursiveGenerator {
public:
    using promise_type = RecursiveGeneratorPromise<T>;
    using handle_type = coroutine_handle<promise_type>;
    using iterator = RecursiveGeneratorIterator<T>;
    using value_type = typename promise_type::value_type;
    using reference = typename promise_type::reference;

    RecursiveGenerator() noexcept = default;

    explicit RecursiveGenerator(handle_type h) noexcept : handle_(h) {}

    RecursiveGenerator(RecursiveGenerator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    RecursiveGenerator& operator=(RecursiveGenerator&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    RecursiveGenerator(const RecursiveGenerator&) = delete;
    RecursiveGenerator& operator=(const RecursiveGenerator&) = delete;

    ~RecursiveGenerator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    CUPCPP_NODISCARD bool valid() const noexcept {
        return handle_ != nullptr;
    }

    CUPCPP_NODISCARD explicit operator bool() const noexcept {
        return valid();
    }

    iterator begin() {
        if (handle_) {
            handle_.resume();
        }
        return iterator{handle_};
    }

    GeneratorSentinel end() noexcept {
        return GeneratorSentinel{};
    }

    handle_type handle() const noexcept {
        return handle_;
    }

private:
    handle_type handle_{nullptr};

    friend class RecursiveGeneratorPromise<T>;
};

template<typename T>
RecursiveGenerator<T> RecursiveGeneratorPromise<T>::get_return_object() noexcept {
    return RecursiveGenerator<T>{handle_type::from_promise(*this)};
}

template<typename T>
bool operator==(const RecursiveGeneratorIterator<T>& it, GeneratorSentinel) noexcept {
    return !it.handle_ || it.handle_.done();
}

template<typename T>
bool operator!=(const RecursiveGeneratorIterator<T>& it, GeneratorSentinel s) noexcept {
    return !(it == s);
}

// Utility functions

// Create a generator from a range
template<typename Range>
auto from_range(Range&& range) -> Generator<typename std::decay_t<Range>::value_type> {
    for (auto&& elem : std::forward<Range>(range)) {
        co_yield elem;
    }
}

// Create a generator that yields n copies of a value
template<typename T>
Generator<T> repeat(T value, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        co_yield value;
    }
}

// Create an infinite generator that yields the same value
template<typename T>
Generator<T> repeat_forever(T value) {
    while (true) {
        co_yield value;
    }
}

// Create a generator that yields integers in a range
inline Generator<int> iota(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

inline Generator<int> iota(int end) {
    return iota(0, end);
}

// Take first n elements from a generator
template<typename T>
Generator<T> take(Generator<T> gen, std::size_t n) {
    std::size_t count = 0;
    for (auto&& value : gen) {
        if (count++ >= n) break;
        co_yield value;
    }
}

// Filter elements from a generator
template<typename T, typename Pred>
Generator<T> filter(Generator<T> gen, Pred pred) {
    for (auto&& value : gen) {
        if (pred(value)) {
            co_yield value;
        }
    }
}

// Transform elements from a generator
template<typename T, typename Func>
auto transform(Generator<T> gen, Func func) -> Generator<decltype(func(std::declval<T>()))> {
    for (auto&& value : gen) {
        co_yield func(value);
    }
}

} // namespace cupcpp

#endif // CUPCPP_GENERATOR_HPP
