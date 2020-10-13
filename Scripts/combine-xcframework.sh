#!/bin/sh
set -e

# Remove the previous version of the xcframework.
rm -rf "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.xcframework"

# Combine all frameworks into xcframework.
framework_path="${BUILD_DIR}/${CONFIGURATION}/${PRODUCT_NAME}.framework"
xcframeworks+=( -framework "${framework_path}")
for sdk in iphoneos iphonesimulator appletvos appletvsimulator maccatalyst; do
  framework_path="${BUILD_DIR}/${CONFIGURATION}-${sdk}/${PRODUCT_NAME}.framework"
  xcframeworks+=( -framework "${framework_path}")
done
xcodebuild -create-xcframework "${xcframeworks[@]}" -output "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.xcframework"
