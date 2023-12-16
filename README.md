# RamBam

Stress test your API/web app/website using massive parallel HTTP requests. Which can be triggered remotely.
Written in C++, using a [C++20 HTTP client](https://github.com/avocadoboi/cpp20-http-client) (written in C++ as well and using TCP sockets).

## Requirements

- C++ Compiler (`sudo apt install build-essential`)
- CMake (`sudo apt install cmake`)
- Ccache (optional, but much **recommended**: `sudo apt install ccache`)

## Build

Building the RamBam binary is easy:

```bash
# Configure build folder (prepare)
cmake -B build
# Build it! Using make
cmake --build ./build -j 8 --target rambam
```

Binary is now located at: `build/rambam`.
