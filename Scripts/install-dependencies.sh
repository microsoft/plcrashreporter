#!/bin/bash

PROTOBUF_VERSION="26.1"
PROTOBUF_PLATFORM="osx-universal_binary"
URL=https://github.com/google/protobuf/releases/download/v${PROTOBUF_VERSION}/protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}.zip

brew install automake libtool wget
echo "Removing old protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}"
rm -rf protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}
rm -rf protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}.zip

echo "Downloading ${URL}"
wget ${URL}

echo "Unzipping protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}.zip"
unzip protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}.zip -d protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}

# Determine the shell
if [[ $SHELL == */bash ]]; then
    CONFIG_FILE="$HOME/.bashrc"
elif [[ $SHELL == */zsh ]]; then
    CONFIG_FILE="$HOME/.zshrc"
else
    echo "Unsupported shell: $SHELL"
    exit 1
fi

# Add protoc to PATH if not already present
if [[ ":$PATH:" != *":${PWD}/protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}/bin:"* ]]; then
    echo "Adding protoc to PATH"
    echo -e "export PATH=\"\$PATH:${PWD}/protoc-${PROTOBUF_VERSION}-${PROTOBUF_PLATFORM}/bin\"" >> "$CONFIG_FILE"
fi