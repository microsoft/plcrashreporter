#!/bin/sh
set -e

# Build Mac Catalyst.
# Print only target name and issues. Mimic Xcode output to make prettify tools happy.
echo "=== BUILD TARGET ${PROJECT_NAME} iOS Framework OF PROJECT ${PROJECT_NAME} WITH CONFIGURATION ${CONFIGURATION} ==="
# OBJROOT must be customized to avoid conflicts with the current process.
env -i "PATH=$PATH" xcodebuild \
    SYMROOT="${SYMROOT}" OBJROOT="${BUILT_PRODUCTS_DIR}" PROJECT_TEMP_DIR="${PROJECT_TEMP_DIR}" \
    ONLY_ACTIVE_ARCH=NO ARCHS="${ARCHS}" \
    -project "${PROJECT_NAME}.xcodeproj" -configuration "${CONFIGURATION}" -scheme "${PROJECT_NAME} iOS Framework" \
    -destination 'platform=macOS,variant=Mac Catalyst'

# Remove the previous version of the xcframework.
rm -rf "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.xcframework"

# Build XCFramework.
for SDK in iphoneos iphonesimulator appletvos appletvsimulator macOS maccatalyst; do
  FRAMEWORK_PATH="${BUILD_DIR}/${CONFIGURATION}-${SDK}/${PRODUCT_NAME}.framework"
  XC_FRAMEWORKS+=( -framework "${FRAMEWORK_PATH}")
done
xcodebuild -create-xcframework "${XC_FRAMEWORKS[@]}" -output "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.xcframework"
