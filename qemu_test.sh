#!/bin/bash
# QEMU Test Script for ANCS + A2DP + GATTS Coexistence Example

set -e

echo "========================================"
echo "ESP-IDF ANCS + A2DP + GATTS QEMU Test"
echo "========================================"
echo ""

# Check ESP-IDF
if [ -z "$IDF_PATH" ]; then
    export IDF_PATH=/root/esp/esp-idf
fi

echo "ESP-IDF Path: $IDF_PATH"
echo "Project Path: $(pwd)"
echo ""

# Check QEMU
if ! command -v qemu-system-xtensa &> /dev/null; then
    echo "❌ QEMU not found!"
    exit 1
fi

QEMU_VERSION=$(qemu-system-xtensa --version | head -1)
echo "✅ QEMU found: $QEMU_VERSION"
echo ""

# Step 1: Full clean
echo "Step 1: Cleaning build directory..."
rm -rf build/
echo "✅ Clean complete"
echo ""

# Step 2: Set target and sdkconfig
echo "Step 2: Setting target to ESP32..."
python3 $IDF_PATH/tools/idf.py set-target esp32 2>&1 | tail -5
echo "✅ Target set"
echo ""

# Step 3: Build
echo "Step 3: Building project..."
python3 $IDF_PATH/tools/idf.py build 2>&1 | tail -20

# Check if build succeeded
if [ ! -f "build/ancs_a2dp_gatts_coex.elf" ]; then
    echo "❌ Build failed - ELF file not found!"
    exit 1
fi

echo "✅ Build successful!"
echo ""

# Step 4: Check binary size
echo "Step 4: Build artifacts..."
ls -lh build/*.elf build/*.bin 2>/dev/null | head -10
echo ""

# Step 5: Run QEMU
echo "Step 5: Running in QEMU..."
echo "Note: QEMU will run for 10 seconds then exit"
echo ""

# Create a simple test to verify the firmware runs
timeout 10 qemu-system-xtensa \
    -machine esp32 \
    -drive file=build/ancs_a2dp_gatts_coex.bin,if=mtd,format=raw \
    -nographic \
    -serial stdio 2>&1 | head -50 || true

echo ""
echo "========================================"
echo "QEMU Test Complete"
echo "========================================"
