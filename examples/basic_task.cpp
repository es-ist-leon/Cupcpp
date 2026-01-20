// Basic Task<T> example - demonstrating async coroutines
// Compile: g++ -std=c++20 -I../include basic_task.cpp -o basic_task -pthread

#include <cupcpp/cupcpp.hpp>
#include <iostream>
#include <string>

// Simple async function that returns a value
cupcpp::Task<int> compute_value() {
    co_return 42;
}

// Async function that calls another async function
cupcpp::Task<int> compute_sum(int a, int b) {
    auto base = co_await compute_value();
    co_return base + a + b;
}

// Async function that can fail
cupcpp::Task<int> divide(int a, int b) {
    if (b == 0) {
        throw std::runtime_error("Division by zero");
    }
    co_return a / b;
}

// Async function returning void
cupcpp::Task<void> print_message(const std::string& msg) {
    std::cout << "Message: " << msg << "\n";
    co_return;
}

// Chaining multiple async operations
cupcpp::Task<std::string> fetch_and_process() {
    auto value1 = co_await compute_value();
    auto value2 = co_await compute_sum(10, 20);

    co_return "Result: " + std::to_string(value1) + " + " + std::to_string(value2);
}

int main() {
    std::cout << "=== Basic Task<T> Examples ===\n\n";

    // Example 1: Simple task execution
    std::cout << "1. Simple task:\n";
    auto result1 = cupcpp::sync_wait(compute_value());
    std::cout << "   compute_value() = " << result1 << "\n\n";

    // Example 2: Chained tasks
    std::cout << "2. Chained tasks:\n";
    auto result2 = cupcpp::sync_wait(compute_sum(5, 3));
    std::cout << "   compute_sum(5, 3) = " << result2 << "\n\n";

    // Example 3: Void task
    std::cout << "3. Void task:\n";
    cupcpp::sync_wait(print_message("Hello from coroutine!"));
    std::cout << "\n";

    // Example 4: String result
    std::cout << "4. String result:\n";
    auto result4 = cupcpp::sync_wait(fetch_and_process());
    std::cout << "   " << result4 << "\n\n";

    // Example 5: Exception handling
    std::cout << "5. Exception handling:\n";
    try {
        auto result5 = cupcpp::sync_wait(divide(10, 0));
        std::cout << "   Result: " << result5 << "\n";
    } catch (const std::exception& e) {
        std::cout << "   Caught exception: " << e.what() << "\n";
    }
    std::cout << "\n";

    // Example 6: Ready tasks
    std::cout << "6. Ready tasks:\n";
    auto ready_task = cupcpp::make_ready_task(100);
    auto result6 = cupcpp::sync_wait(std::move(ready_task));
    std::cout << "   make_ready_task(100) = " << result6 << "\n\n";

    std::cout << "=== All examples completed ===\n";
    return 0;
}
