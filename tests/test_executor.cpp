// Tests for executors

#include <cupcpp/cupcpp.hpp>
#include <vector>
#include <atomic>
#include <chrono>

// Test framework declarations
namespace test {
    int register_test(const std::string& name, std::function<void()> func);
    void check(bool condition, const std::string& message, const char* file, int line);
}

#define TEST(name) \
    void test_##name(); \
    static int test_##name##_registered = test::register_test(#name, test_##name); \
    void test_##name()

#define CHECK(cond) test::check(cond, #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) test::check((a) == (b), std::string(#a " == " #b), __FILE__, __LINE__)

// Tests for ManualExecutor

TEST(manual_executor_basic) {
    cupcpp::ManualExecutor executor;

    CHECK(!executor.has_pending());
    CHECK_EQ(executor.pending(), 0u);

    int counter = 0;
    executor.execute([&] { counter += 1; });
    executor.execute([&] { counter += 10; });

    CHECK(executor.has_pending());
    CHECK_EQ(executor.pending(), 2u);
    CHECK_EQ(counter, 0);  // Not executed yet

    CHECK(executor.run_one());
    CHECK_EQ(counter, 1);
    CHECK_EQ(executor.pending(), 1u);

    CHECK(executor.run_one());
    CHECK_EQ(counter, 11);
    CHECK_EQ(executor.pending(), 0u);

    CHECK(!executor.run_one());  // Nothing left
}

TEST(manual_executor_run_all) {
    cupcpp::ManualExecutor executor;

    int counter = 0;
    executor.execute([&] { counter += 1; });
    executor.execute([&] { counter += 2; });
    executor.execute([&] { counter += 3; });

    auto ran = executor.run_all();
    CHECK_EQ(ran, 3u);
    CHECK_EQ(counter, 6);
}

// Tests for InlineExecutor

TEST(inline_executor_basic) {
    cupcpp::InlineExecutor executor;

    int counter = 0;
    executor.execute([&] { counter += 1; });
    CHECK_EQ(counter, 1);  // Executed immediately

    executor.execute([&] { counter += 10; });
    CHECK_EQ(counter, 11);  // Executed immediately
}

// Tests for ThreadPool

TEST(thread_pool_basic) {
    cupcpp::ThreadPool pool(2);

    CHECK_EQ(pool.size(), 2u);
    CHECK(!pool.is_stopped());
}

TEST(thread_pool_submit) {
    cupcpp::ThreadPool pool(2);

    auto future = pool.submit([] {
        return 42;
    });

    auto result = future.get();
    CHECK_EQ(result, 42);
}

TEST(thread_pool_submit_multiple) {
    cupcpp::ThreadPool pool(4);

    std::vector<std::future<int>> futures;

    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([i] {
            return i * 10;
        }));
    }

    for (int i = 0; i < 10; ++i) {
        auto result = futures[i].get();
        CHECK_EQ(result, i * 10);
    }
}

TEST(thread_pool_execute) {
    cupcpp::ThreadPool pool(2);
    std::atomic<int> counter{0};

    pool.execute([&] { counter++; });
    pool.execute([&] { counter++; });
    pool.execute([&] { counter++; });

    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    CHECK_EQ(counter.load(), 3);
}

TEST(thread_pool_stop) {
    auto pool = std::make_unique<cupcpp::ThreadPool>(2);

    CHECK(!pool->is_stopped());

    pool->stop();
    CHECK(pool->is_stopped());

    pool.reset();  // Destructor should be safe
    CHECK(true);
}

// Tests for global pool

TEST(global_pool_exists) {
    auto& pool = cupcpp::global_pool();
    CHECK(pool.size() > 0);
}

// Tests for scheduling utilities

TEST(sleep_for_basic) {
    auto task = []() -> cupcpp::Task<int> {
        auto start = std::chrono::steady_clock::now();
        co_await cupcpp::sleep_for(std::chrono::milliseconds(50));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        ).count();
        co_return static_cast<int>(elapsed);
    };

    auto elapsed = cupcpp::sync_wait(task());
    CHECK(elapsed >= 45);  // Allow some tolerance
}

TEST(resume_on_executor) {
    cupcpp::ManualExecutor executor;

    auto task = [&]() -> cupcpp::Task<int> {
        co_await cupcpp::resume_on(executor);
        co_return 42;
    };

    auto t = task();
    CHECK(executor.has_pending());

    executor.run_all();
    CHECK(!executor.has_pending());
}
