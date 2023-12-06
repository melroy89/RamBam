cmake_minimum_required(VERSION 3.20)

include(FetchContent)

set(PROJECT_NAME RamBam)
set(PROJECT_TARGET rambam)

project(${PROJECT_NAME}
  DESCRIPTION "Stress test your website/API with massive parallel HTTP requests"
  LANGUAGES CXX)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compiler warnings
add_compile_options(-Wall -Wextra -pedantic)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
# Enable optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Add the executable
add_executable(${PROJECT_TARGET} src/main.cpp)

# Download external dependency
FetchContent_Declare(
    Cpp20HttpClient
    GIT_REPOSITORY https://github.com/avocadoboi/cpp20-http-client.git
)
FetchContent_MakeAvailable(Cpp20HttpClient)

# Link HTTP Client for C++20
target_link_libraries(${PROJECT_TARGET} Cpp20HttpClient::cpp20_http_client)
