// Simple test framework for cupcpp
// No external dependencies required

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

// Simple test framework
namespace test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

int register_test(const std::string& name, std::function<void()> func) {
    get_tests().push_back({name, func});
    return 0;
}

void check(bool condition, const std::string& message, const char* file, int line) {
    if (!condition) {
        throw std::runtime_error(
            std::string(file) + ":" + std::to_string(line) + ": " + message
        );
    }
}

} // namespace test

#define TEST(name) \
    void test_##name(); \
    static int test_##name##_registered = test::register_test(#name, test_##name); \
    void test_##name()

#define CHECK(cond) test::check(cond, #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) test::check((a) == (b), std::string(#a " == " #b) + " (got " + std::to_string(a) + " vs " + std::to_string(b) + ")", __FILE__, __LINE__)
#define CHECK_THROWS(expr) \
    do { \
        bool threw = false; \
        try { expr; } catch (...) { threw = true; } \
        test::check(threw, #expr " should throw", __FILE__, __LINE__); \
    } while(0)

// Forward declarations of test registration
void register_task_tests();
void register_generator_tests();
void register_sync_tests();
void register_executor_tests();

int main() {
    std::cout << "=== Cupcpp Test Suite ===\n\n";

    int passed = 0;
    int failed = 0;

    for (const auto& test : test::get_tests()) {
        std::cout << "Running: " << test.name << "... ";
        std::cout.flush();

        try {
            test.func();
            std::cout << "PASSED\n";
            passed++;
        } catch (const std::exception& e) {
            std::cout << "FAILED\n";
            std::cout << "  Error: " << e.what() << "\n";
            failed++;
        }
    }

    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Total:  " << (passed + failed) << "\n";

    return failed > 0 ? 1 : 0;
}
