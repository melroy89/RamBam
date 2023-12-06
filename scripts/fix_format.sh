#!/usr/bin/env bash
find src/ -iname *.cc -o -iname *.h -o -iname *.h.in | xargs clang-format -style=file -fallback-style=LLVM -assume-filename=../.clang-format
