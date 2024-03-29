#!/bin/bash
set -e
rm -rf build && mkdir build && pushd build > /dev/null
cmake -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF ..
make -B
popd > /dev/null
