// SPDX-License-Identifier: GPL-3.0-or-later
// Cupcpp - Modern C++ Coroutine Utilities Library
// https://github.com/es-ist-leon/Cupcpp
//
// A comprehensive, header-only C++ coroutine utilities library providing:
//   - Task<T>: Lazy coroutine type for async operations
//   - Generator<T>: Synchronous generator for lazy sequences
//   - Async primitives: Mutex, Semaphore, Channel, Event, Latch
//   - Thread pool executor with scheduling support
//   - Combinators: when_all, when_any, then, retry, sequence
//
// Quick Start:
//
//   #include <cupcpp/cupcpp.hpp>
//   #include <iostream>
//
//   cupcpp::Task<int> async_computation() {
//       co_return 42;
//   }
//
//   cupcpp::Generator<int> fibonacci(int n) {
//       int a = 0, b = 1;
//       for (int i = 0; i < n; ++i) {
//           co_yield a;
//           auto next = a + b;
//           a = b;
//           b = next;
//       }
//   }
//
//   int main() {
//       // Run async task synchronously
//       int result = cupcpp::sync_wait(async_computation());
//       std::cout << "Result: " << result << "\n";
//
//       // Use generator
//       for (int fib : fibonacci(10)) {
//           std::cout << fib << " ";
//       }
//       std::cout << "\n";
//   }
//
// Requires C++20 with coroutine support or C++17 with coroutine TS.
// Compile with: -std=c++20 -fcoroutines (GCC/Clang) or /std:c++20 (MSVC)

#ifndef CUPCPP_HPP
#define CUPCPP_HPP

#include "config.hpp"
#include "task.hpp"
#include "generator.hpp"
#include "sync.hpp"
#include "executor.hpp"
#include "combinators.hpp"

#endif // CUPCPP_HPP
