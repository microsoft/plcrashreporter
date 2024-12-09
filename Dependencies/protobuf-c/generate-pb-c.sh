#!/bin/sh

cd "$(dirname "$0")/../../Source/" && protoc-c --c_out=. "PLCrashReport.proto"