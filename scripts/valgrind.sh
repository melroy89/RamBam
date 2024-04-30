#!/usr/bin/env bash
# By: Melroy van den Berg
# Description: Used for memory leak analysis

cd ./build
G_DEBUG=gc-friendly G_SLICE=always-malloc valgrind \
--leak-check=full --show-leak-kinds=all --track-origins=yes \
./rambam
