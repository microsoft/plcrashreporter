#!/bin/sh
set -xe

# Set up the destroot
OUTPUT_DEST="${BUILT_PRODUCTS_DIR}"
PRODUCT_DIR="${PRODUCT_NAME}-Static-${CURRENT_PROJECT_VERSION}"
DESTROOT="${OUTPUT_DEST}/${PRODUCT_DIR}"

mkdir -p "${DESTROOT}"

HEADERS_DEST="${DESTROOT}/include"
LICENSE_DEST="${DESTROOT}/LICENSE.txt"

DOC_SUBDIR="Documentation"

# Per-platform framework sources
MAC_SRC="${BUILD_DIR}/${CONFIGURATION}-macOS"
IPHONE_SRC="${BUILD_DIR}/${CONFIGURATION}-iOS"
APPLETV_SRC="${BUILD_DIR}/${CONFIGURATION}-tvOS"

# Populate the destroot
cp "${IPHONE_SRC}/libCrashReporter.a" "${DESTROOT}/libCrashReporter-iOS.a"
cp "${APPLETV_SRC}/libCrashReporter.a" "${DESTROOT}/libCrashReporter-tvOS.a"
cp "${MAC_SRC}/libCrashReporter.a" "${DESTROOT}/libCrashReporter-MacOSX-Static.a"

rsync -av "${SRCROOT}/LICENSE" "${LICENSE_DEST}"

# Copy Documentation
rm -rf "${DESTROOT}/${DOC_SUBDIR}"
rsync -av "${BUILD_DIR}/${DOC_SUBDIR}" "${DESTROOT}"

# Add a top-level Documentation symlink
pushd "${DESTROOT}" >/dev/null
    ln -sf "${DOC_SUBDIR}/index.html" "API Documentation.html"
popd >/dev/null

# Copy headers
mkdir -p "${HEADERS_DEST}"
cp -r "${IPHONE_SRC}/CrashReporter.framework/Headers/." "${HEADERS_DEST}/"

# Create the ZIP archive
rm -f "${DESTROOT}.zip"
pushd "${OUTPUT_DEST}" >/dev/null
    zip --symlinks -r "${PRODUCT_DIR}.zip" "${PRODUCT_DIR}"
popd >/dev/null
