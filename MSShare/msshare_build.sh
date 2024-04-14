#!/bin/bash
set -e

protoc -I=. --cpp_out=. ./QXMSMsg.proto

echo "Build msproto success!"

mv QXMSMsg.pb.h include/
