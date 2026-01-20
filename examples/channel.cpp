// Channel example - demonstrating async communication
// Compile: g++ -std=c++20 -I../include channel.cpp -o channel -pthread

#include <cupcpp/cupcpp.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// Producer coroutine
cupcpp::Task<void> producer(cupcpp::Channel<int>& ch, int count) {
    for (int i = 1; i <= count; ++i) {
        std::cout << "  Producer: sending " << i << "\n";
        bool sent = co_await ch.send(i);
        if (!sent) {
            std::cout << "  Producer: channel closed\n";
            break;
        }
    }
    ch.close();
    std::cout << "  Producer: done, channel closed\n";
}

// Consumer coroutine
cupcpp::Task<int> consumer(cupcpp::Channel<int>& ch) {
    int sum = 0;
    while (true) {
        auto value = co_await ch.receive();
        if (!value) {
            std::cout << "  Consumer: channel closed\n";
            break;
        }
        std::cout << "  Consumer: received " << *value << "\n";
        sum += *value;
    }
    co_return sum;
}

// Buffered channel demonstration
cupcpp::Task<void> buffered_demo() {
    std::cout << "\n2. Buffered channel (capacity 3):\n";

    cupcpp::Channel<std::string> ch(3);  // Buffer of 3

    // Send without blocking (up to buffer capacity)
    std::cout << "  Sending 3 messages to buffer...\n";
    ch.try_send("Hello");
    ch.try_send("World");
    ch.try_send("!");

    std::cout << "  Buffer size: " << ch.size() << "\n";

    // Receive buffered messages
    while (auto msg = ch.try_receive()) {
        std::cout << "  Received: " << *msg << "\n";
    }

    ch.close();
}

// Async mutex demonstration
cupcpp::Task<void> mutex_demo() {
    std::cout << "\n3. AsyncMutex demonstration:\n";

    cupcpp::AsyncMutex mutex;
    int shared_counter = 0;

    auto increment = [&]() -> cupcpp::Task<void> {
        for (int i = 0; i < 5; ++i) {
            auto lock = co_await mutex.scoped_lock();
            int old_value = shared_counter;
            shared_counter++;
            std::cout << "  Task: " << old_value << " -> " << shared_counter << "\n";
        }
    };

    // Run two incrementers
    auto task1 = increment();
    auto task2 = increment();

    // Simple sequential execution for this demo
    cupcpp::sync_wait(std::move(task1));
    cupcpp::sync_wait(std::move(task2));

    std::cout << "  Final counter: " << shared_counter << "\n";
}

// Semaphore demonstration
cupcpp::Task<void> semaphore_demo() {
    std::cout << "\n4. AsyncSemaphore demonstration:\n";

    cupcpp::AsyncSemaphore sem(2);  // Allow 2 concurrent

    auto worker = [&](int id) -> cupcpp::Task<void> {
        std::cout << "  Worker " << id << " waiting for semaphore...\n";
        co_await sem.acquire();
        std::cout << "  Worker " << id << " acquired semaphore, working...\n";
        // Simulate work
        co_await cupcpp::sleep_for(std::chrono::milliseconds(50));
        std::cout << "  Worker " << id << " releasing semaphore\n";
        sem.release();
    };

    // Start workers sequentially for demo
    for (int i = 1; i <= 4; ++i) {
        cupcpp::sync_wait(worker(i));
    }
}

// Event demonstration
cupcpp::Task<void> event_demo() {
    std::cout << "\n5. AsyncEvent demonstration:\n";

    cupcpp::AsyncEvent event;

    auto waiter = [&](int id) -> cupcpp::Task<void> {
        std::cout << "  Waiter " << id << " waiting for event...\n";
        co_await event.wait();
        std::cout << "  Waiter " << id << " event received!\n";
    };

    // Start waiters
    auto w1 = waiter(1);
    auto w2 = waiter(2);

    std::cout << "  Setting event...\n";
    event.set();

    cupcpp::sync_wait(std::move(w1));
    cupcpp::sync_wait(std::move(w2));
}

// Latch demonstration
cupcpp::Task<void> latch_demo() {
    std::cout << "\n6. AsyncLatch demonstration:\n";

    cupcpp::AsyncLatch latch(3);

    auto worker = [&](int id) -> cupcpp::Task<void> {
        std::cout << "  Worker " << id << " completed task\n";
        latch.count_down();
        co_return;
    };

    auto coordinator = [&]() -> cupcpp::Task<void> {
        std::cout << "  Coordinator waiting for all workers...\n";

        // Start workers
        cupcpp::sync_wait(worker(1));
        cupcpp::sync_wait(worker(2));
        cupcpp::sync_wait(worker(3));

        co_await latch.wait();
        std::cout << "  Coordinator: all workers done!\n";
    };

    cupcpp::sync_wait(coordinator());
}

int main() {
    std::cout << "=== Async Primitives Examples ===\n";

    // Example 1: Basic producer-consumer with unbuffered channel
    std::cout << "\n1. Producer-Consumer (unbuffered channel):\n";
    {
        cupcpp::Channel<int> ch;  // Unbuffered

        // In a real scenario, these would run concurrently
        // For this demo, we use a simpler approach
        auto prod = producer(ch, 5);
        auto cons = consumer(ch);

        // Start producer
        cupcpp::sync_wait(std::move(prod));
    }

    // Example 2: Buffered channel
    cupcpp::sync_wait(buffered_demo());

    // Example 3: AsyncMutex
    cupcpp::sync_wait(mutex_demo());

    // Example 4: Semaphore
    cupcpp::sync_wait(semaphore_demo());

    // Example 5: Event
    cupcpp::sync_wait(event_demo());

    // Example 6: Latch
    cupcpp::sync_wait(latch_demo());

    std::cout << "\n=== All examples completed ===\n";
    return 0;
}
