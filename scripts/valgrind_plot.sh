#!/usr/bin/env bash
# By: Melroy van den Berg
# Description: Generate memory usage plots in massif format (https://valgrind.org/docs/manual/ms-manual.html),

cd ./build
G_DEBUG=gc-friendly G_SLICE=always-malloc valgrind \
--tool=massif \
./rambam
