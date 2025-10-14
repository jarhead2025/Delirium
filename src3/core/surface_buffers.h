#ifndef SURFACE_BUFFERS_H
#define SURFACE_BUFFERS_H

#include "object_descriptor_buffers.h" // For ProceduralSurfaceBuffer
#include <cstdint>
#include <vector>
#include <stdexcept>

enum SurfaceEquationType : uint8_t {
    EQUATION_PLANE = 0,
    EQUATION_BILINEAR = 1,
    EQUATION_QUADRATIC = 2,
    EQUATION_METABALL = 3,
    EQUATION_BEZIER_PATCH = 32  // Added this for CreateCube function
};

enum CombinationOperation : uint8_t {
    COMBO_NONE = 0,
    COMBO_BLEND = 1,
    COMBO_SUBTRACT = 2,
    COMBO_UNION = 3
};

enum SurfaceModifier : uint8_t {
    MOD_NONE = 0,
    MOD_ROUGHNESS = 1,
    MOD_COLOR = 2
};

class SurfaceBufferUtils {
public:
    // Fixed function signatures to match implementation
    static ProceduralSurfaceBuffer CreateSingle(SurfaceEquationType type, const std::vector<float>& params, uint8_t modifiers);
    
    // Multiple overloads for CreateWithAnchors
    static ProceduralSurfaceBuffer CreateWithAnchors(SurfaceEquationType type, const uint16_t anchor_indices[4], const float params[8], uint8_t modifiers);
    static ProceduralSurfaceBuffer CreateWithAnchors(SurfaceEquationType type, const std::vector<int>& anchorIndices, const std::vector<float>& params, uint8_t modifiers);
    
    static ProceduralSurfaceBuffer CreateMetaball(const float center[3], float radius, uint8_t modifiers);
    
    // Fixed function signature to match implementation
    static ProceduralSurfaceBuffer CreateCombination(SurfaceEquationType primaryType, const std::vector<ProceduralSurfaceBuffer*>& buffers, CombinationOperation combo, uint8_t modifiers);
    
    // Fixed function signature
    static uint16_t PackSurfaceIndex(SurfaceEquationType type, CombinationOperation combo, uint8_t modifiers);
    
    static SurfaceEquationType ExtractBaseType(const ProceduralSurfaceBuffer* surface) {
        if (!surface) throw std::runtime_error("Null surface in ExtractBaseType");
        return static_cast<SurfaceEquationType>(surface->secondary_surface_type & 0x0F);
    }
    
    static CombinationOperation ExtractCombination(const ProceduralSurfaceBuffer* surface) {
        if (!surface) throw std::runtime_error("Null surface in ExtractCombination");
        return static_cast<CombinationOperation>((surface->secondary_surface_type >> 4) & 0x0F);
    }
    
    static uint8_t ExtractModifiers(const ProceduralSurfaceBuffer* surface) {
        if (!surface) throw std::runtime_error("Null surface in ExtractModifiers");
        return surface->surface_modifiers;
    }
    
    static const float* ExtractParameters(const ProceduralSurfaceBuffer* surface) {
        if (!surface) throw std::runtime_error("Null surface in ExtractParameters");
        return surface->eqn_params;
    }
};

#endif