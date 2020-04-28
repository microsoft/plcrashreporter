#!/bin/sh
set -xe

# Set up the destroot
OUTPUT_DEST="${BUILT_PRODUCTS_DIR}"
PRODUCT_DIR="${PRODUCT_NAME}-${CURRENT_PROJECT_VERSION}"
DESTROOT="${OUTPUT_DEST}/${PRODUCT_DIR}"

mkdir -p "${DESTROOT}"

# Per-target installation destinations
IPHONE_DEST="${DESTROOT}/iOS Framework"
APPLETV_DEST="${DESTROOT}/tvOS Framework"
MAC_DEST="${DESTROOT}/Mac OS X Framework"
TOOL_DEST="${DESTROOT}/Tools"
LICENSE_DEST="${DESTROOT}/LICENSE.txt"

DOC_SUBDIR="Documentation"

# Per-platform framework sources
MAC_SRC="${BUILD_DIR}/${CONFIGURATION}-macOS"
IPHONE_SRC="${BUILD_DIR}/${CONFIGURATION}-iOS"
APPLETV_SRC="${BUILD_DIR}/${CONFIGURATION}-tvOS"

# Populate the destroot
mkdir -p "${IPHONE_DEST}"
rsync -av "${IPHONE_SRC}/CrashReporter.framework" "${IPHONE_DEST}"

mkdir -p "${APPLETV_DEST}"
rsync -av "${APPLETV_SRC}/CrashReporter.framework" "${APPLETV_DEST}"

mkdir -p "${MAC_DEST}"
rsync -av "${MAC_SRC}/CrashReporter.framework" "${MAC_SRC}/CrashReporter.framework.dSYM" "${MAC_DEST}"

mkdir -p "${TOOL_DEST}"
install -m 755 "${BUILD_DIR}/${CONFIGURATION}-macOS/plcrashutil" "${TOOL_DEST}"

rsync -av "${SRCROOT}/LICENSE" "${LICENSE_DEST}"

# Copy Documentation
rm -rf "${DESTROOT}/${DOC_SUBDIR}"
rsync -av "${BUILD_DIR}/${DOC_SUBDIR}" "${DESTROOT}"

# Add a top-level Documentation symlink
pushd "${DESTROOT}" >/dev/null
    ln -sf "${DOC_SUBDIR}/index.html" "API Documentation.html"
popd >/dev/null

# Create the disk image
rm -f "${DESTROOT}.dmg"
hdiutil create -srcfolder "${DESTROOT}" "${DESTROOT}.dmg"

# Create the ZIP archive
rm -f "${DESTROOT}.zip"
pushd "${OUTPUT_DEST}" >/dev/null
    zip --symlinks -r "${PRODUCT_DIR}.zip" "${PRODUCT_DIR}"
popd >/dev/null
