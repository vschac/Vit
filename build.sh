#!/bin/bash
set -e

echo "Building Vit..."

cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build ./build --config Release

echo "Build complete!"
echo "Usage: ./vit <command>"