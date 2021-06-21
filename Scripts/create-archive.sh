#!/bin/sh
set -e

# Creates arcive for publishing. Should be run from build results directory.
# Usage: create-archive.sh <result-name> <list-of-content>

PROJECT_DIR="$(dirname "$0")/.."

# Create temporary directory
TEMP_PRODUCT_NAME="${PRODUCT_NAME:-`echo $1 | cut -d'-' -f 1`}"
TEMP_DIR=$(mktemp -d -t "$TEMP_PRODUCT_NAME")
TEMP_PATH_TO_ARCH="$TEMP_DIR/$TEMP_PRODUCT_NAME"
mkdir -p "$TEMP_PATH_TO_ARCH"

# Copy required files
cp "$PROJECT_DIR/LICENSE" "$TEMP_PATH_TO_ARCH/LICENSE.txt"
cp -R "$PROJECT_DIR/Documentation" "$TEMP_PATH_TO_ARCH"
(cd "$TEMP_PATH_TO_ARCH" && ln -sf "Documentation/index.html" "API Documentation.html")
cp -R "${@:2}" "$TEMP_PATH_TO_ARCH"

# Archive content
rm -f "$TEMP_DIR/$TEMP_PRODUCT_NAME.zip"
(cd "$TEMP_DIR" && zip -ryq9 "$1.zip" "$TEMP_PRODUCT_NAME")
mv "$TEMP_DIR/$1.zip" .

# Remove temporary directory
rm -rf "$TEMP_DIR"
