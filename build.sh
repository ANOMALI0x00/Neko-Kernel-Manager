#!/bin/bash

# Build script for Neko Kernel Manager

echo "Building Neko Kernel Manager..."

# Build Rust library
cd config-option-lib
cargo build --release
cd ..

# Setup meson
meson setup build --buildtype=release

# Build
meson compile -C build

echo "Build complete. Run ./build/neko-kernel-manager"