#!/bin/bash

# Build the docker image
docker build -t neko-void-build . || { echo "Docker build failed"; exit 1; }

# Run the build
docker run --rm -v "$(pwd):/app" neko-void-build || { echo "Compilation failed"; exit 1; }

echo "------------------------------------------------"
echo "Build finished. Check the 'build' directory."
echo "Note: To run the UI, you would need to export X11/Wayland to the container,"
echo "but for now, this confirms the code compiles correctly for Void Linux."
echo "------------------------------------------------"