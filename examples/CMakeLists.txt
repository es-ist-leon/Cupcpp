# Examples for cupcpp library

# Helper function to add an example
function(add_cupcpp_example name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE cupcpp::cupcpp)
    target_compile_features(${name} PRIVATE cxx_std_20)
endfunction()

# Basic examples
add_cupcpp_example(basic_task)
add_cupcpp_example(generator)
add_cupcpp_example(thread_pool)
add_cupcpp_example(channel)
add_cupcpp_example(combinators)
