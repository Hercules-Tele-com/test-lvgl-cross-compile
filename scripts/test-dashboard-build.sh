#!/bin/bash
# Test dashboard compilation for both Windows and Raspberry Pi

set -e  # Exit on error

cd "$(dirname "$0")/../ui-dashboard"

echo "================================"
echo "Dashboard Build Test"
echo "================================"
echo ""

# Detect platform
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    PLATFORM="Windows"
    CMAKE_EXTRA="-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -A x64"
else
    PLATFORM="Linux/Raspberry Pi"
    CMAKE_EXTRA=""
fi

echo "Platform: $PLATFORM"
echo ""

# Clean previous build
echo "Cleaning previous build..."
rm -rf build
mkdir build
cd build

echo ""
echo "Configuring CMake..."
echo "  Battery: EMBOO (250 kbps)"
echo "  Motor: ROAM (RM100)"
echo ""

cmake .. \
    -DBATTERY_TYPE=EMBOO \
    -DMOTOR_TYPE=ROAM \
    $CMAKE_EXTRA

echo ""
echo "Building..."
echo ""

if [[ "$PLATFORM" == "Windows" ]]; then
    cmake --build . --config Release
    BINARY="Release/leaf-can-dashboard.exe"
else
    make -j$(nproc)
    BINARY="leaf-can-dashboard"
fi

echo ""
echo "================================"
echo "Build Status"
echo "================================"

if [ -f "$BINARY" ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Binary: build/$BINARY"
    echo "Size: $(du -h "$BINARY" | cut -f1)"
    echo ""
    echo "Configuration:"
    echo "  - Battery: EMBOO (Orion BMS, 250 kbps)"
    echo "  - Motor: ROAM/RM100"
    echo "  - Platform: $PLATFORM"
    echo ""

    if [[ "$PLATFORM" == "Linux/Raspberry Pi" ]]; then
        echo "To run:"
        echo "  sudo ./build/leaf-can-dashboard"
    else
        echo "To run:"
        echo "  ./build/Release/leaf-can-dashboard.exe"
    fi

    exit 0
else
    echo "✗ Build failed!"
    echo "Binary not found: $BINARY"
    exit 1
fi
