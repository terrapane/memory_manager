cmake_minimum_required(VERSION 3.21)

# Define the Memory Manager project
project(memory_manager
        VERSION 1.0.0.0
        DESCRIPTION "Memory Manager Library"
        LANGUAGES CXX)

# Set options depending on whether this is a subproject
if(PROJECT_IS_TOP_LEVEL)
    # Option to control whether tests are built
    option(memory_manager_BUILD_TESTS "Build Tests for the Memory Manager" ON)
else()
    # Option to control whether tests are built
    option(memory_manager_BUILD_TESTS "Build Tests for the Memory Manager" OFF)
endif()

# Option to control ability to install the library
option(memory_manager_INSTALL "Install the Memory Manager" ON)

# Determine whether clang-tidy should be used during build
option(memory_manager_CLANG_TIDY "Use clang-tidy to perform linting during build" OFF)

add_subdirectory(dependencies)
add_subdirectory(src)

include(CTest)

if(BUILD_TESTING AND memory_manager_BUILD_TESTS)
    add_subdirectory(test)
endif()
