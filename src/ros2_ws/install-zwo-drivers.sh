#!/bin/bash
set -e

# ZWO ASI Camera Driver Installation Script
# This script installs the ZWO ASI SDK on Raspberry Pi

echo "=========================================="
echo "ZWO ASI Camera Driver Setup"
echo "=========================================="
echo ""

# Check if already installed
if [ -f /usr/local/lib/libASICamera2.so ]; then
    echo "ZWO SDK is already installed at /usr/local/lib/libASICamera2.so"
    echo "Reloading drivers..."
    sudo modprobe -r usbfs 2>/dev/null || true
    sudo modprobe usbfs 2>/dev/null || true
    exit 0
fi

echo "This script will download and install the ZWO ASI SDK."
echo "You will need sudo access."
echo ""

# Install dependencies
echo "Installing dependencies..."
sudo apt update
sudo apt install -y \
    libusb-1.0-0 \
    libusb-1.0-0-dev \
    curl \
    build-essential \
    wget

# Download ZWO SDK
echo "Downloading ZWO ASI SDK..."
cd /tmp

# Check architecture
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ]; then
    echo "Detected ARM64 architecture"
    # Try multiple URLs for ARM64
    URLS=(
        "https://www.zwoastro.com/downloads/accessories/asi_camera_linux_sdk_for_armv8.tar.bz2"
        "http://www.zwoastro.com/downloads/accessories/asi_camera_linux_sdk_for_armv8.tar.bz2"
    )
elif [ "$ARCH" = "armv7l" ]; then
    echo "Detected ARM32 architecture"
    # Try multiple URLs for ARM32
    URLS=(
        "https://www.zwoastro.com/downloads/accessories/asi_camera_linux_sdk_v1.36_for_armhf.tar.bz2"
        "http://www.zwoastro.com/downloads/accessories/asi_camera_linux_sdk_v1.36_for_armhf.tar.bz2"
    )
else
    echo "WARNING: Unsupported architecture: $ARCH"
    echo "Please visit http://www.zwoastro.com/support/download and download manually."
    exit 1
fi

# Try to download from multiple URLs
DOWNLOADED=0
for URL in "${URLS[@]}"; do
    echo "Trying: $URL"
    if wget "$URL" -O asi_camera_sdk.tar.bz2 --timeout=30 2>/dev/null; then
        # Verify it's actually a tar file
        if tar -tjf asi_camera_sdk.tar.bz2 >/dev/null 2>&1; then
            echo "Download and verification successful!"
            DOWNLOADED=1
            break
        else
            echo "  Downloaded file is not a valid tar.bz2 - trying next URL..."
            rm -f asi_camera_sdk.tar.bz2
        fi
    fi
done

if [ $DOWNLOADED -eq 0 ]; then
    echo ""
    echo "ERROR: Could not download ZWO ASI SDK from automatic URLs."
    echo ""
    echo "Please download manually:"
    echo "  1. Visit: http://www.zwoastro.com/support/download"
    echo "  2. Download the appropriate version for your CPU:"
    if [ "$ARCH" = "aarch64" ]; then
        echo "     - 'ASI Camera Linux SDK (armv8)' for ARM64"
    else
        echo "     - 'ASI Camera Linux SDK (armhf)' for ARM32"
    fi
    echo "  3. Extract: tar -xjf asi_camera_*.tar.bz2"
    echo "  4. Install: cd asi_camera_linux_sdk* && sudo make -C libusbwrapper install"
    echo "  5. Register: sudo ldconfig"
    exit 1
fi

# Extract the SDK
if ! tar -xjf asi_camera_sdk.tar.bz2; then
    echo "ERROR: Failed to extract SDK archive."
    exit 1
fi

# Find the SDK directory
SDK_DIR=$(find . -maxdepth 2 -type d -name "*asi*" | head -1)
if [ -z "$SDK_DIR" ]; then
    SDK_DIR="."
fi

echo "Installing SDK from $SDK_DIR..."
cd "$SDK_DIR"

# Install the library
if [ -f "libusbwrapper/Makefile" ]; then
    sudo make -C libusbwrapper install
else
    echo "ERROR: libusbwrapper/Makefile not found in SDK"
    exit 1
fi

# Run ldconfig
sudo ldconfig

echo ""
echo "=========================================="
echo "Installation complete!"
echo "=========================================="

# Verify installation
if [ -f /usr/local/lib/libASICamera2.so ]; then
    echo "✓ libASICamera2.so installed successfully"
else
    echo "WARNING: libASICamera2.so not found after installation"
fi

# Check if camera is now visible
echo ""
echo "Checking for camera devices..."
for device in /dev/video{0,19,20,21,22,23,24,25}; do
    if [ -e "$device" ]; then
        echo "  Found: $device"
    fi
done

# Clean up
cd /tmp
rm -rf asi_camera_sdk.tar.bz2 asi-camera-sdk-* 2>/dev/null || true

echo ""
echo "You may need to reboot for the camera to be fully recognized:"
echo "  sudo reboot"

