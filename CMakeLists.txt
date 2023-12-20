cmake_minimum_required(VERSION 3.20)

include(FetchContent)

set(PROJECT_NAME RamBam)
set(PROJECT_TARGET rambam)
set(PROJECT_VERSION_MAJOR "1")
set(PROJECT_VERSION_MINOR "0")
set(PROJECT_VERSION_PATCH "0")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

project(${PROJECT_NAME}
  VERSION ${PROJECT_VERSION}
  DESCRIPTION "Stress test your website/API with massive parallel HTTP requests"
  LANGUAGES CXX)

# Enable compiler warnings
add_compile_options(-Wall -Wextra -pedantic)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
# Enable optimization
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

configure_file(
  "${PROJECT_SOURCE_DIR}/include/project_config.h.in"
  "${PROJECT_BINARY_DIR}/project_config.h")

set(HEADERS
)

set(SOURCES
  src/main.cc
  ${HEADERS}
)

# Add the executable
add_executable(${PROJECT_TARGET} ${SOURCES})

# Set C++20
target_compile_features(${PROJECT_TARGET} PUBLIC cxx_std_20)
set_target_properties(${PROJECT_TARGET} PROPERTIES CXX_EXTENSIONS OFF)

# Download external dependencies

# CPP20 HTTP Client
FetchContent_Declare(
    Cpp20HttpClient
    GIT_REPOSITORY https://github.com/melroy89/cpp20-http-client.git
    GIT_TAG        origin/main
    GIT_SHALLOW    TRUE
)
set(CPP20_HTTP_CLIENT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CPP20_HTTP_CLIENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CPP20_HTTP_CLIENT_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Cpp20HttpClient)

# Lightweight C++ command line option parser
include(FetchContent)
FetchContent_Declare(
        cxxopts
        GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
        GIT_TAG        v3.1.1
        GIT_SHALLOW    TRUE
)
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cxxopts)

# Link HTTP Client for C++20 & CXXOPTS
target_link_libraries(${PROJECT_TARGET} Cpp20HttpClient::cpp20_http_client cxxopts)

target_include_directories(${PROJECT_TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR})
