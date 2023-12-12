#!/usr/bin/env bash
# By: Melroy van den Berg
# Description: Release production build to /usr
rm -rf build_prod
cmake -GNinja -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE=Release -B build_prod
cmake --build ./build_prod --target rambam --config Release
