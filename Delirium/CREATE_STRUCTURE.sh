#!/bin/bash
# =============================================================================
# CREATE_STRUCTURE.sh - Create complete directory structure
# Location: DeliRium_Local/CREATE_STRUCTURE.sh
# =============================================================================

set -e

echo "==================================================="
echo "Creating DeliRium Engine Directory Structure"
echo "==================================================="

PROJECT_ROOT="DeliRium_Local"

mkdir -p "$PROJECT_ROOT"
cd "$PROJECT_ROOT"

echo "Creating directory structure..."

mkdir -p engine/src/core
mkdir -p engine/src/assets
mkdir -p engine/src/rendering
mkdir -p engine/src/physics
mkdir -p engine/include/delirium
mkdir -p engine/shaders

mkdir -p assets/models
mkdir -p assets/scenes
mkdir -p assets/textures
mkdir -p assets/test_data

mkdir -p tools/fbx_converter
mkdir -p tools/scripts

mkdir -p tests
mkdir -p docs

mkdir -p experiments/src1
mkdir -p experiments/src2
mkdir -p experiments/src3
mkdir -p experiments/src4

mkdir -p build

find . -type d -empty -exec touch {}/.gitkeep \;

echo "✓ Directory structure created!"
echo ""
echo "==================================================="
echo "Next steps:"
echo "1. Copy the provided source files into engine/src/"
echo "2. Copy the provided headers into engine/include/delirium/"
echo "3. Copy your existing code to engine/include/delirium/"
echo "4. Copy CMakeLists.txt to project root"
echo "5. Copy .gitignore to project root"
echo "6. Run: cd build && cmake .. && make"
echo "==================================================="