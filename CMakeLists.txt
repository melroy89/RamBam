cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Get GIT_TAG_VERSION from most recent git tag
include(GitVersion)

set(PROJECT_NAME RamBam)
set(PROJECT_TARGET rambam)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

project(${PROJECT_NAME}
  VERSION ${GIT_TAG_VERSION}
  DESCRIPTION "Stress test your website/API with massive parallel HTTP requests"
  LANGUAGES CXX)

cmake_policy(VERSION 3.10...3.24)

# Enable compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic") #  -Werror
# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
# Enable optimization for release
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# Production build
if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
  # Include CPack packaging settings
  include(Packaging)
endif()

configure_file(
  "${PROJECT_SOURCE_DIR}/include/project_config.h.in"
  "${PROJECT_BINARY_DIR}/project_config.h")

set(HEADERS
  include/thread-pool.h
  include/client.h
  include/handler.h
  include/reply_struct.h
  include/duration_struct.h
  include/result_response_struct.h
)

set(SOURCES
  src/main.cc
  src/thread-pool.cc
  src/client.cc
  src/handler.cc
  ${HEADERS}
)

# Add the executable
add_executable(${PROJECT_TARGET} ${SOURCES})

# Set C++20
target_compile_features(${PROJECT_TARGET} PUBLIC cxx_std_20)
set_target_properties(${PROJECT_TARGET} PROPERTIES CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

include(FetchContent)

# Download external dependencies
# Boost Asio
set(BOOST_INCLUDE_LIBRARIES asio regex algorithm)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
  URL_MD5 893b5203b862eb9bbd08553e24ff146a
)
FetchContent_MakeAvailable(Boost)

# Lightweight C++ command line option parser
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

# Link CXXOPTS, Boost Asio plus OPenSSL
target_link_libraries(${PROJECT_TARGET} cxxopts Boost::asio OpenSSL::Crypto OpenSSL::SSL)

target_include_directories(${PROJECT_TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR})

install(TARGETS ${PROJECT_TARGET} RUNTIME DESTINATION "bin" COMPONENT applications)
