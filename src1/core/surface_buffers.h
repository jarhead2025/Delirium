// =============================================================================
// SURFACE BUFFERS HEADER - Terry-Bell
// 2025-07-01 18:14:22
// =============================================================================

#pragma once
#include <vector>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdio>

// Include object descriptor buffers first
#include "object_descriptor_buffers.h"

// =============================================================================
// ENUMS DEFINITION - SELF-CONTAINED
// =============================================================================

enum SurfaceEquationType : uint8_t {
    EQUATION_PLANE = 0, EQUATION_SPHERE = 1, EQUATION_CYLINDER = 2, EQUATION_CONE = 3,
    EQUATION_TORUS = 4, EQUATION_ELLIPSOID = 5, EQUATION_PARABOLOID = 6, EQUATION_HYPERBOLOID = 7,
    EQUATION_BILINEAR = 16, EQUATION_BIQUADRATIC = 17, EQUATION_BICUBIC = 18,
    EQUATION_BEZIER_PATCH = 32, EQUATION_BSPLINE = 33, EQUATION_NURBS = 34,
    EQUATION_HEIGHTFIELD = 56, EQUATION_PERLIN_NOISE = 57, EQUATION_FRACTAL = 58,
    EQUATION_WAVE = 59, EQUATION_METABALL = 60
};

enum CombinationOperation : uint8_t {
    COMBO_NONE = 0, COMBO_BLEND = 1, COMBO_ADD = 2, COMBO_MULTIPLY = 3,
    COMBO_CSG_UNION = 4, COMBO_CSG_INTERSECT = 5, COMBO_CSG_SUBTRACT = 6,
    COMBO_MORPH = 7, COMBO_SMOOTH_MIN = 11, COMBO_SMOOTH_MAX = 12, COMBO_LAYER = 15
};

enum SurfaceModifier : uint8_t {
    MOD_NONE = 0, MOD_TWIST = (1 << 0), MOD_BEND = (1 << 1), MOD_TAPER = (1 << 2),
    MOD_NOISE = (1 << 3), MOD_ANIMATE = (1 << 4), MOD_MIRROR = (1 << 5)
};

// =============================================================================
// UTILITY FUNCTIONS - INLINE IMPLEMENTATION
// =============================================================================

// New function names (for internal use in this file)
inline SurfaceEquationType GetSurfaceTypeFromIndex(uint16_t packedIndex) { 
    return static_cast<SurfaceEquationType>(packedIndex & 0x003F); 
}

inline CombinationOperation GetCombinationFromIndex(uint16_t packedIndex) { 
    return static_cast<CombinationOperation>((packedIndex >> 6) & 0x000F); 
}

inline uint8_t GetModifiersFromIndex(uint16_t packedIndex) { 
    return static_cast<uint8_t>((packedIndex >> 10) & 0x003F); 
}

// Original function names (for backward compatibility)
inline SurfaceEquationType ExtractBaseType(uint16_t packedIndex) { 
    return static_cast<SurfaceEquationType>(packedIndex & 0x003F); 
}

inline CombinationOperation ExtractCombination(uint16_t packedIndex) { 
    return static_cast<CombinationOperation>((packedIndex >> 6) & 0x000F); 
}

inline uint8_t ExtractModifiers(uint16_t packedIndex) { 
    return static_cast<uint8_t>((packedIndex >> 10) & 0x003F); 
}

inline uint16_t PackSurfaceIndex(SurfaceEquationType baseType, CombinationOperation combo, uint8_t mods) {
    return static_cast<uint16_t>(baseType) | 
           (static_cast<uint16_t>(combo) << 6) | 
           (static_cast<uint16_t>(mods & 0x3F) << 10);
}

// =============================================================================
// SURFACE BUFFER UTILITIES CLASS - DECLARATIONS ONLY
// =============================================================================

class SurfaceBufferUtils {
public:
    // Create a single surface
    static ProceduralSurfaceBuffer CreateSingle(
        SurfaceEquationType type, 
        const std::vector<float>& params, 
        uint8_t modifiers = 0);

    // Create a combination of surfaces
    static ProceduralSurfaceBuffer CreateCombination(
        SurfaceEquationType primaryType, 
        const std::vector<ProceduralSurfaceBuffer*>& buffers,
        CombinationOperation combo, 
        uint8_t modifiers = 0);

    // Create a surface with anchor point references (vector version)
    static ProceduralSurfaceBuffer CreateWithAnchors(
        SurfaceEquationType type, 
        const std::vector<int>& anchorIndices, 
        const std::vector<float>& params, 
        uint8_t modifiers = 0);

    // Create a surface with anchor point references (raw array version)
    static ProceduralSurfaceBuffer CreateWithAnchors(
        SurfaceEquationType type, 
        const uint16_t anchorIndices[4], 
        const float params[8], 
        uint8_t modifiers = 0);

    // Extract parameters from a buffer
    static std::vector<float> ExtractParameters(const ProceduralSurfaceBuffer* buffer);

    // Extract the base type from a buffer
    static SurfaceEquationType ExtractBaseType(const ProceduralSurfaceBuffer* buffer);

    // Extract the combination operation from a buffer
    static CombinationOperation ExtractCombination(const ProceduralSurfaceBuffer* buffer);

    // Extract the modifiers from a buffer
    static uint8_t ExtractModifiers(const ProceduralSurfaceBuffer* buffer);

    // Create a metaball surface
    static ProceduralSurfaceBuffer CreateMetaball(const std::vector<float>& params);
};