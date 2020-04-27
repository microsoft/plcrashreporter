#!/bin/sh
set -xe

DEVICE_SDK="appletvos"
SIMULATOR_SDK="appletvsimulator"
TARGET_NAME="${PROJECT_NAME} tvOS"

# Building both SDKs
echo "\n\n\n 🛠 Building for ${DEVICE_SDK}"
xcodebuild \
    BUILD_DIR="${BUILD_DIR}" BUILD_ROOT="${BUILD_ROOT}" ONLY_ACTIVE_ARCH=NO \
    BITCODE_GENERATION_MODE=bitcode OTHER_CFLAGS="-fembed-bitcode" \
    -project "${PROJECT_NAME}.xcodeproj" -configuration "${CONFIGURATION}" -target "${TARGET_NAME}" -sdk "${DEVICE_SDK}" \
    clean build

echo "\n\n\n 🛠 Building for ${SIMULATOR_SDK}"
xcodebuild \
    BUILD_DIR="${BUILD_DIR}" BUILD_ROOT="${BUILD_ROOT}" ONLY_ACTIVE_ARCH=NO \
    BITCODE_GENERATION_MODE=bitcode OTHER_CFLAGS="-fembed-bitcode" \
    -project "${PROJECT_NAME}.xcodeproj" -configuration "${CONFIGURATION}" -target "${TARGET_NAME}" -sdk "${SIMULATOR_SDK}" \
    clean build

# Combine libraries
echo "\n\n\n 📦 Combining libraries"
mkdir -p "${BUILT_PRODUCTS_DIR}"
lipo \
    "${BUILD_DIR}/${CONFIGURATION}-${DEVICE_SDK}/lib${PRODUCT_NAME}.a" \
    "${BUILD_DIR}/${CONFIGURATION}-${SIMULATOR_SDK}/lib${PRODUCT_NAME}.a" \
    -create -output "${BUILT_PRODUCTS_DIR}/lib${PRODUCT_NAME}.a"
echo "Final library architectures: $(lipo -archs "${BUILT_PRODUCTS_DIR}/lib${PRODUCT_NAME}.a")"

# Pack to fake framework
cd "${BUILT_PRODUCTS_DIR}"
FULL_PRODUCT_NAME="${PRODUCT_NAME}.framework"
if [ -d "${FULL_PRODUCT_NAME}" ]; then
    rm -rf "${FULL_PRODUCT_NAME}"
fi

# Copy module map
mkdir -p "${FULL_PRODUCT_NAME}/Versions/${FRAMEWORK_VERSION}/Modules"
cp -f "${SRCROOT}/${MODULEMAP_FILE}" "${FULL_PRODUCT_NAME}/Versions/${FRAMEWORK_VERSION}/Modules/module.modulemap"

# Copy headers files
mkdir -p "${FULL_PRODUCT_NAME}/Versions/${FRAMEWORK_VERSION}/Headers"
cp -R "${BUILD_DIR}/${CONFIGURATION}-${DEVICE_SDK}/include/" "${FULL_PRODUCT_NAME}/Versions/${FRAMEWORK_VERSION}/Headers"

# Copy library
cp "${BUILT_PRODUCTS_DIR}/lib${PRODUCT_NAME}.a" "${FULL_PRODUCT_NAME}/Versions/${FRAMEWORK_VERSION}/${PRODUCT_NAME}"

# Create links
ln -sfh ${FRAMEWORK_VERSION} "${FULL_PRODUCT_NAME}/Versions/Current"
ln -sfh Versions/Current/Modules "${FULL_PRODUCT_NAME}/Modules"
ln -sfh Versions/Current/Headers "${FULL_PRODUCT_NAME}/Headers"
ln -sfh Versions/Current/${PRODUCT_NAME} "${FULL_PRODUCT_NAME}/${PRODUCT_NAME}"
