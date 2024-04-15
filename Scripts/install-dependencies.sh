#!/bin/bash

PROTOBUF_VERSION="26.1"
PROTOBUF_PLATFORM="osx-universal_binary"
PROTOBUF_C_VERSION="1.5.0"
PROTOBUF_C_URL="https://github.com/protobuf-c/protobuf-c/releases/download/v${PROTOBUF_C_VERSION}/protobuf-c-${PROTOBUF_C_VERSION}.tar.gz"
PROTOBUF_URL=https://github.com/google/protobuf/releases/download/v${PROTOBUF_VERSION}/protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}.zip

brew install automake libtool wget

# Determine the shell
if [[ $SHELL == */bash ]]; then
    CONFIG_FILE="$HOME/.bashrc"
elif [[ $SHELL == */zsh ]]; then
    CONFIG_FILE="$HOME/.zshrc"
else
    echo "Unsupported shell: $SHELL"
    exit 1
fi

echo "Config file: $CONFIG_FILE"

# Install protoc
echo "Removing old protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}"
rm -rf protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}
rm -rf protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}.zip

echo "Downloading ${URL}"
wget ${PROTOBUF_URL}

echo "Unzipping protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}.zip"
unzip protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}.zip -d protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}

# Add protoc to PATH if not already present
if [[ ":$PзATH:" != *":${PWD}/protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}/bin:"* ]]; then
    echo "Adding protoc to PATH"
    echo -e "\nexport PATH=\"${PWD}/protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}/bin:\$PATH\"" >> "$CONFIG_FILE"
    source "$CONFIG_FILE"
    echo protoc version: $(protoc --version)
fi

# Install protobuf-c
echo "Removing old protobuf-c-${PROTOBUF_C_VERSION}"
rm -rf protobuf-c-${PROTOBUF_C_VERSION}
rm -rf protobuf-c-${PROTOBUF_C_VERSION}.tar.gz

echo "Downloading ${PROTOBUF_C_URL}"
wget ${PROTOBUF_C_URL}
tar -xzvf protobuf-c-${PROTOBUF_C_VERSION}.tar.gz 

cd protobuf-c-${PROTOBUF_C_VERSION}
./autogen.sh
./configure
make
make install
