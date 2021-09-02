#!/bin/sh
set -e

PREF="static"

# Remove the previous version of the xcframework.
rm -rf "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.xcframework"
rm -rf "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}-${PREF}.xcframework"

# Combine all frameworks into xcframework.
for sdk in iphoneos iphonesimulator appletvos appletvsimulator maccatalyst; do
  framework_path="${BUILD_DIR}/${CONFIGURATION}-${sdk}/${PRODUCT_NAME}.framework"
  xcframeworks+=( -framework "${framework_path}")
  xcframeworksStatic+=( -framework "${framework_path}")
done

# Add macOS with dinamic framework to CrashReporter-XCFramework.
framework_path="${BUILD_DIR}/${CONFIGURATION}-macosx/${PRODUCT_NAME}.framework"
xcframeworks+=( -framework "${framework_path}")

# Add macOS with dinamic framework to CrashReporter-static-XCFramework.
framework_path="${BUILD_DIR}/${CONFIGURATION}-macosx-${PREF}/${PRODUCT_NAME}.framework"
xcframeworksStatic+=( -framework "${framework_path}")

xcodebuild -create-xcframework "${xcframeworks[@]}" -output "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.xcframework"
xcodebuild -create-xcframework "${xcframeworksStatic[@]}" -output "${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}-${PREF}.xcframework"
