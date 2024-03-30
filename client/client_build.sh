#!/bin/bash
set -e
rm -rf build && mkdir build && pushd build > /dev/null
cmake ..
make -B
popd > /dev/null
