#!/bin/sh
set -e

# Remove the previous version of the xcframework.
rm -rf "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.xcframework"
rm -rf "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}-static.xcframework"

# Combine all frameworks into xcframework.
for sdk in iphoneos iphonesimulator appletvos appletvsimulator maccatalyst; do
  framework_path="${BUILD_DIR}/${CONFIGURATION}-${sdk}/${PRODUCT_NAME}.framework"
  xcframeworks+=( -framework "${framework_path}")
  xcframeworksStatic+=( -framework "${framework_path}")
done

# Add macOS with dynamic framework to CrashReporter XCFramework.
framework_path="${BUILD_DIR}/${CONFIGURATION}-macosx/${PRODUCT_NAME}.framework"
xcframeworks+=( -framework "${framework_path}")

# Add macOS with static framework to CrashReporter Static XCFramework.
framework_path="${BUILD_DIR}/${CONFIGURATION}-macosx-static/${PRODUCT_NAME}.framework"
xcframeworksStatic+=( -framework "${framework_path}")

xcodebuild -create-xcframework "${xcframeworks[@]}" -output "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.xcframework"
xcodebuild -create-xcframework "${xcframeworksStatic[@]}" -output "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}-static.xcframework"
