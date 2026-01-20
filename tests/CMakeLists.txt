# Tests for cupcpp library

# Simple test runner without external dependencies
add_executable(test_runner
    test_main.cpp
    test_task.cpp
    test_generator.cpp
    test_sync.cpp
    test_executor.cpp
)

target_link_libraries(test_runner PRIVATE cupcpp::cupcpp)
target_compile_features(test_runner PRIVATE cxx_std_20)

# Register with CTest
add_test(NAME cupcpp_tests COMMAND test_runner)
