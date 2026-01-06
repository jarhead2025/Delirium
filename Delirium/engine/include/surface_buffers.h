#pragma once

#include <stdint.h>

#include <cstring>

#include "object_descriptor_buffers.h"  // Get the canonical buffer definitions



// =============================================================================

// ENUMS (Must be consistent with all other files)

// =============================================================================



// Base equation types (6 bits = 64 types max)

enum SurfaceEquationType : uint8_t {

    EQUATION_PLANE = 0, EQUATION_SPHERE = 1, EQUATION_CYLINDER = 2, EQUATION_CONE = 3,

    EQUATION_TORUS = 4, EQUATION_ELLIPSOID = 5, EQUATION_PARABOLOID = 6, EQUATION_HYPERBOLOID = 7,

    EQUATION_BILINEAR = 16, EQUATION_BIQUADRATIC = 17, EQUATION_BICUBIC = 18,

    EQUATION_BEZIER_PATCH = 32, EQUATION_BSPLINE = 33, EQUATION_NURBS = 34,

    EQUATION_HEIGHTFIELD = 56, EQUATION_PERLIN_NOISE = 57, EQUATION_FRACTAL = 58,

    EQUATION_WAVE = 59, EQUATION_METABALL = 60

};



// Combination operations (4 bits = 16 types max)

enum CombinationOperation : uint8_t {

    COMBO_NONE = 0, COMBO_BLEND = 1, COMBO_ADD = 2, COMBO_MULTIPLY = 3,

    COMBO_CSG_UNION = 4, COMBO_CSG_INTERSECT = 5, COMBO_CSG_SUBTRACT = 6,

    COMBO_MORPH = 7, COMBO_SMOOTH_MIN = 11, COMBO_SMOOTH_MAX = 12, COMBO_LAYER = 15

};



// Surface modifiers (6 bits = 64 combinations max)

enum SurfaceModifier : uint8_t {

    MOD_NONE = 0, MOD_TWIST = (1 << 0), MOD_BEND = (1 << 1), MOD_TAPER = (1 << 2),

    MOD_NOISE = (1 << 3), MOD_ANIMATE = (1 << 4), MOD_MIRROR = (1 << 5)

};



// Forward-declare from mathematical_color_evaluator.h

enum ColorEquationType : uint8_t;



// =============================================================================

// UTILITY FUNCTIONS

// =============================================================================



// Bit packing/unpacking helpers

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

// SURFACE BUFFER UTILITIES

// =============================================================================



class SurfaceBufferUtils {

public:

    static ProceduralSurfaceBuffer CreateSingle(SurfaceEquationType type, 

                                              const float params[8], 

                                              uint8_t modifiers = MOD_NONE);

    

    static ProceduralSurfaceBuffer CreateCombination(

        SurfaceEquationType primaryType, const float primaryParams[4],

        SurfaceEquationType secondaryType, const float secondaryParams[4],

        CombinationOperation operation, float blendWeight = 0.5f, 

        uint8_t modifiers = MOD_NONE);

    

    static const char* GetSurfaceDescription(const ProceduralSurfaceBuffer* buffer);

    static bool ValidateBuffer(const ProceduralSurfaceBuffer* buffer);

};
