#!/usr/bin/env bash
FLAGS=$1

echo "Cloning latest google test repository"
wget https://github.com/google/googletest/archive/release-1.11.0.tar.gz -O googletest-1.11.0.tar.gz
tar xf googletest-1.11.0.tar.gz

mkdir build
cd build
cmake ../googletest-release-1.11.0 -DCMAKE_INSTALL_PREFIX=../dist -DCMAKE_CXX_FLAGS="${FLAGS}"
cmake --build . --target install
