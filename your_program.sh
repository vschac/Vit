#!/bin/sh
#
# Use this script to run your vit version control system locally.
#

set -e # Exit early if any commands fail

# Build the project
(
  cd "$(dirname "$0")" # Ensure compile steps are run within the repository directory
  cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
  cmake --build ./build
)

# Run the vit program
exec $(dirname "$0")/build/vit "$@"
