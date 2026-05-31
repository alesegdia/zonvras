#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

echo "==> Build type: $BUILD_TYPE"

mkdir -p "$BUILD_DIR"

echo "==> Installing Conan dependencies..."
conan install . \
    --output-folder="$BUILD_DIR" \
    --build=missing \
    -s build_type="$BUILD_TYPE"

echo "==> Configuring CMake..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$BUILD_DIR/conan_toolchain.cmake" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "==> Building..."
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel

echo ""
echo "Done. Run with:"
echo "  $BUILD_DIR/zonvras          (Linux/macOS)"
echo "  $BUILD_DIR/$BUILD_TYPE/zonvras.exe  (Windows)"
