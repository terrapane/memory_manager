# Create the test excutable
add_executable(test_memory_manager test_memory_manager.cpp)

# Link to the required libraries
target_link_libraries(test_memory_manager Terra::memory_manager Terra::stf)

# Specify the C++ standard to observe
set_target_properties(test_memory_manager
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

# Specify the compiler options
target_compile_options(test_memory_manager
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)

# Ensure CTest can find the test
add_test(NAME test_memory_manager
         COMMAND test_memory_manager)
