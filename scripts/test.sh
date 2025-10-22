#!/bin/bash
#
# test.sh - Test suite for NXP Simtemp Challenge
#

set -e

echo "=========================================="
echo "NXP Simtemp Challenge - Test Suite"
echo "=========================================="
echo ""

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0

print_success() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((TESTS_PASSED++))
}

print_error() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((TESTS_FAILED++))
}

print_info() {
    echo -e "${YELLOW}➜ TEST${NC}: $1"
}

# Check root
if [ "$EUID" -ne 0 ]; then 
    echo "This script must be run as root"
    exit 1
fi

# Ensure module is loaded
if ! lsmod | grep -q nxp_simtemp; then
    echo "Loading module..."
    insmod kernel/nxp_simtemp.ko || exit 1
    sleep 1
fi

echo "Running tests..."
echo ""

# Test 1: Device exists
print_info "Device node exists"
if [ -e /dev/simtemp ]; then
    print_success "Device /dev/simtemp exists"
else
    print_error "Device /dev/simtemp not found"
fi

# Test 2: Can read from device
print_info "Can read from device"
if dd if=/dev/simtemp bs=16 count=1 of=/dev/null 2>/dev/null; then
    print_success "Successfully read from device"
else
    print_error "Failed to read from device"
fi

# Test 3: CLI runs
print_info "CLI application runs"
if timeout 5 ./cli/simtemp_cli -n 5 > /dev/null 2>&1; then
    print_success "CLI executed successfully"
else
    print_error "CLI execution failed"
fi

# Test 4: JSON output valid
print_info "JSON output format"
if ./cli/simtemp_cli -n 1 -f json 2>/dev/null | grep -q "temperature_C"; then
    print_success "JSON output valid"
else
    print_error "JSON output invalid"
fi

# Test 5: CSV output valid
print_info "CSV output format"
if ./cli/simtemp_cli -n 1 -f csv 2>/dev/null | grep -q "Temperature_C"; then
    print_success "CSV output valid"
else
    print_error "CSV output invalid"
fi

# Test 6: Multiple reads
print_info "Multiple sequential reads"
if ./cli/simtemp_cli -n 50 > /dev/null 2>&1; then
    print_success "50 samples read successfully"
else
    print_error "Multiple reads failed"
fi

# Test 7: Module info
print_info "Module information accessible"
if modinfo kernel/nxp_simtemp.ko > /dev/null 2>&1; then
    print_success "Module info accessible"
else
    print_error "Module info failed"
fi

echo ""
echo "=========================================="
echo "Test Results"
echo "=========================================="
echo -e "Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Failed: ${RED}$TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed${NC}"
    exit 1
fi
