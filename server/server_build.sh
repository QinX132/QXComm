#!/bin/bash
set -e
pushd src/backEnd > /dev/null 
	rm -rf build && mkdir build && pushd build > /dev/null
	cmake ..
	make -B
	popd > /dev/null
popd > /dev/null
