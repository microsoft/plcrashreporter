#!/bin/sh
set -e

# Build Mac Catalyst.
# Print only target name and issues. Mimic Xcode output to make prettify tools happy.
echo "=== BUILD TARGET $1 OF PROJECT ${PROJECT_NAME} WITH CONFIGURATION ${CONFIGURATION} ==="
# OBJROOT must be customized to avoid conflicts with the current process.
xcodebuild -quiet \
    SYMROOT="${SYMROOT}" OBJROOT="${BUILT_PRODUCTS_DIR}" PROJECT_TEMP_DIR="${PROJECT_TEMP_DIR}" \
    ONLY_ACTIVE_ARCH=NO ARCHS="${ARCHS}" BITCODE_GENERATION_MODE=bitcode OTHER_CFLAGS="-fembed-bitcode" \
    -project "${PROJECT_NAME}.xcodeproj" -configuration "${CONFIGURATION}" -scheme "${PROJECT_NAME} iOS" \
    -destination 'platform=macOS,variant=Mac Catalyst'

# Build XCFramework.
for SDK in iphoneos iphonesimulator appletvos appletvsimulator macOS maccatalyst; do
  HEADERS_PATH="${BUILD_DIR}/${CONFIGURATION}-${SDK}/include"
  LIBRARY_PATH="${BUILD_DIR}/${CONFIGURATION}-${SDK}/lib${PRODUCT_NAME}.a"
  XC_LIBRARIES="${XC_LIBRARIES} -library ${LIBRARY_PATH} -headers ${HEADERS_PATH}"
done
xcodebuild -create-xcframework ${XC_LIBRARIES} -output ${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.xcframework
