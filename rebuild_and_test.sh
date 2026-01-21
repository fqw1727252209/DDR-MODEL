#!/bin/bash

# LPDDR5 AC Timing Checker - Rebuild and Test Script
# This script rebuilds the project and runs the AC timing tests

echo "========================================="
echo "LPDDR5 AC Timing Checker - Rebuild Script"
echo "========================================="

# Step 1: Clean build directory
echo ""
echo "[Step 1/5] Cleaning build directory..."
cd build
rm -rf *

# Step 2: Run CMake
echo ""
echo "[Step 2/5] Running CMake configuration..."
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed!"
    exit 1
fi

# Step 3: Compile
echo ""
echo "[Step 3/5] Compiling project (this may take a few minutes)..."
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "ERROR: Compilation failed!"
    exit 1
fi

# Step 4: Set library path
echo ""
echo "[Step 4/5] Setting library path..."
export LD_LIBRARY_PATH=/opt/systemc-2/lib:$LD_LIBRARY_PATH

# Step 5: Run tests
echo ""
echo "[Step 5/5] Running AC Timing tests..."
cd bin
./dmutest AC_TIMING_TEST ../../lib/DRAMsys/configs/lpddr5-example.json

echo ""
echo "========================================="
echo "Test execution completed!"
echo "Check the latest log file in build/bin/logs/"
echo "========================================="
