#!/bin/bash

# Install dependencies (Ubuntu/Debian)
# sudo apt-get install libcurl4-openssl-dev nlohmann-json3-dev

# Or use vcpkg
# vcpkg install curl nlohmann-json

mkdir -p build
cd build

cmake .. -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
make

echo "Built examples:"
echo "  ./openai_example"
echo "  ./ollama_example"