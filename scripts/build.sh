#!/bin/bash
#
# build.sh - Build script for NXP Simtemp Challenge
#
# This script compiles both the kernel module and CLI application
#

set -e  # Exit on any error

echo "=========================================="
echo "NXP Simtemp Challenge - Build Script"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored messages
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}➜ $1${NC}"
}

# Check if running from project root
if [ ! -d "kernel" ] || [ ! -d "cli" ]; then
    print_error "This script must be run from the project root directory"
    exit 1
fi

# Build kernel module
print_info "Building kernel module..."
cd kernel
make clean > /dev/null 2>&1
if make; then
    print_success "Kernel module built successfully"
    ls -lh nxp_simtemp.ko
else
    print_error "Kernel module build failed"
    exit 1
fi
cd ..

echo ""

# Build CLI application
print_info "Building CLI application..."
cd cli
make clean > /dev/null 2>&1
if make; then
    print_success "CLI application built successfully"
    ls -lh simtemp_cli
else
    print_error "CLI application build failed"
    exit 1
fi
cd ..

echo ""

# Build userspace test programs (optional)
if [ -d "userspace" ]; then
    print_info "Building userspace test programs..."
    cd userspace
    
    for src in test_simtemp*.c; do
        if [ -f "$src" ]; then
            binary="${src%.c}"
            if gcc -o "$binary" "$src" 2>/dev/null; then
                print_success "Built $binary"
            fi
        fi
    done
    cd ..
    echo ""
fi

echo "=========================================="
print_success "Build completed successfully!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  1. Load kernel module:  sudo ./scripts/load_module.sh"
echo "  2. Run demo:            sudo ./scripts/demo.sh"
echo "  3. Or use CLI directly: sudo ./cli/simtemp_cli --help"
echo ""
