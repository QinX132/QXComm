#!/bin/bash
set -e

FOLDER_NEED_TO_BUILD=("GmSSL" "libevent")

for subdir in */ ; do
	subdir=${subdir%/}
	if [[ " ${FOLDER_NEED_TO_BUILD[*]} " == *" $subdir "* ]]; then
		pushd "$subdir" > /dev/null
		echo "Building $subdir"
		rm -rf build && mkdir build && pushd build > /dev/null
		cmake -DBUILD_SHARED_LIBS=OFF ..
#		cmake ..
        make -B
        popd > /dev/null
        popd > /dev/null
	fi
done
