#!/bin/bash
set -e

protoc -I=. --cpp_out=. ./QXSCMsg.proto
echo "Build proto success!"
