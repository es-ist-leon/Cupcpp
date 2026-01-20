// SPDX-License-Identifier: GPL-3.0-or-later
// Cupcpp - Modern C++ Coroutine Utilities Library
// https://github.com/es-ist-leon/Cupcpp

#ifndef CUPCPP_CONFIG_HPP
#define CUPCPP_CONFIG_HPP

// Version information
#define CUPCPP_VERSION_MAJOR 0
#define CUPCPP_VERSION_MINOR 1
#define CUPCPP_VERSION_PATCH 0
#define CUPCPP_VERSION_STRING "0.1.0"

// C++ standard detection
#if defined(_MSVC_LANG)
    #define CUPCPP_CPLUSPLUS _MSVC_LANG
#else
    #define CUPCPP_CPLUSPLUS __cplusplus
#endif

#if CUPCPP_CPLUSPLUS >= 202002L
    #define CUPCPP_CPP20 1
#elif CUPCPP_CPLUSPLUS >= 201703L
    #define CUPCPP_CPP17 1
#else
    #error "Cupcpp requires C++17 or later"
#endif

// Coroutine support detection
#if defined(CUPCPP_CPP20)
    #include <version>
    #if defined(__cpp_impl_coroutine) && defined(__cpp_lib_coroutine)
        #define CUPCPP_HAS_COROUTINES 1
        #include <coroutine>
        namespace cupcpp {
            using std::coroutine_handle;
            using std::suspend_always;
            using std::suspend_never;
            using std::noop_coroutine;
        }
    #endif
#endif

// Experimental coroutine support (C++17 with coroutine TS)
#if !defined(CUPCPP_HAS_COROUTINES)
    #if __has_include(<experimental/coroutine>)
        #define CUPCPP_HAS_COROUTINES 1
        #define CUPCPP_EXPERIMENTAL_COROUTINES 1
        #include <experimental/coroutine>
        namespace cupcpp {
            using std::experimental::coroutine_handle;
            using std::experimental::suspend_always;
            using std::experimental::suspend_never;
            // noop_coroutine may not exist in experimental
            #if defined(__cpp_lib_noop_coroutine)
                using std::experimental::noop_coroutine;
            #endif
        }
    #endif
#endif

#if !defined(CUPCPP_HAS_COROUTINES)
    #error "Cupcpp requires coroutine support. Use C++20 or enable coroutine TS with -fcoroutines-ts (Clang) or /await (MSVC)"
#endif

// Compiler detection
#if defined(_MSC_VER)
    #define CUPCPP_MSVC 1
    #define CUPCPP_FORCEINLINE __forceinline
    #define CUPCPP_NOINLINE __declspec(noinline)
#elif defined(__clang__)
    #define CUPCPP_CLANG 1
    #define CUPCPP_FORCEINLINE __attribute__((always_inline)) inline
    #define CUPCPP_NOINLINE __attribute__((noinline))
#elif defined(__GNUC__)
    #define CUPCPP_GCC 1
    #define CUPCPP_FORCEINLINE __attribute__((always_inline)) inline
    #define CUPCPP_NOINLINE __attribute__((noinline))
#else
    #define CUPCPP_FORCEINLINE inline
    #define CUPCPP_NOINLINE
#endif

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define CUPCPP_WINDOWS 1
#elif defined(__linux__)
    #define CUPCPP_LINUX 1
#elif defined(__APPLE__)
    #define CUPCPP_MACOS 1
#endif

// Utility macros
#define CUPCPP_NODISCARD [[nodiscard]]
#define CUPCPP_MAYBE_UNUSED [[maybe_unused]]

// Assert configuration
#ifndef CUPCPP_ASSERT
    #include <cassert>
    #define CUPCPP_ASSERT(cond) assert(cond)
#endif

// Debug mode
#if !defined(NDEBUG)
    #define CUPCPP_DEBUG 1
#endif

namespace cupcpp {

// Type aliases for clarity
using handle_type = void*;

} // namespace cupcpp

#endif // CUPCPP_CONFIG_HPP
