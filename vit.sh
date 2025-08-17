#!/bin/sh
# vit.sh - Run vit without rebuilding

set -e

# Check if binary exists
if [ ! -f "$(dirname "$0")/build/vit" ]; then
    echo "Vit not built yet. Please run: ./install.sh"
    exit 1
fi

# Just run the binary
exec "$(dirname "$0")/build/vit" "$@"