cmake_minimum_required(VERSION 3.14)

project(cupcpp
    VERSION 0.1.0
    DESCRIPTION "Modern C++ Coroutine Utilities Library"
    HOMEPAGE_URL "https://github.com/es-ist-leon/Cupcpp"
    LANGUAGES CXX
)

# Options
option(CUPCPP_BUILD_EXAMPLES "Build example programs" ON)
option(CUPCPP_BUILD_TESTS "Build unit tests" ON)

# Require C++20 (or C++17 with coroutine TS)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Header-only library interface
add_library(cupcpp INTERFACE)
add_library(cupcpp::cupcpp ALIAS cupcpp)

target_include_directories(cupcpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Compiler-specific flags for coroutine support
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(cupcpp INTERFACE
        $<$<VERSION_LESS:$<CXX_COMPILER_VERSION>,10>:-fcoroutines>
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(cupcpp INTERFACE
        $<$<VERSION_LESS:$<CXX_COMPILER_VERSION>,14>:-fcoroutines-ts>
    )
endif()

# Thread support
find_package(Threads REQUIRED)
target_link_libraries(cupcpp INTERFACE Threads::Threads)

# Build examples
if(CUPCPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Build tests
if(CUPCPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS cupcpp
    EXPORT cupcppTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/cupcpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT cupcppTargets
    FILE cupcppTargets.cmake
    NAMESPACE cupcpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cupcpp
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cupcppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cupcppConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cupcpp
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cupcppConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cupcppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cupcppConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cupcpp
)

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "cupcpp")
set(CPACK_PACKAGE_VENDOR "Cupcpp Authors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern C++ Coroutine Utilities Library")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
include(CPack)
