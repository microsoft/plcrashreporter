#!/bin/bash

# Определите желаемую версию Protobuf
PROTOBUF_VERSION="25.3"
URL=https://github.com/google/protobuf/releases/download/v${PROTOBUF_VERSION}/protobuf-${PROTOBUF_VERSION}.tar.g

brew install automake libtool wget
wget ${URL}
tar -xvjf protobuf-${PROTOBUF_VERSION}.tar.bz2
cd protobuf-${PROTOBUF_VERSION}
./autogen.sh
./configure
make; make check
sudo make install