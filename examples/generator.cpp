// Generator<T> example - demonstrating lazy sequences
// Compile: g++ -std=c++20 -I../include generator.cpp -o generator -pthread

#include <cupcpp/cupcpp.hpp>
#include <iostream>
#include <vector>
#include <string>

// Generate Fibonacci numbers
cupcpp::Generator<long long> fibonacci(int count) {
    long long a = 0, b = 1;
    for (int i = 0; i < count; ++i) {
        co_yield a;
        auto next = a + b;
        a = b;
        b = next;
    }
}

// Generate prime numbers up to n
cupcpp::Generator<int> primes(int max) {
    std::vector<bool> is_prime(max + 1, true);
    is_prime[0] = is_prime[1] = false;

    for (int i = 2; i <= max; ++i) {
        if (is_prime[i]) {
            co_yield i;
            for (int j = i * 2; j <= max; j += i) {
                is_prime[j] = false;
            }
        }
    }
}

// Generate lines from a text (simulating file reading)
cupcpp::Generator<std::string> split_lines(const std::string& text) {
    std::string line;
    for (char c : text) {
        if (c == '\n') {
            co_yield line;
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) {
        co_yield line;
    }
}

// Infinite counter generator
cupcpp::Generator<int> counter(int start = 0) {
    int i = start;
    while (true) {
        co_yield i++;
    }
}

// Generate powers of 2
cupcpp::Generator<long long> powers_of_two(int count) {
    long long value = 1;
    for (int i = 0; i < count; ++i) {
        co_yield value;
        value *= 2;
    }
}

int main() {
    std::cout << "=== Generator<T> Examples ===\n\n";

    // Example 1: Fibonacci sequence
    std::cout << "1. First 15 Fibonacci numbers:\n   ";
    for (auto fib : fibonacci(15)) {
        std::cout << fib << " ";
    }
    std::cout << "\n\n";

    // Example 2: Prime numbers
    std::cout << "2. Prime numbers up to 50:\n   ";
    for (auto prime : primes(50)) {
        std::cout << prime << " ";
    }
    std::cout << "\n\n";

    // Example 3: Using iota (built-in range generator)
    std::cout << "3. iota(5, 15):\n   ";
    for (auto i : cupcpp::iota(5, 15)) {
        std::cout << i << " ";
    }
    std::cout << "\n\n";

    // Example 4: String splitting
    std::cout << "4. Split lines:\n";
    std::string text = "Line 1\nLine 2\nLine 3\nLine 4";
    int line_num = 1;
    for (const auto& line : split_lines(text)) {
        std::cout << "   " << line_num++ << ": " << line << "\n";
    }
    std::cout << "\n";

    // Example 5: Take from infinite generator
    std::cout << "5. First 10 from infinite counter (starting at 100):\n   ";
    auto gen = counter(100);
    for (int i = 0; i < 10; ++i) {
        gen.next();
        std::cout << gen.value() << " ";
    }
    std::cout << "\n\n";

    // Example 6: Powers of 2
    std::cout << "6. First 16 powers of 2:\n   ";
    for (auto pow : powers_of_two(16)) {
        std::cout << pow << " ";
    }
    std::cout << "\n\n";

    // Example 7: Using take helper
    std::cout << "7. Take first 5 primes:\n   ";
    for (auto p : cupcpp::take(primes(100), 5)) {
        std::cout << p << " ";
    }
    std::cout << "\n\n";

    // Example 8: Using filter helper
    std::cout << "8. Even Fibonacci numbers (first 10 fibs):\n   ";
    for (auto f : cupcpp::filter(fibonacci(10), [](auto n) { return n % 2 == 0; })) {
        std::cout << f << " ";
    }
    std::cout << "\n\n";

    // Example 9: Using transform helper
    std::cout << "9. Squared values (1-5):\n   ";
    for (auto sq : cupcpp::transform(cupcpp::iota(1, 6), [](int n) { return n * n; })) {
        std::cout << sq << " ";
    }
    std::cout << "\n\n";

    // Example 10: Repeat value
    std::cout << "10. Repeat 'x' 5 times:\n   ";
    for (auto c : cupcpp::repeat('x', 5)) {
        std::cout << c << " ";
    }
    std::cout << "\n\n";

    std::cout << "=== All examples completed ===\n";
    return 0;
}
