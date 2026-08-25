# This is your build configuration
cmake_minimum_required(VERSION 3.16)  # Minimum CMake version
project(log_aggregator)               # Project name

add_executable(test_phase1            # Create executable
    tests/test_phase1.cpp             # From this source
    src/tokenizer.cpp                 # And this one
    src/index.cpp                     # And this one
)

target_include_directories(test_phase1 PRIVATE include/)
# Tell compiler where to find headers