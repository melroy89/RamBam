cmake_minimum_required(VERSION 3.12)

project(RamBam)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the executable
add_executable(rambam src/main.cpp)
