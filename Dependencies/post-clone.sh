#!/bin/sh
set -e

cd protobuf-c
echo "runnig autogen.sh"
./autogen.sh
echo "runnig build"
./configure --prefix=$(pwd)/../protobuf-output && make && make install
echo "copy protobuf-c.c file to output folder"
cp $(pwd)/protobuf-c/protobuf-c.c $(pwd)/../protobuf-output
