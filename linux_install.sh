#!/bin/bash

PROJECT_DIR=$(pwd)
EXECUTABLE_PATH="${PROJECT_DIR}/cmake-build-debug/src/TypeIt"
SYMLINK_TARGET="/usr/games/TypeIt"

echo "Checking dependencies..."

if ! command -v git &> /dev/null; then
    echo "Git is not installed. Installing Git..."
    sudo apt-get update
    sudo apt-get install -y git
else
    echo "Git is already installed."
fi

if ! command -v cmake &> /dev/null; then
    echo "CMake is not installed. Installing CMake..."
    sudo apt-get install -y cmake
else
    CMAKE_VERSION=$(cmake --version | awk 'NR==1{print $3}')
    MIN_VERSION="3.25.0"
    if [ "$(printf '%s\n' "$MIN_VERSION" "$CMAKE_VERSION" | sort -V | head -n1)" != "$MIN_VERSION" ]; then
        echo "Upgrading CMake (requires ≥ $MIN_VERSION)..."
        sudo apt-get install -y --only-upgrade cmake
    fi
fi

if ! command -v ninja &> /dev/null; then
    echo "Ninja is not installed. Installing Ninja..."
    sudo apt-get install -y ninja-build
else
    echo "Ninja is already installed."
fi

echo "Initializing and updating Git submodules..."
git submodule update --init --recursive

echo "Running CMake configuration using linux-gcc preset..."
# Let CMake handle build directory via preset
cmake --preset linux-gcc

echo "Building the project..."
cmake --build --preset linux-gcc

read -p "Do you want to create a symlink to the executable? (Y/n): " user_response

if [[ "$user_response" =~ (^[Yy]$|^$) ]]; then
    echo "Creating symlink..."
    sudo ln -sfv "${EXECUTABLE_PATH}" "${SYMLINK_TARGET}"
    echo "Symlink created successfully."
else
    echo "Skipping symlink creation."
fi

echo "Linux installation and build completed successfully."
echo "Run game with command TypeIt"