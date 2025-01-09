#!/bin/sh

cd "$(dirname "$0")/../../Source/" && protoc-c --c_out=. "PLCrashReport.proto" && cd "../Tests/" && protoc-c --c_out=. "PLCrashLogWriterEncodingTests.proto"