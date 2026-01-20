// SPDX-License-Identifier: GPL-3.0-or-later
// Cupcpp - Modern C++ Coroutine Utilities Library
// https://github.com/es-ist-leon/Cupcpp

#ifndef CUPCPP_SYNC_HPP
#define CUPCPP_SYNC_HPP

#include "config.hpp"
#include "task.hpp"

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>
#include <chrono>
#include <memory>
#include <limits>

namespace cupcpp {

// ============================================================================
// AsyncMutex - A mutex that suspends coroutines instead of blocking threads
// ============================================================================

class AsyncMutex {
public:
    class ScopedLock;

    AsyncMutex() = default;
    ~AsyncMutex() = default;

    AsyncMutex(const AsyncMutex&) = delete;
    AsyncMutex& operator=(const AsyncMutex&) = delete;

    // Awaiter returned by lock()
    class LockAwaiter {
    public:
        explicit LockAwaiter(AsyncMutex& mutex) noexcept : mutex_(mutex) {}

        bool await_ready() const noexcept {
            return false;
        }

        bool await_suspend(coroutine_handle<> handle) noexcept {
            waiter_.handle = handle;
            std::lock_guard<std::mutex> lock(mutex_.internal_mutex_);

            if (!mutex_.locked_) {
                mutex_.locked_ = true;
                return false; // Don't suspend, acquired lock
            }

            // Add to wait queue
            waiter_.next = nullptr;
            if (mutex_.tail_) {
                mutex_.tail_->next = &waiter_;
            } else {
                mutex_.head_ = &waiter_;
            }
            mutex_.tail_ = &waiter_;
            return true; // Suspend
        }

        void await_resume() const noexcept {}

    private:
        friend class AsyncMutex;

        struct Waiter {
            coroutine_handle<> handle;
            Waiter* next{nullptr};
        };

        AsyncMutex& mutex_;
        Waiter waiter_;
    };

    // Acquire the lock
    CUPCPP_NODISCARD LockAwaiter lock() noexcept {
        return LockAwaiter{*this};
    }

    // Release the lock
    void unlock() {
        coroutine_handle<> to_resume{nullptr};

        {
            std::lock_guard<std::mutex> lock(internal_mutex_);
            CUPCPP_ASSERT(locked_);

            if (head_) {
                // Wake up next waiter
                auto* waiter = head_;
                head_ = waiter->next;
                if (!head_) {
                    tail_ = nullptr;
                }
                to_resume = waiter->handle;
            } else {
                locked_ = false;
            }
        }

        if (to_resume) {
            to_resume.resume();
        }
    }

    // Try to acquire without blocking
    CUPCPP_NODISCARD bool try_lock() noexcept {
        std::lock_guard<std::mutex> lock(internal_mutex_);
        if (!locked_) {
            locked_ = true;
            return true;
        }
        return false;
    }

    // RAII lock guard
    class ScopedLock {
    public:
        explicit ScopedLock(AsyncMutex& mutex) noexcept : mutex_(&mutex) {}

        ScopedLock(ScopedLock&& other) noexcept : mutex_(other.mutex_) {
            other.mutex_ = nullptr;
        }

        ScopedLock& operator=(ScopedLock&& other) noexcept {
            if (this != &other) {
                if (mutex_) {
                    mutex_->unlock();
                }
                mutex_ = other.mutex_;
                other.mutex_ = nullptr;
            }
            return *this;
        }

        ScopedLock(const ScopedLock&) = delete;
        ScopedLock& operator=(const ScopedLock&) = delete;

        ~ScopedLock() {
            if (mutex_) {
                mutex_->unlock();
            }
        }

    private:
        AsyncMutex* mutex_;
    };

    // Acquire lock and return RAII guard
    CUPCPP_NODISCARD auto scoped_lock() {
        struct ScopedLockAwaiter {
            AsyncMutex& mutex;

            bool await_ready() const noexcept { return false; }

            bool await_suspend(coroutine_handle<> handle) noexcept {
                return LockAwaiter{mutex}.await_suspend(handle);
            }

            ScopedLock await_resume() noexcept {
                return ScopedLock{mutex};
            }
        };
        return ScopedLockAwaiter{*this};
    }

private:
    std::mutex internal_mutex_;
    bool locked_{false};
    LockAwaiter::Waiter* head_{nullptr};
    LockAwaiter::Waiter* tail_{nullptr};
};

// ============================================================================
// AsyncSemaphore - Counting semaphore that suspends coroutines
// ============================================================================

class AsyncSemaphore {
public:
    explicit AsyncSemaphore(std::size_t initial_count = 0) noexcept
        : count_(initial_count) {}

    ~AsyncSemaphore() = default;

    AsyncSemaphore(const AsyncSemaphore&) = delete;
    AsyncSemaphore& operator=(const AsyncSemaphore&) = delete;

    // Awaiter for acquire()
    class AcquireAwaiter {
    public:
        explicit AcquireAwaiter(AsyncSemaphore& sem) noexcept : sem_(sem) {}

        bool await_ready() const noexcept {
            return false;
        }

        bool await_suspend(coroutine_handle<> handle) noexcept {
            waiter_.handle = handle;
            std::lock_guard<std::mutex> lock(sem_.mutex_);

            if (sem_.count_ > 0) {
                --sem_.count_;
                return false; // Don't suspend
            }

            // Add to wait queue
            waiter_.next = nullptr;
            if (sem_.tail_) {
                sem_.tail_->next = &waiter_;
            } else {
                sem_.head_ = &waiter_;
            }
            sem_.tail_ = &waiter_;
            return true; // Suspend
        }

        void await_resume() const noexcept {}

    private:
        friend class AsyncSemaphore;

        struct Waiter {
            coroutine_handle<> handle;
            Waiter* next{nullptr};
        };

        AsyncSemaphore& sem_;
        Waiter waiter_;
    };

    // Acquire (decrement) the semaphore
    CUPCPP_NODISCARD AcquireAwaiter acquire() noexcept {
        return AcquireAwaiter{*this};
    }

    // Release (increment) the semaphore
    void release(std::size_t count = 1) {
        std::vector<coroutine_handle<>> to_resume;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            while (count > 0 && head_) {
                auto* waiter = head_;
                head_ = waiter->next;
                if (!head_) {
                    tail_ = nullptr;
                }
                to_resume.push_back(waiter->handle);
                --count;
            }

            count_ += count; // Add remaining to count
        }

        for (auto handle : to_resume) {
            handle.resume();
        }
    }

    // Try to acquire without blocking
    CUPCPP_NODISCARD bool try_acquire() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ > 0) {
            --count_;
            return true;
        }
        return false;
    }

    // Get current count
    CUPCPP_NODISCARD std::size_t available() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    mutable std::mutex mutex_;
    std::size_t count_;
    AcquireAwaiter::Waiter* head_{nullptr};
    AcquireAwaiter::Waiter* tail_{nullptr};
};

// ============================================================================
// AsyncEvent - One-shot event that can wake multiple waiters
// ============================================================================

class AsyncEvent {
public:
    AsyncEvent() = default;
    ~AsyncEvent() = default;

    AsyncEvent(const AsyncEvent&) = delete;
    AsyncEvent& operator=(const AsyncEvent&) = delete;

    // Awaiter for wait()
    class WaitAwaiter {
    public:
        explicit WaitAwaiter(AsyncEvent& event) noexcept : event_(event) {}

        bool await_ready() const noexcept {
            return event_.signaled_.load(std::memory_order_acquire);
        }

        bool await_suspend(coroutine_handle<> handle) noexcept {
            waiter_.handle = handle;
            std::lock_guard<std::mutex> lock(event_.mutex_);

            if (event_.signaled_.load(std::memory_order_relaxed)) {
                return false; // Already signaled
            }

            // Add to wait list
            waiter_.next = event_.head_;
            event_.head_ = &waiter_;
            return true;
        }

        void await_resume() const noexcept {}

    private:
        friend class AsyncEvent;

        struct Waiter {
            coroutine_handle<> handle;
            Waiter* next{nullptr};
        };

        AsyncEvent& event_;
        Waiter waiter_;
    };

    // Wait for the event to be signaled
    CUPCPP_NODISCARD WaitAwaiter wait() noexcept {
        return WaitAwaiter{*this};
    }

    // Signal the event, waking all waiters
    void set() {
        std::vector<coroutine_handle<>> to_resume;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            signaled_.store(true, std::memory_order_release);

            auto* waiter = head_;
            head_ = nullptr;

            while (waiter) {
                to_resume.push_back(waiter->handle);
                waiter = waiter->next;
            }
        }

        for (auto handle : to_resume) {
            handle.resume();
        }
    }

    // Reset the event
    void reset() noexcept {
        signaled_.store(false, std::memory_order_release);
    }

    // Check if signaled
    CUPCPP_NODISCARD bool is_set() const noexcept {
        return signaled_.load(std::memory_order_acquire);
    }

private:
    std::mutex mutex_;
    std::atomic<bool> signaled_{false};
    WaitAwaiter::Waiter* head_{nullptr};
};

// ============================================================================
// Channel<T> - Async channel for communication between coroutines
// ============================================================================

template<typename T>
class Channel {
public:
    explicit Channel(std::size_t capacity = 0) noexcept
        : capacity_(capacity), closed_(false) {}

    ~Channel() = default;

    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    // Sender part for sending values
    class SendAwaiter {
    public:
        SendAwaiter(Channel& chan, T value)
            : chan_(chan), value_(std::move(value)) {}

        bool await_ready() const noexcept {
            return false;
        }

        bool await_suspend(coroutine_handle<> handle) noexcept {
            std::lock_guard<std::mutex> lock(chan_.mutex_);

            if (chan_.closed_) {
                failed_ = true;
                return false;
            }

            // If there's a waiting receiver, transfer directly
            if (chan_.receiver_head_) {
                auto* receiver = chan_.receiver_head_;
                chan_.receiver_head_ = receiver->next;
                if (!chan_.receiver_head_) {
                    chan_.receiver_tail_ = nullptr;
                }

                *receiver->value_ptr = std::move(value_);
                receiver->success = true;

                // Resume receiver
                auto recv_handle = receiver->handle;
                lock.~lock_guard();
                recv_handle.resume();
                return false;
            }

            // If unbuffered or buffer full, wait
            if (chan_.capacity_ == 0 || chan_.buffer_.size() >= chan_.capacity_) {
                waiter_.handle = handle;
                waiter_.value = std::move(value_);
                waiter_.next = nullptr;

                if (chan_.sender_tail_) {
                    chan_.sender_tail_->next = &waiter_;
                } else {
                    chan_.sender_head_ = &waiter_;
                }
                chan_.sender_tail_ = &waiter_;
                return true;
            }

            // Buffer has space
            chan_.buffer_.push(std::move(value_));
            return false;
        }

        bool await_resume() const noexcept {
            return !failed_;
        }

    private:
        friend class Channel;

        struct Waiter {
            coroutine_handle<> handle;
            T value;
            Waiter* next{nullptr};
        };

        Channel& chan_;
        T value_;
        Waiter waiter_;
        bool failed_{false};
    };

    // Receiver part for receiving values
    class ReceiveAwaiter {
    public:
        explicit ReceiveAwaiter(Channel& chan) noexcept : chan_(chan) {}

        bool await_ready() const noexcept {
            return false;
        }

        bool await_suspend(coroutine_handle<> handle) noexcept {
            std::lock_guard<std::mutex> lock(chan_.mutex_);

            // Check buffer first
            if (!chan_.buffer_.empty()) {
                value_ = std::move(chan_.buffer_.front());
                chan_.buffer_.pop();
                success_ = true;

                // If there's a waiting sender, let them add to buffer
                if (chan_.sender_head_) {
                    auto* sender = chan_.sender_head_;
                    chan_.sender_head_ = sender->next;
                    if (!chan_.sender_head_) {
                        chan_.sender_tail_ = nullptr;
                    }

                    chan_.buffer_.push(std::move(sender->value));

                    auto send_handle = sender->handle;
                    lock.~lock_guard();
                    send_handle.resume();
                }

                return false;
            }

            // Check for waiting senders (unbuffered case)
            if (chan_.sender_head_) {
                auto* sender = chan_.sender_head_;
                chan_.sender_head_ = sender->next;
                if (!chan_.sender_head_) {
                    chan_.sender_tail_ = nullptr;
                }

                value_ = std::move(sender->value);
                success_ = true;

                auto send_handle = sender->handle;
                lock.~lock_guard();
                send_handle.resume();
                return false;
            }

            // Check if closed
            if (chan_.closed_) {
                success_ = false;
                return false;
            }

            // Wait for a sender
            waiter_.handle = handle;
            waiter_.value_ptr = &value_;
            waiter_.success = false;
            waiter_.next = nullptr;

            if (chan_.receiver_tail_) {
                chan_.receiver_tail_->next = &waiter_;
            } else {
                chan_.receiver_head_ = &waiter_;
            }
            chan_.receiver_tail_ = &waiter_;
            return true;
        }

        std::optional<T> await_resume() {
            if (success_ || waiter_.success) {
                return std::move(value_);
            }
            return std::nullopt;
        }

    private:
        friend class Channel;

        struct Waiter {
            coroutine_handle<> handle;
            T* value_ptr{nullptr};
            bool success{false};
            Waiter* next{nullptr};
        };

        Channel& chan_;
        T value_;
        bool success_{false};
        Waiter waiter_;
    };

    // Send a value through the channel
    CUPCPP_NODISCARD SendAwaiter send(T value) {
        return SendAwaiter{*this, std::move(value)};
    }

    // Receive a value from the channel
    CUPCPP_NODISCARD ReceiveAwaiter receive() noexcept {
        return ReceiveAwaiter{*this};
    }

    // Try to send without blocking
    bool try_send(T value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (closed_) {
            return false;
        }

        // If there's a waiting receiver
        if (receiver_head_) {
            auto* receiver = receiver_head_;
            receiver_head_ = receiver->next;
            if (!receiver_head_) {
                receiver_tail_ = nullptr;
            }

            *receiver->value_ptr = std::move(value);
            receiver->success = true;
            receiver->handle.resume();
            return true;
        }

        // Try to buffer
        if (capacity_ > 0 && buffer_.size() < capacity_) {
            buffer_.push(std::move(value));
            return true;
        }

        return false;
    }

    // Try to receive without blocking
    std::optional<T> try_receive() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check buffer
        if (!buffer_.empty()) {
            T value = std::move(buffer_.front());
            buffer_.pop();

            // If there's a waiting sender
            if (sender_head_) {
                auto* sender = sender_head_;
                sender_head_ = sender->next;
                if (!sender_head_) {
                    sender_tail_ = nullptr;
                }

                buffer_.push(std::move(sender->value));
                sender->handle.resume();
            }

            return value;
        }

        // Check for waiting senders
        if (sender_head_) {
            auto* sender = sender_head_;
            sender_head_ = sender->next;
            if (!sender_head_) {
                sender_tail_ = nullptr;
            }

            T value = std::move(sender->value);
            sender->handle.resume();
            return value;
        }

        return std::nullopt;
    }

    // Close the channel
    void close() {
        std::vector<coroutine_handle<>> to_resume;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;

            // Wake all waiting receivers
            auto* receiver = receiver_head_;
            while (receiver) {
                receiver->success = false;
                to_resume.push_back(receiver->handle);
                receiver = receiver->next;
            }
            receiver_head_ = receiver_tail_ = nullptr;

            // Wake all waiting senders
            auto* sender = sender_head_;
            while (sender) {
                to_resume.push_back(sender->handle);
                sender = sender->next;
            }
            sender_head_ = sender_tail_ = nullptr;
        }

        for (auto handle : to_resume) {
            handle.resume();
        }
    }

    // Check if closed
    CUPCPP_NODISCARD bool is_closed() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    // Check if empty (no buffered values)
    CUPCPP_NODISCARD bool empty() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.empty();
    }

    // Get number of buffered values
    CUPCPP_NODISCARD std::size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> buffer_;
    std::size_t capacity_;
    bool closed_;

    typename SendAwaiter::Waiter* sender_head_{nullptr};
    typename SendAwaiter::Waiter* sender_tail_{nullptr};
    typename ReceiveAwaiter::Waiter* receiver_head_{nullptr};
    typename ReceiveAwaiter::Waiter* receiver_tail_{nullptr};
};

// ============================================================================
// AsyncLatch - Single-use barrier that blocks until count reaches zero
// ============================================================================

class AsyncLatch {
public:
    explicit AsyncLatch(std::size_t count) noexcept : count_(count) {}

    ~AsyncLatch() = default;

    AsyncLatch(const AsyncLatch&) = delete;
    AsyncLatch& operator=(const AsyncLatch&) = delete;

    // Awaiter for wait()
    class WaitAwaiter {
    public:
        explicit WaitAwaiter(AsyncLatch& latch) noexcept : latch_(latch) {}

        bool await_ready() const noexcept {
            return latch_.count_.load(std::memory_order_acquire) == 0;
        }

        bool await_suspend(coroutine_handle<> handle) noexcept {
            waiter_.handle = handle;
            std::lock_guard<std::mutex> lock(latch_.mutex_);

            if (latch_.count_.load(std::memory_order_relaxed) == 0) {
                return false;
            }

            waiter_.next = latch_.head_;
            latch_.head_ = &waiter_;
            return true;
        }

        void await_resume() const noexcept {}

    private:
        friend class AsyncLatch;

        struct Waiter {
            coroutine_handle<> handle;
            Waiter* next{nullptr};
        };

        AsyncLatch& latch_;
        Waiter waiter_;
    };

    // Wait for count to reach zero
    CUPCPP_NODISCARD WaitAwaiter wait() noexcept {
        return WaitAwaiter{*this};
    }

    // Decrement the count
    void count_down(std::size_t n = 1) {
        std::vector<coroutine_handle<>> to_resume;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto old_count = count_.load(std::memory_order_relaxed);
            auto new_count = (n >= old_count) ? 0 : old_count - n;
            count_.store(new_count, std::memory_order_release);

            if (new_count == 0) {
                auto* waiter = head_;
                head_ = nullptr;

                while (waiter) {
                    to_resume.push_back(waiter->handle);
                    waiter = waiter->next;
                }
            }
        }

        for (auto handle : to_resume) {
            handle.resume();
        }
    }

    // Check if ready
    CUPCPP_NODISCARD bool try_wait() const noexcept {
        return count_.load(std::memory_order_acquire) == 0;
    }

private:
    std::mutex mutex_;
    std::atomic<std::size_t> count_;
    WaitAwaiter::Waiter* head_{nullptr};
};

} // namespace cupcpp

#endif // CUPCPP_SYNC_HPP
