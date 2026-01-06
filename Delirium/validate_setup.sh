#!/bin/bash
# =============================================================================
# validate_setup.sh - Verify all required files are in place
# Location: DeliRium_Local/validate_setup.sh
# =============================================================================

set +e

echo "==================================================="
echo "DeliRium Engine - Setup Validation"
echo "==================================================="
echo ""

MISSING_COUNT=0
PROVIDED_COUNT=0
EXISTING_COUNT=0

check_file() {
    local file="$1"
    local type="$2"
    
    if [ -f "$file" ]; then
        echo "✓ $file"
        if [ "$type" == "PROVIDED" ]; then
            ((PROVIDED_COUNT++))
        else
            ((EXISTING_COUNT++))
        fi
        return 0
    else
        echo "✗ MISSING: $file"
        ((MISSING_COUNT++))
        return 1
    fi
}

echo "Checking ROOT files..."
check_file "CMakeLists.txt" "PROVIDED"
check_file ".gitignore" "PROVIDED"
echo ""

echo "Checking ENGINE HEADERS..."
check_file "engine/include/delirium/Transform.h" "PROVIDED"
check_file "engine/include/delirium/GameObject.h" "PROVIDED"
check_file "engine/include/delirium/Scene.h" "PROVIDED"
check_file "engine/include/delirium/Engine.h" "PROVIDED"
check_file "engine/include/delirium/Renderer.h" "PROVIDED"
check_file "engine/include/delirium/PhysicsWorld.h" "PROVIDED"
check_file "engine/include/delirium/ProceduralObject.h" "PROVIDED"
check_file "engine/include/delirium/AssetManager.h" "PROVIDED"
echo ""

echo "Checking YOUR EXISTING HEADERS..."
check_file "engine/include/delirium/math_types.h" "EXISTING"
check_file "engine/include/delirium/object_descriptor_buffers.h" "EXISTING"
check_file "engine/include/delirium/surface_buffer_evaluator.h" "EXISTING"
check_file "engine/include/delirium/mathematical_color_evaluator.h" "EXISTING"
check_file "engine/include/delirium/bone_evaluator.h" "EXISTING"
check_file "engine/include/delirium/anchorpoint_evaluator.h" "EXISTING"
check_file "engine/include/delirium/surface_buffers.h" "EXISTING"
echo ""

echo "Checking ENGINE SOURCE - CORE..."
check_file "engine/src/core/Transform.cpp" "PROVIDED"
check_file "engine/src/core/GameObject.cpp" "PROVIDED"
check_file "engine/src/core/Scene.cpp" "PROVIDED"
check_file "engine/src/core/Engine.cpp" "PROVIDED"
echo ""

echo "Checking YOUR EXISTING SOURCE..."
check_file "engine/src/core/surface_buffer_evaluator.cpp" "EXISTING"
check_file "engine/src/core/mathematical_color_evaluator.cpp" "EXISTING"
check_file "engine/src/core/bone_evaluator.cpp" "EXISTING"
check_file "engine/src/core/anchorpoint_evaluator.cpp" "EXISTING"
echo ""

echo "Checking ENGINE SOURCE - ASSETS..."
check_file "engine/src/assets/ProceduralObject.cpp" "PROVIDED"
check_file "engine/src/assets/AssetManager.cpp" "PROVIDED"
echo ""

echo "Checking ENGINE SOURCE - PHYSICS..."
check_file "engine/src/physics/PhysicsWorld.cpp" "PROVIDED"
echo ""

echo "Checking MAIN PROGRAM..."
check_file "engine/src/main.cpp" "PROVIDED"
echo ""

echo "==================================================="
echo "VALIDATION SUMMARY"
echo "==================================================="
echo "Provided files found: $PROVIDED_COUNT / 17"
echo "Your files found: $EXISTING_COUNT / 11"
echo "Missing files: $MISSING_COUNT"
echo ""

if [ $MISSING_COUNT -eq 0 ]; then
    echo "✓✓✓ ALL FILES IN PLACE! ✓✓✓"
    echo ""
    echo "You can now build the engine:"
    echo "  cd build"
    echo "  cmake .."
    echo "  make -j8"
    echo "  ./delirium"
    echo ""
    exit 0
else
    echo "✗✗✗ SETUP INCOMPLETE ✗✗✗"
    echo ""
    echo "Missing $MISSING_COUNT file(s)."
    echo ""
    exit 1
fi