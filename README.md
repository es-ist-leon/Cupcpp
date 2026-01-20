# Cupcpp - Modern C++ Coroutine Utilities Library

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)]()
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

**Cupcpp** (C++ Coroutine Utilities) is a comprehensive, header-only library that fills the gap left by C++20 coroutines - the language added coroutine support but provided zero library utilities. Cupcpp provides everything you need to write practical asynchronous code.

## Features

- **Task<T>** - Lazy coroutine type for async operations
- **Generator<T>** - Synchronous generator for lazy sequences
- **Async Primitives** - Mutex, Semaphore, Channel, Event, Latch
- **Thread Pool** - Execute coroutines on worker threads
- **Combinators** - `when_all`, `when_any`, `then`, `retry`, `sequence`
- **Zero Dependencies** - Just the C++ standard library
- **Header-only** - Simply include and use

## Quick Start

**Get productive in under 5 minutes:**

### 1. Installation

```bash
# Clone the repository
git clone https://github.com/es-ist-leon/Cupcpp.git

# Or use CMake FetchContent
```

```cmake
include(FetchContent)
FetchContent_Declare(
    cupcpp
    GIT_REPOSITORY https://github.com/es-ist-leon/Cupcpp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(cupcpp)

target_link_libraries(your_target PRIVATE cupcpp::cupcpp)
```

### 2. Basic Usage

```cpp
#include <cupcpp/cupcpp.hpp>
#include <iostream>

// Define an async function
cupcpp::Task<int> fetch_data() {
    // Simulate async work
    co_return 42;
}

// Chain async operations
cupcpp::Task<int> compute() {
    int data = co_await fetch_data();
    co_return data * 2;
}

int main() {
    // Run synchronously
    int result = cupcpp::sync_wait(compute());
    std::cout << "Result: " << result << "\n";  // Output: Result: 84
}
```

### 3. Compile

```bash
# GCC 10+
g++ -std=c++20 -I/path/to/cupcpp/include main.cpp -pthread

# Clang 14+
clang++ -std=c++20 -I/path/to/cupcpp/include main.cpp -pthread

# MSVC 2019+
cl /std:c++20 /I/path/to/cupcpp/include main.cpp
```

## Examples

### Task<T> - Async Operations

```cpp
#include <cupcpp/cupcpp.hpp>

// Simple async function
cupcpp::Task<std::string> fetch_user(int id) {
    // In real code, this would be an async network call
    co_return "User_" + std::to_string(id);
}

// Chaining async operations
cupcpp::Task<std::string> get_greeting(int user_id) {
    std::string user = co_await fetch_user(user_id);
    co_return "Hello, " + user + "!";
}

// Exception handling works naturally
cupcpp::Task<int> may_fail(bool should_fail) {
    if (should_fail) {
        throw std::runtime_error("Something went wrong");
    }
    co_return 42;
}

int main() {
    // Synchronous execution
    auto greeting = cupcpp::sync_wait(get_greeting(123));
    std::cout << greeting << "\n";  // Hello, User_123!

    // Exception propagation
    try {
        cupcpp::sync_wait(may_fail(true));
    } catch (const std::exception& e) {
        std::cout << "Caught: " << e.what() << "\n";
    }
}
```

### Generator<T> - Lazy Sequences

```cpp
#include <cupcpp/cupcpp.hpp>

// Fibonacci generator
cupcpp::Generator<long long> fibonacci(int count) {
    long long a = 0, b = 1;
    for (int i = 0; i < count; ++i) {
        co_yield a;
        auto next = a + b;
        a = b;
        b = next;
    }
}

// Infinite sequence
cupcpp::Generator<int> naturals() {
    int i = 0;
    while (true) {
        co_yield i++;
    }
}

int main() {
    // Range-based for loop
    for (auto fib : fibonacci(10)) {
        std::cout << fib << " ";
    }
    // Output: 0 1 1 2 3 5 8 13 21 34

    // Use helper functions
    for (auto n : cupcpp::take(naturals(), 5)) {
        std::cout << n << " ";
    }
    // Output: 0 1 2 3 4

    // Filter and transform
    for (auto sq : cupcpp::transform(
            cupcpp::filter(cupcpp::iota(1, 10),
                          [](int n) { return n % 2 == 0; }),
            [](int n) { return n * n; })) {
        std::cout << sq << " ";
    }
    // Output: 4 16 36 64
}
```

### Channel<T> - Async Communication

```cpp
#include <cupcpp/cupcpp.hpp>

cupcpp::Task<void> producer(cupcpp::Channel<int>& ch) {
    for (int i = 1; i <= 5; ++i) {
        co_await ch.send(i);
        std::cout << "Sent: " << i << "\n";
    }
    ch.close();
}

cupcpp::Task<void> consumer(cupcpp::Channel<int>& ch) {
    while (auto value = co_await ch.receive()) {
        std::cout << "Received: " << *value << "\n";
    }
    std::cout << "Channel closed\n";
}
```

### Thread Pool Execution

```cpp
#include <cupcpp/cupcpp.hpp>

cupcpp::Task<int> heavy_computation(int value) {
    // Switch to thread pool
    co_await cupcpp::schedule();

    // CPU-intensive work runs on pool thread
    int result = 0;
    for (int i = 0; i < value; ++i) {
        result += i;
    }
    co_return result;
}

int main() {
    // Create custom thread pool
    cupcpp::ThreadPool pool(4);

    // Submit work
    auto future = pool.submit([] {
        return expensive_calculation();
    });

    // Or run coroutine on pool
    auto result = cupcpp::run_on(heavy_computation(1000), pool);
}
```

### Async Primitives

```cpp
#include <cupcpp/cupcpp.hpp>

// Async Mutex
cupcpp::AsyncMutex mutex;

cupcpp::Task<void> safe_increment(int& counter) {
    auto lock = co_await mutex.scoped_lock();
    counter++;
}

// Semaphore for rate limiting
cupcpp::AsyncSemaphore sem(3);  // Max 3 concurrent

cupcpp::Task<void> rate_limited_work() {
    co_await sem.acquire();
    // Do work...
    sem.release();
}

// Event for signaling
cupcpp::AsyncEvent event;

cupcpp::Task<void> waiter() {
    co_await event.wait();
    std::cout << "Event received!\n";
}

// Latch for synchronization
cupcpp::AsyncLatch latch(3);

cupcpp::Task<void> worker() {
    // Do work...
    latch.count_down();
}

cupcpp::Task<void> coordinator() {
    co_await latch.wait();
    std::cout << "All workers done!\n";
}
```

### Combinators

```cpp
#include <cupcpp/cupcpp.hpp>

// Transform results
auto task = cupcpp::then(fetch_data(), [](int data) {
    return data * 2;
});

// Handle exceptions
auto safe = cupcpp::catch_exception(risky_operation(), [](auto) {
    return default_value;
});

// Retry on failure
auto reliable = cupcpp::retry(
    []() { return flaky_operation(); },
    5,  // max attempts
    std::chrono::milliseconds(100)  // delay between
);

// Sequential execution
std::vector<cupcpp::Task<int>> tasks = {...};
auto results = co_await cupcpp::sequence(std::move(tasks));
```

## API Reference

### Core Types

| Type | Description |
|------|-------------|
| `Task<T>` | Lazy coroutine returning a value |
| `Task<void>` | Lazy coroutine returning nothing |
| `Generator<T>` | Synchronous generator yielding values |
| `RecursiveGenerator<T>` | Generator supporting nested `co_yield` |

### Synchronization Primitives

| Type | Description |
|------|-------------|
| `AsyncMutex` | Coroutine-aware mutex |
| `AsyncSemaphore` | Counting semaphore |
| `AsyncEvent` | One-shot event signal |
| `AsyncLatch` | Countdown barrier |
| `Channel<T>` | Async MPMC channel |

### Executors

| Type | Description |
|------|-------------|
| `ThreadPool` | Fixed-size thread pool |
| `InlineExecutor` | Immediate execution |
| `ManualExecutor` | Manual task control |

### Functions

| Function | Description |
|----------|-------------|
| `sync_wait(task)` | Run task synchronously |
| `when_all(tasks...)` | Wait for all to complete |
| `when_any(tasks)` | Wait for first to complete |
| `then(task, func)` | Transform result |
| `retry(factory, n)` | Retry on failure |
| `sequence(tasks)` | Run sequentially |
| `schedule()` | Switch to global pool |
| `sleep_for(duration)` | Async sleep |

### Generator Utilities

| Function | Description |
|----------|-------------|
| `iota(start, end)` | Integer range |
| `repeat(value, n)` | Repeat value |
| `take(gen, n)` | First n elements |
| `filter(gen, pred)` | Filter elements |
| `transform(gen, fn)` | Transform elements |

## Building

```bash
# Configure
cmake -B build -DCUPCPP_BUILD_EXAMPLES=ON -DCUPCPP_BUILD_TESTS=ON

# Build
cmake --build build

# Run tests
cd build && ctest

# Run examples
./build/examples/basic_task
./build/examples/generator
./build/examples/thread_pool
./build/examples/channel
./build/examples/combinators
```

## Requirements

- C++20 compiler with coroutine support:
  - GCC 10+
  - Clang 14+
  - MSVC 2019 16.8+
- CMake 3.14+ (for building examples/tests)
- pthreads (on Linux/macOS)

## Design Philosophy

Cupcpp follows the patterns of successful C++ libraries:

1. **Zero-friction integration** - Header-only, no dependencies
2. **STL-compatible interfaces** - Familiar APIs, iterators, RAII
3. **Explicit scope** - Async utilities only, not a framework
4. **Exception safety** - Proper propagation and RAII cleanup
5. **Move semantics** - Efficient ownership transfer
6. **Thread safety** - Safe concurrent access where documented

## Limitations

- `when_all` and `when_any` for variadic tasks are simplified implementations
- Cancellation support is not yet implemented
- No io_uring/IOCP integration (yet)

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Acknowledgments

Inspired by:
- [cppcoro](https://github.com/lewissbaker/cppcoro) (abandoned)
- [libcoro](https://github.com/jbaldwin/libcoro)
- [folly::coro](https://github.com/facebook/folly)
- Rust's async ecosystem
