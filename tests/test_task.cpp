// Tests for Task<T>

#include <cupcpp/cupcpp.hpp>
#include <string>
#include <stdexcept>

// Test framework declarations (defined in test_main.cpp)
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

// Helper coroutines
cupcpp::Task<int> return_int(int value) {
    co_return value;
}

cupcpp::Task<std::string> return_string(const std::string& s) {
    co_return s;
}

cupcpp::Task<void> return_void() {
    co_return;
}

cupcpp::Task<int> add_values(int a, int b) {
    auto val_a = co_await return_int(a);
    auto val_b = co_await return_int(b);
    co_return val_a + val_b;
}

cupcpp::Task<int> throw_exception() {
    throw std::runtime_error("test error");
    co_return 0;
}

// Tests

TEST(task_returns_int) {
    auto result = cupcpp::sync_wait(return_int(42));
    CHECK_EQ(result, 42);
}

TEST(task_returns_string) {
    auto result = cupcpp::sync_wait(return_string("hello"));
    CHECK(result == "hello");
}

TEST(task_returns_void) {
    cupcpp::sync_wait(return_void());
    CHECK(true);  // If we got here, it worked
}

TEST(task_chaining) {
    auto result = cupcpp::sync_wait(add_values(10, 20));
    CHECK_EQ(result, 30);
}

TEST(task_exception_propagates) {
    bool caught = false;
    try {
        cupcpp::sync_wait(throw_exception());
    } catch (const std::runtime_error& e) {
        caught = true;
        CHECK(std::string(e.what()) == "test error");
    }
    CHECK(caught);
}

TEST(task_move_semantics) {
    auto task1 = return_int(100);
    CHECK(task1.valid());

    auto task2 = std::move(task1);
    CHECK(!task1.valid());
    CHECK(task2.valid());

    auto result = cupcpp::sync_wait(std::move(task2));
    CHECK_EQ(result, 100);
}

TEST(task_make_ready) {
    auto task = cupcpp::make_ready_task(55);
    auto result = cupcpp::sync_wait(std::move(task));
    CHECK_EQ(result, 55);
}

TEST(task_make_ready_void) {
    auto task = cupcpp::make_ready_task();
    cupcpp::sync_wait(std::move(task));
    CHECK(true);
}

TEST(task_nested_await) {
    auto outer = []() -> cupcpp::Task<int> {
        auto inner = []() -> cupcpp::Task<int> {
            co_return co_await return_int(5);
        };
        auto val = co_await inner();
        co_return val * 2;
    };

    auto result = cupcpp::sync_wait(outer());
    CHECK_EQ(result, 10);
}

TEST(task_multiple_awaits) {
    auto multi = []() -> cupcpp::Task<int> {
        int sum = 0;
        sum += co_await return_int(1);
        sum += co_await return_int(2);
        sum += co_await return_int(3);
        sum += co_await return_int(4);
        sum += co_await return_int(5);
        co_return sum;
    };

    auto result = cupcpp::sync_wait(multi());
    CHECK_EQ(result, 15);
}
