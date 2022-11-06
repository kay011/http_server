#!/bin/bash
echo "start clang-format..."
clang-format -i include/*.h
clang-format -i src/*.cc
clang-format -i test/*.cc
echo "end clang-format..."
rm -rf build
mkdir build
cd build
cmake ..
make
