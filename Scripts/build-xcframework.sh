#!/bin/sh

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

# Work dir is directory where all XCFramework artifacts is stored.
WORK_DIR="${SRCROOT}/build"
XCFRAMEWORK_DIR="${WORK_DIR}/xcframework"
MACOS_DIR="macOS"
TVOS_DEVICE_SDK="appletvos"
TVOS_SIMULATOR_SDK="appletvsimulator"
IOS_DEVICE_SDK="iphoneos"
IOS_SIMULATOR_SDK="iphonesimulator"

# Output dir will be the final output to to the framework.
XC_FRAMEWORK_PATH="${XCFRAMEWORK_DIR}/Output/${PROJECT_NAME}.xcframework"

# Copy all framework files to use them for xcframework file creation.
mkdir -p "${XCFRAMEWORK_DIR}"

# Clean previus XCFramework build.
rm -rf ${PROJECT_NAME}.xcframework/

# Build XCFramework.
function SetXcBuildCommandFramework() {
    FRAMEWORK_PATH="$XCFRAMEWORK_DIR/${CONFIGURATION}-$1/${PROJECT_NAME}.framework"
    [ -e "$FRAMEWORK_PATH" ] && XC_BUILD_COMMAND="$XC_BUILD_COMMAND -framework $FRAMEWORK_PATH";
}

for arch_name in $IOS_DEVICE_SDK $IOS_SIMULATOR_SDK $TVOS_DEVICE_SDK $TVOS_SIMULATOR_SDK $MACOS_DIR; do
  cp -R "${BUILD_DIR}/${CONFIGURATION}-$arch_name/" "${XCFRAMEWORK_DIR}/"${CONFIGURATION}"-$arch_name"
  SetXcBuildCommandFramework $arch_name
done

if [ -z "$XC_BUILD_COMMAND" ]; then
  echo "You must build 'Disk Image' before executing this scheme."
  exit 1
fi

XC_BUILD_COMMAND="xcodebuild -create-xcframework $XC_BUILD_COMMAND -output $XC_FRAMEWORK_PATH"
eval "$XC_BUILD_COMMAND"
