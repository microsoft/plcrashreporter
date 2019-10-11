#!/bin/sh

cd protobuf-c
echo "runnig autogen.sh"
./autogen.sh
echo "runnig build"
./configure --prefix=$(pwd)/../protobuf-output && make && make install