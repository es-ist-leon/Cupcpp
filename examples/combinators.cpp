// Combinators example - demonstrating task composition
// Compile: g++ -std=c++20 -I../include combinators.cpp -o combinators -pthread

#include <cupcpp/cupcpp.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <random>

// Simulated async operations
cupcpp::Task<int> fetch_value(int id, int delay_ms) {
    std::cout << "  Fetching value " << id << "...\n";
    co_await cupcpp::sleep_for(std::chrono::milliseconds(delay_ms));
    std::cout << "  Value " << id << " ready\n";
    co_return id * 10;
}

cupcpp::Task<std::string> fetch_string(const std::string& name) {
    co_await cupcpp::sleep_for(std::chrono::milliseconds(50));
    co_return "Hello, " + name + "!";
}

cupcpp::Task<int> may_fail(bool should_fail) {
    if (should_fail) {
        throw std::runtime_error("Intentional failure");
    }
    co_return 42;
}

// Retry demonstration
int attempt_counter = 0;
cupcpp::Task<int> flaky_operation() {
    attempt_counter++;
    std::cout << "    Attempt " << attempt_counter << "\n";

    if (attempt_counter < 3) {
        throw std::runtime_error("Not yet!");
    }
    co_return 100;
}

int main() {
    std::cout << "=== Combinator Examples ===\n";

    // Example 1: then - transform result
    std::cout << "\n1. then() - transform result:\n";
    {
        auto task = fetch_value(5, 50);
        auto transformed = cupcpp::then(std::move(task), [](int value) {
            std::cout << "  Transforming " << value << " -> " << value * 2 << "\n";
            return value * 2;
        });

        auto result = cupcpp::sync_wait(std::move(transformed));
        std::cout << "  Final result: " << result << "\n";
    }

    // Example 2: then with void task
    std::cout << "\n2. then() with void task:\n";
    {
        auto void_task = []() -> cupcpp::Task<void> {
            std::cout << "  Doing some work...\n";
            co_return;
        };

        auto chained = cupcpp::then(void_task(), []() {
            std::cout << "  Work completed, returning value\n";
            return 42;
        });

        auto result = cupcpp::sync_wait(std::move(chained));
        std::cout << "  Result: " << result << "\n";
    }

    // Example 3: catch_exception
    std::cout << "\n3. catch_exception() - handle errors:\n";
    {
        auto task = may_fail(true);
        auto safe_task = cupcpp::catch_exception(std::move(task), [](std::exception_ptr) {
            std::cout << "  Caught exception, returning default\n";
            return -1;
        });

        auto result = cupcpp::sync_wait(std::move(safe_task));
        std::cout << "  Result: " << result << "\n";
    }

    // Example 4: retry
    std::cout << "\n4. retry() - retry on failure:\n";
    {
        attempt_counter = 0;  // Reset counter

        auto result = cupcpp::sync_wait(
            cupcpp::retry([]() { return flaky_operation(); }, 5)
        );

        std::cout << "  Final result after " << attempt_counter << " attempts: " << result << "\n";
    }

    // Example 5: sequence - run tasks sequentially
    std::cout << "\n5. sequence() - sequential execution:\n";
    {
        std::vector<cupcpp::Task<int>> tasks;
        tasks.push_back(fetch_value(1, 30));
        tasks.push_back(fetch_value(2, 20));
        tasks.push_back(fetch_value(3, 10));

        auto results = cupcpp::sync_wait(cupcpp::sequence(std::move(tasks)));

        std::cout << "  Results: ";
        for (auto r : results) {
            std::cout << r << " ";
        }
        std::cout << "\n";
    }

    // Example 6: race - first successful result
    std::cout << "\n6. race() - first successful result:\n";
    {
        std::vector<cupcpp::Task<int>> tasks;

        // Create tasks where some might fail
        auto success_task = []() -> cupcpp::Task<int> {
            std::cout << "  Success task running\n";
            co_return 999;
        };

        tasks.push_back(success_task());

        auto winner = cupcpp::sync_wait(cupcpp::race(std::move(tasks)));

        if (winner) {
            std::cout << "  Winner: " << *winner << "\n";
        } else {
            std::cout << "  No successful task\n";
        }
    }

    // Example 7: Practical example - fetch multiple resources
    std::cout << "\n7. Practical example - fetching resources:\n";
    {
        auto fetch_user = [](int id) -> cupcpp::Task<std::string> {
            co_await cupcpp::sleep_for(std::chrono::milliseconds(30));
            co_return "User_" + std::to_string(id);
        };

        auto fetch_posts = [](int user_id) -> cupcpp::Task<int> {
            co_await cupcpp::sleep_for(std::chrono::milliseconds(20));
            co_return user_id * 5;  // Number of posts
        };

        auto fetch_comments = [](int user_id) -> cupcpp::Task<int> {
            co_await cupcpp::sleep_for(std::chrono::milliseconds(25));
            co_return user_id * 10;  // Number of comments
        };

        // Fetch user first, then fetch posts and comments
        auto workflow = [&]() -> cupcpp::Task<void> {
            int user_id = 42;

            std::cout << "  Fetching user " << user_id << "...\n";
            auto username = co_await fetch_user(user_id);
            std::cout << "  Got user: " << username << "\n";

            std::cout << "  Fetching posts and comments...\n";
            auto posts = co_await fetch_posts(user_id);
            auto comments = co_await fetch_comments(user_id);

            std::cout << "  User " << username << " has " << posts
                      << " posts and " << comments << " comments\n";
        };

        cupcpp::sync_wait(workflow());
    }

    // Example 8: Pipeline pattern
    std::cout << "\n8. Pipeline pattern:\n";
    {
        auto pipeline = []() -> cupcpp::Task<int> {
            // Stage 1: Get initial value
            auto value = co_await fetch_value(1, 20);
            std::cout << "  Stage 1 result: " << value << "\n";

            // Stage 2: Transform
            value = value * 2;
            std::cout << "  Stage 2 result: " << value << "\n";

            // Stage 3: Add more
            auto extra = co_await fetch_value(2, 15);
            value += extra;
            std::cout << "  Stage 3 result: " << value << "\n";

            co_return value;
        };

        auto result = cupcpp::sync_wait(pipeline());
        std::cout << "  Final pipeline result: " << result << "\n";
    }

    std::cout << "\n=== All examples completed ===\n";
    return 0;
}
