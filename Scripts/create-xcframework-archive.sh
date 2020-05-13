#!/bin/sh
set -xe

# Set up the target folder
OUTPUT_DEST="${BUILT_PRODUCTS_DIR}"
PRODUCT_DIR="${PRODUCT_NAME}-XCFramework-${CURRENT_PROJECT_VERSION}"
DESTROOT="${OUTPUT_DEST}/${PRODUCT_DIR}"

mkdir -p "${DESTROOT}"

LICENSE_DEST="${DESTROOT}/LICENSE.txt"

DOC_SUBDIR="Documentation"

# Populate the target folder
mkdir -p "${DESTROOT}/CrashReporter.xcframework"
rsync -av "${BUILD_DIR}/${CONFIGURATION}-xcframework/CrashReporter.xcframework" "${DESTROOT}"

rsync -av "${SRCROOT}/LICENSE" "${LICENSE_DEST}"

# Copy Documentation
rm -rf "${DESTROOT}/${DOC_SUBDIR}"
rsync -av "${BUILD_DIR}/${DOC_SUBDIR}" "${DESTROOT}"

# Add a top-level Documentation symlink
pushd "${DESTROOT}" >/dev/null
    ln -sf "${DOC_SUBDIR}/index.html" "API Documentation.html"
popd >/dev/null

# Create the ZIP archive
rm -f "${DESTROOT}.zip"
pushd "${OUTPUT_DEST}" >/dev/null
    zip --symlinks -r "${PRODUCT_DIR}.zip" "${PRODUCT_DIR}"
popd >/dev/null
