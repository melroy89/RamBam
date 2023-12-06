# RamBam

Stress test your API/web app/website using massive parallel HTTP requests. Which can be triggered remotely.
Written in C++, using a [C++20 HTTP client](https://github.com/avocadoboi/cpp20-http-client) (written in C++ as well and using TCP sockets).

## Requirements

- C++ Compiler (`sudo apt install build-essential`)
- CMake (`sudo apt install cmake`)

## Build

Building the RamBam binary is easy:

```bash
# First create a build folder
mkdir build
# Go into the folder
cd build
# Prepare the make
cmake ..
# Build it!
make -j 4
```
