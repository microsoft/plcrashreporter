#!/bin/sh
set -xe

# Set up the destroot
OUTPUT_DEST="${BUILT_PRODUCTS_DIR}"
PRODUCT_DIR="${PRODUCT_NAME}-XCFramework-${CURRENT_PROJECT_VERSION}"
DESTROOT="${OUTPUT_DEST}/${PRODUCT_DIR}"

mkdir -p "${DESTROOT}"

LICENSE_DEST="${DESTROOT}/LICENSE.txt"

DOC_SUBDIR="Documentation"

# XCFramework sources
XCF_SRC="${BUILD_DIR}/${CONFIGURATION}-xcframework"

# Populate the destroot
mkdir -p "${DESTROOT}/CrashReporter.xcframework"
rsync -av "${XCF_SRC}/CrashReporter.xcframework" "${DESTROOT}"

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
