// Tests for Generator<T>

#include <cupcpp/cupcpp.hpp>
#include <vector>
#include <string>

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

// Helper generators
cupcpp::Generator<int> range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

cupcpp::Generator<int> fibonacci(int count) {
    int a = 0, b = 1;
    for (int i = 0; i < count; ++i) {
        co_yield a;
        int next = a + b;
        a = b;
        b = next;
    }
}

cupcpp::Generator<std::string> strings() {
    co_yield "one";
    co_yield "two";
    co_yield "three";
}

cupcpp::Generator<int> empty_generator() {
    co_return;
}

cupcpp::Generator<int> throws_exception() {
    co_yield 1;
    co_yield 2;
    throw std::runtime_error("generator error");
    co_yield 3;
}

// Tests

TEST(generator_basic_iteration) {
    auto gen = range(0, 5);
    std::vector<int> values;

    for (int v : gen) {
        values.push_back(v);
    }

    CHECK_EQ(values.size(), 5u);
    CHECK_EQ(values[0], 0);
    CHECK_EQ(values[4], 4);
}

TEST(generator_fibonacci) {
    auto gen = fibonacci(7);
    std::vector<int> values;

    for (int v : gen) {
        values.push_back(v);
    }

    CHECK_EQ(values.size(), 7u);
    CHECK_EQ(values[0], 0);
    CHECK_EQ(values[1], 1);
    CHECK_EQ(values[2], 1);
    CHECK_EQ(values[3], 2);
    CHECK_EQ(values[4], 3);
    CHECK_EQ(values[5], 5);
    CHECK_EQ(values[6], 8);
}

TEST(generator_strings) {
    auto gen = strings();
    std::vector<std::string> values;

    for (const auto& s : gen) {
        values.push_back(s);
    }

    CHECK_EQ(values.size(), 3u);
    CHECK(values[0] == "one");
    CHECK(values[1] == "two");
    CHECK(values[2] == "three");
}

TEST(generator_empty) {
    auto gen = empty_generator();
    std::vector<int> values;

    for (int v : gen) {
        values.push_back(v);
    }

    CHECK_EQ(values.size(), 0u);
}

TEST(generator_move_semantics) {
    auto gen1 = range(0, 3);
    CHECK(gen1.valid());

    auto gen2 = std::move(gen1);
    CHECK(!gen1.valid());
    CHECK(gen2.valid());

    std::vector<int> values;
    for (int v : gen2) {
        values.push_back(v);
    }
    CHECK_EQ(values.size(), 3u);
}

TEST(generator_manual_iteration) {
    auto gen = range(10, 15);

    CHECK(gen.next());
    CHECK_EQ(gen.value(), 10);

    CHECK(gen.next());
    CHECK_EQ(gen.value(), 11);

    CHECK(gen.next());
    CHECK_EQ(gen.value(), 12);
}

TEST(generator_exception_propagates) {
    auto gen = throws_exception();
    std::vector<int> values;
    bool caught = false;

    try {
        for (int v : gen) {
            values.push_back(v);
        }
    } catch (const std::runtime_error& e) {
        caught = true;
    }

    CHECK(caught);
    CHECK_EQ(values.size(), 2u);  // Should have gotten 1 and 2 before exception
}

TEST(generator_iota) {
    std::vector<int> values;
    for (int v : cupcpp::iota(5)) {
        values.push_back(v);
    }

    CHECK_EQ(values.size(), 5u);
    for (size_t i = 0; i < values.size(); ++i) {
        CHECK_EQ(values[i], static_cast<int>(i));
    }
}

TEST(generator_iota_range) {
    std::vector<int> values;
    for (int v : cupcpp::iota(5, 10)) {
        values.push_back(v);
    }

    CHECK_EQ(values.size(), 5u);
    CHECK_EQ(values[0], 5);
    CHECK_EQ(values[4], 9);
}

TEST(generator_repeat) {
    std::vector<int> values;
    for (int v : cupcpp::repeat(42, 3)) {
        values.push_back(v);
    }

    CHECK_EQ(values.size(), 3u);
    CHECK_EQ(values[0], 42);
    CHECK_EQ(values[1], 42);
    CHECK_EQ(values[2], 42);
}

TEST(generator_take) {
    std::vector<int> values;
    for (int v : cupcpp::take(cupcpp::iota(100), 5)) {
        values.push_back(v);
    }

    CHECK_EQ(values.size(), 5u);
}

TEST(generator_filter) {
    std::vector<int> values;
    for (int v : cupcpp::filter(cupcpp::iota(10), [](int n) { return n % 2 == 0; })) {
        values.push_back(v);
    }

    CHECK_EQ(values.size(), 5u);  // 0, 2, 4, 6, 8
    CHECK_EQ(values[0], 0);
    CHECK_EQ(values[4], 8);
}

TEST(generator_transform) {
    std::vector<int> values;
    for (int v : cupcpp::transform(cupcpp::iota(1, 4), [](int n) { return n * n; })) {
        values.push_back(v);
    }

    CHECK_EQ(values.size(), 3u);
    CHECK_EQ(values[0], 1);
    CHECK_EQ(values[1], 4);
    CHECK_EQ(values[2], 9);
}
