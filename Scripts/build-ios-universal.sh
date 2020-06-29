#!/bin/sh
set -e

# Helper variables
DEVICE_SDK="iphoneos"
SIMULATOR_SDK="iphonesimulator"
TARGET_NAME="${PROJECT_NAME} iOS Framework"
DEVICE_DIR="${BUILD_DIR}/${CONFIGURATION}-${DEVICE_SDK}"
SIMULATOR_DIR="${BUILD_DIR}/${CONFIGURATION}-${SIMULATOR_SDK}"
LIBRARY_BINARY="lib${PRODUCT_NAME}.a"
FRAMEWORK_BINARY="${PRODUCT_NAME}.framework/${PRODUCT_NAME}"

# Building both SDKs
build() {
    # Print only target name and issues. Mimic Xcode output to make prettify tools happy.
    echo "=== BUILD TARGET $1 OF PROJECT ${PROJECT_NAME} WITH CONFIGURATION ${CONFIGURATION} ==="
    # OBJROOT must be customized to avoid conflicts with the current process.
    env -i "PATH=$PATH" xcodebuild \
        SYMROOT="${SYMROOT}" OBJROOT="${BUILT_PRODUCTS_DIR}" PROJECT_TEMP_DIR="${PROJECT_TEMP_DIR}" \
        ONLY_ACTIVE_ARCH=NO \
        -project "${PROJECT_NAME}.xcodeproj" -configuration "${CONFIGURATION}" -target "$1" -sdk "$2"
}
echo "Building the library for ${DEVICE_SDK} and ${SIMULATOR_SDK}..."
build "${TARGET_NAME}" "${DEVICE_SDK}"
build "${TARGET_NAME}" "${SIMULATOR_SDK}"

# Clean output folder
rm -rf "${BUILT_PRODUCTS_DIR}"
mkdir -p "${BUILT_PRODUCTS_DIR}"

# Combine libraries and frameworks
echo "Combining libraries..."
lipo \
    "${DEVICE_DIR}/${LIBRARY_BINARY}" \
    "${SIMULATOR_DIR}/${LIBRARY_BINARY}" \
    -create -output "${BUILT_PRODUCTS_DIR}/${LIBRARY_BINARY}"
echo "Final library architectures: $(lipo -archs "${BUILT_PRODUCTS_DIR}/${LIBRARY_BINARY}")"

# Frameworks contains additional linker data and should be processed separately.
echo "Combining frameworks..."
cp -R "${DEVICE_DIR}/${PRODUCT_NAME}.framework" "${BUILT_PRODUCTS_DIR}"
lipo \
    "${BUILT_PRODUCTS_DIR}/${FRAMEWORK_BINARY}" \
    "${SIMULATOR_DIR}/${FRAMEWORK_BINARY}" \
    -create -output "${BUILT_PRODUCTS_DIR}/${FRAMEWORK_BINARY}"
echo "Final framework architectures: $(lipo -archs "${BUILT_PRODUCTS_DIR}/${FRAMEWORK_BINARY}")"

echo "Appending simulator to Info.plist"
plutil -insert CFBundleSupportedPlatforms.1 -string "iPhoneSimulator" "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.framework/Info.plist"
