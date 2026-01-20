// ThreadPool example - demonstrating parallel execution
// Compile: g++ -std=c++20 -I../include thread_pool.cpp -o thread_pool -pthread

#include <cupcpp/cupcpp.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>

// Helper to get thread ID as string
std::string thread_id() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

// Simulated async work
cupcpp::Task<int> async_work(int id, int work_ms) {
    std::cout << "  Task " << id << " starting on thread " << thread_id() << "\n";

    // Simulate work by sleeping
    co_await cupcpp::sleep_for(std::chrono::milliseconds(work_ms));

    std::cout << "  Task " << id << " completed on thread " << thread_id() << "\n";
    co_return id * 10;
}

// Task that switches to thread pool
cupcpp::Task<int> compute_on_pool(cupcpp::ThreadPool& pool, int value) {
    std::cout << "  Before switch: thread " << thread_id() << "\n";

    // Switch to thread pool
    co_await pool.schedule_on();

    std::cout << "  After switch: thread " << thread_id() << "\n";

    // Do some computation
    int result = 0;
    for (int i = 0; i < value; ++i) {
        result += i;
    }

    co_return result;
}

int main() {
    std::cout << "=== ThreadPool Examples ===\n\n";
    std::cout << "Main thread: " << thread_id() << "\n\n";

    // Example 1: Basic thread pool usage
    std::cout << "1. Basic thread pool (4 workers):\n";
    {
        cupcpp::ThreadPool pool(4);
        std::cout << "   Pool size: " << pool.size() << " threads\n";

        // Submit some work
        auto future1 = pool.submit([] {
            std::cout << "   Lambda running on thread " << thread_id() << "\n";
            return 42;
        });

        auto future2 = pool.submit([] {
            std::cout << "   Another lambda on thread " << thread_id() << "\n";
            return 100;
        });

        std::cout << "   Results: " << future1.get() << ", " << future2.get() << "\n";
    }
    std::cout << "\n";

    // Example 2: Running coroutines on thread pool
    std::cout << "2. Coroutines on thread pool:\n";
    {
        cupcpp::ThreadPool pool(2);

        auto result = cupcpp::run_on(compute_on_pool(pool, 100), pool);
        std::cout << "   Sum of 0-99: " << result << "\n";
    }
    std::cout << "\n";

    // Example 3: Global thread pool
    std::cout << "3. Using global thread pool:\n";
    {
        auto task = []() -> cupcpp::Task<std::string> {
            co_await cupcpp::schedule(); // Switch to global pool
            std::ostringstream oss;
            oss << "Running on global pool, thread " << thread_id();
            co_return oss.str();
        };

        auto result = cupcpp::sync_wait(task());
        std::cout << "   " << result << "\n";
    }
    std::cout << "\n";

    // Example 4: ManualExecutor for testing
    std::cout << "4. ManualExecutor for controlled execution:\n";
    {
        cupcpp::ManualExecutor executor;

        int counter = 0;
        executor.execute([&] { counter += 1; });
        executor.execute([&] { counter += 10; });
        executor.execute([&] { counter += 100; });

        std::cout << "   Pending tasks: " << executor.pending() << "\n";
        std::cout << "   Running one task...\n";
        executor.run_one();
        std::cout << "   Counter after 1 task: " << counter << "\n";

        std::cout << "   Running remaining tasks...\n";
        auto ran = executor.run_all();
        std::cout << "   Ran " << ran << " tasks, counter: " << counter << "\n";
    }
    std::cout << "\n";

    // Example 5: InlineExecutor
    std::cout << "5. InlineExecutor (immediate execution):\n";
    {
        cupcpp::InlineExecutor executor;

        std::cout << "   Before execute, thread " << thread_id() << "\n";
        executor.execute([] {
            std::cout << "   During execute, thread " << thread_id() << "\n";
        });
        std::cout << "   After execute, thread " << thread_id() << "\n";
    }
    std::cout << "\n";

    // Example 6: Sleep and timing
    std::cout << "6. Async sleep:\n";
    {
        auto timed_task = []() -> cupcpp::Task<int> {
            auto start = std::chrono::steady_clock::now();

            std::cout << "   Sleeping for 100ms...\n";
            co_await cupcpp::sleep_for(std::chrono::milliseconds(100));

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();

            std::cout << "   Woke up after " << elapsed << "ms\n";
            co_return static_cast<int>(elapsed);
        };

        cupcpp::sync_wait(timed_task());
    }
    std::cout << "\n";

    std::cout << "=== All examples completed ===\n";
    return 0;
}
