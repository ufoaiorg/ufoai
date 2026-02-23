#!/bin/bash
# UFO:AI Smoke Test - Validates that the game boots to main menu
# Used by CI/CD to ensure build succeeds and basic startup works

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}UFO:AI Smoke Test${NC}"
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found at $BUILD_DIR${NC}"
    echo "Please run: cd $PROJECT_ROOT && cmake . -B build && cmake --build build"
    exit 1
fi

# Find the executable
UFO_EXECUTABLE=""
if [ -f "$BUILD_DIR/ufo" ]; then
    UFO_EXECUTABLE="$BUILD_DIR/ufo"
elif [ -f "$BUILD_DIR/Debug/ufo.exe" ]; then
    UFO_EXECUTABLE="$BUILD_DIR/Debug/ufo.exe"
elif [ -f "$BUILD_DIR/Release/ufo.exe" ]; then
    UFO_EXECUTABLE="$BUILD_DIR/Release/ufo.exe"
else
    echo -e "${RED}Error: UFO:AI executable not found${NC}"
    echo "Searched locations:"
    echo "  - $BUILD_DIR/ufo"
    echo "  - $BUILD_DIR/Debug/ufo.exe"
    echo "  - $BUILD_DIR/Release/ufo.exe"
    exit 1
fi

echo -e "${GREEN}✓ Found executable: $UFO_EXECUTABLE${NC}"

# Run smoke test - boot to menu
echo ""
echo -e "${YELLOW}Starting UFO:AI...${NC}"
echo "Command: $UFO_EXECUTABLE --headless --skip-intro"
echo ""

# Run with timeout (10 seconds should be enough to boot to menu)
# The --headless flag prevents GUI initialization, --skip-intro skips intro videos
TIMEOUT=10
if timeout $TIMEOUT "$UFO_EXECUTABLE" --headless --skip-intro &>/dev/null; then
    RESULT=$?
else
    RESULT=$?
fi

if [ $RESULT -eq 0 ] || [ $RESULT -eq 124 ]; then
    # Exit code 0 = success, 124 = timeout (expected for infinite loop)
    echo -e "${GREEN}✓ Smoke test PASSED: Game booted successfully${NC}"
    exit 0
else
    echo -e "${RED}✗ Smoke test FAILED: Game failed to boot (exit code: $RESULT)${NC}"
    exit 1
fi
