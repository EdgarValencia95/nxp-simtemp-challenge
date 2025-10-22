#!/bin/bash
#
# cleanup.sh - Cleanup script for NXP Simtemp Challenge
#
# Removes all compiled files and kernel module
#

echo "=========================================="
echo "NXP Simtemp Challenge - Cleanup"
echo "=========================================="
echo ""

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}➜ $1${NC}"
}

# Unload module if loaded
if lsmod | grep -q nxp_simtemp; then
    print_info "Unloading kernel module..."
    if [ "$EUID" -eq 0 ]; then
        rmmod nxp_simtemp 2>/dev/null && print_success "Module unloaded"
    else
        sudo rmmod nxp_simtemp 2>/dev/null && print_success "Module unloaded"
    fi
fi

# Clean kernel
if [ -d "kernel" ]; then
    print_info "Cleaning kernel build..."
    cd kernel
    make clean > /dev/null 2>&1
    print_success "Kernel build cleaned"
    cd ..
fi

# Clean CLI
if [ -d "cli" ]; then
    print_info "Cleaning CLI build..."
    cd cli
    make clean > /dev/null 2>&1
    print_success "CLI build cleaned"
    cd ..
fi

# Clean userspace tests
if [ -d "userspace" ]; then
    print_info "Cleaning userspace tests..."
    cd userspace
    rm -f test_simtemp test_simtemp_blocking test_simtemp_buffered test_simtemp_poll
    print_success "Userspace tests cleaned"
    cd ..
fi

# Remove backup files
print_info "Removing backup files..."
find . -name "*.bak" -delete 2>/dev/null
find . -name "*.phase*" -delete 2>/dev/null
find . -name "*~" -delete 2>/dev/null
print_success "Backup files removed"

echo ""
echo "=========================================="
print_success "Cleanup completed!"
echo "=========================================="
