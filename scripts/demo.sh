#!/bin/bash
#
# demo.sh - Demonstration script for NXP Simtemp Challenge
#
# This script loads the kernel module and runs various demonstrations
#

set -e

echo "=========================================="
echo "NXP Simtemp Challenge - Demo Script"
echo "=========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}➜ $1${NC}"
}

print_demo() {
    echo -e "${BLUE}═══ $1 ═══${NC}"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    print_error "This script must be run as root (use sudo)"
    exit 1
fi

# Check if module is built
if [ ! -f "kernel/nxp_simtemp.ko" ]; then
    print_error "Kernel module not found. Run ./build.sh first"
    exit 1
fi

# Check if CLI is built
if [ ! -f "cli/simtemp_cli" ]; then
    print_error "CLI application not found. Run ./build.sh first"
    exit 1
fi

# Unload module if already loaded
if lsmod | grep -q nxp_simtemp; then
    print_info "Unloading existing module..."
    rmmod nxp_simtemp 2>/dev/null || true
fi

# Load kernel module
print_info "Loading kernel module..."
if insmod kernel/nxp_simtemp.ko; then
    print_success "Module loaded successfully"
else
    print_error "Failed to load module"
    exit 1
fi

# Wait for device to be ready
sleep 1

# Check if device exists
if [ ! -e /dev/simtemp ]; then
    print_error "Device /dev/simtemp not found"
    rmmod nxp_simtemp
    exit 1
fi

print_success "Device /dev/simtemp ready"
echo ""

# Display module information
print_demo "Module Information"
modinfo kernel/nxp_simtemp.ko | grep -E "filename|description|author|version"
echo ""

# Show kernel logs
print_demo "Kernel Initialization Logs"
dmesg | grep simtemp | tail -10
echo ""
sleep 2

# Demo 1: Basic table output
print_demo "Demo 1: Basic Temperature Reading (Table Format)"
echo ""
./cli/simtemp_cli -n 10
echo ""
sleep 2

# Demo 2: Statistics
print_demo "Demo 2: Temperature Reading with Statistics"
echo ""
./cli/simtemp_cli -n 20 -s
echo ""
sleep 2

# Demo 3: JSON output
print_demo "Demo 3: JSON Output Format"
echo ""
./cli/simtemp_cli -n 5 -f json
echo ""
sleep 2

# Demo 4: CSV output
print_demo "Demo 4: CSV Output Format"
echo ""
./cli/simtemp_cli -n 5 -f csv
echo ""
sleep 2

# Demo 5: Continuous mode (10 seconds)
print_demo "Demo 5: Continuous Monitoring (10 seconds, press Ctrl+C to stop)"
echo ""
timeout 10 ./cli/simtemp_cli -c -s || true
echo ""
sleep 1

# Show final kernel stats
print_demo "Final Kernel Statistics"
dmesg | grep simtemp | tail -5
echo ""

echo "=========================================="
print_success "Demo completed successfully!"
echo "=========================================="
echo ""
echo "Module is still loaded. To unload:"
echo "  sudo rmmod nxp_simtemp"
echo ""
echo "To run CLI manually:"
echo "  sudo ./cli/simtemp_cli --help"
echo ""
