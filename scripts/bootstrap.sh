#!/bin/bash

git submodule update --init --recursive --remote    

cd Tools/protobuf-c &&  ./autogen.sh && ./configure && make && make install && cd ../..
mkdir -p Dependencies/protobuf-c/protobuf-c
cp Tools/protobuf-c/protobuf-c/protobuf-c.h Dependencies/protobuf-c/protobuf-c
cp Tools/protobuf-c/protobuf-c/protobuf-c.c Dependencies/protobuf-c/protobuf-c

brew bundle
bundle update