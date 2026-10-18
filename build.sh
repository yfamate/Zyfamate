#!/bin/sh
set -e
CXX=${CXX:-clang++}
$CXX -std=c++20 -O3 -march=native -static source/main.cpp -o "${1:-zyfamate.exe}" -lpthread
