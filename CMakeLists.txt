cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME RamBam)
set(PROJECT_TARGET rambam)

option(DOXYGEN "Build Documentation" OFF)
option(PACKAGE "Build packages in release mode" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# If we have a git folder, get GIT_TAG_VERSION from most recent git tag
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  include(GitVersion)
  set(LOCAL_PROJECT_VERSION ${GIT_TAG_VERSION})
else()
  # In source archives there is no .git folder present.
  # If CUSTOM_PROJECT_VERSION is defined use it as version.
  if(DEFINED CUSTOM_PROJECT_VERSION)
      set(LOCAL_PROJECT_VERSION "${CUSTOM_PROJECT_VERSION}")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/version.txt")
    # Read version from version.txt file
    file(READ "version.txt" LOCAL_PROJECT_VERSION)
  else()
    # Fallback to version 1.0.0
    set(LOCAL_PROJECT_VERSION "1.0.0")
  endif()
endif()


find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Build code documentation using Doxygen
if(DOXYGEN)
  include(Doxygen)
endif()

project(${PROJECT_NAME}
  VERSION ${LOCAL_PROJECT_VERSION}
  DESCRIPTION "Stress test your website/API with massive parallel HTTP requests"
  LANGUAGES CXX)

cmake_policy(VERSION 3.10...3.24)

# Enable compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror")
# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
# Enable optimization for release
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# Production build
if("${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND PACKAGE)
  # Include CPack packaging settings
  include(Packaging)
endif()

configure_file(
  "${PROJECT_SOURCE_DIR}/include/project_config.h.in"
  "${PROJECT_BINARY_DIR}/project_config.h")

set(HEADERS
  include/client.h
  include/handler.h
  include/output.h
  include/reply_struct.h
  include/duration_struct.h
  include/result_response_struct.h
)

set(SOURCES
  src/main.cc
  src/client.cc
  src/handler.cc
  src/output.cc
  ${HEADERS}
)

# Add the executable
add_executable(${PROJECT_TARGET} ${SOURCES})

# Set C++20
target_compile_features(${PROJECT_TARGET} PUBLIC cxx_std_20)
set_target_properties(${PROJECT_TARGET} PROPERTIES CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)

# Download external dependencies
# Asio
include(FetchContent)
FetchContent_Declare(asio
  GIT_REPOSITORY  https://github.com/chriskohlhoff/asio.git
  GIT_TAG         asio-1-30-2
  GIT_SHALLOW     TRUE
)
FetchContent_MakeAvailable(asio)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
# Use as standalone library and do not allow deprecated features
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
# Link threads as dependency
target_link_libraries(asio INTERFACE Threads::Threads)

# Lightweight C++ command line option parser
FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG        v3.2.0
  GIT_SHALLOW    TRUE
)
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cxxopts)

# Link CXXOPTS, Asio and OpenSSL
target_link_libraries(${PROJECT_TARGET} cxxopts asio OpenSSL::Crypto OpenSSL::SSL)

target_include_directories(${PROJECT_TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR})

install(TARGETS ${PROJECT_TARGET} RUNTIME DESTINATION "bin" COMPONENT applications)
