# RamBam

Stress test your API/web app/website using massive parallel HTTP requests. Which can be triggered remotely.
Written in C++, using a [C++20 HTTP client](https://github.com/avocadoboi/cpp20-http-client) (written in C++ as well and using TCP sockets).

## Download

I didn't create a release yet (it's still in development), but if you are using Linux you can just download the job artifact from [the build job](https://gitlab.melroy.org/melroy/rambam/-/pipelines/latest) (`rambam` binary)

## Developers

### Requirements

- C++ Compiler (`sudo apt install build-essential` for GNU compiler, _Clang is **also** supported_)
- CMake (`sudo apt install cmake`)
- Ninja build system (optional, but **recommended**: `sudo apt install ninja-build`)
- Ccache (optional, but much **recommended**: `sudo apt install ccache`)

### Build

Building the RamBam binary is very easy:

```bash
# Configure build folder (prepare)
cmake -B build
# Build it! Using make
cmake --build ./build -j 8 --config Release --target rambam
```

Binary is now located at: `build/rambam`.
