#!/usr/bin/env bash
# By: Melroy van den Berg
# Description: Debug build
rm -rf build_debug
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -B build_debug
cmake --build ./build_debug --target rambam --config Debug
