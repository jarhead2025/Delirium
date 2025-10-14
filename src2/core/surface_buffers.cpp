#include "surface_buffers.h"
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <iostream> // Added for potential error logging

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateSingle(SurfaceEquationType type, const std::vector<float>& params, uint8_t modifiers) {
    // Validate parameters
    for (float param : params) {
        if (!std::isfinite(param)) {
            throw std::runtime_error("Invalid parameter in CreateSingle");
        }
    }
    
    // Create fixed buffer
    ProceduralSurfaceBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    
    // Set surface definition index and modifiers
    buffer.surface_definition_index = PackSurfaceIndex(type, COMBO_NONE, modifiers);
    buffer.secondary_surface_type = static_cast<uint8_t>(type);
    buffer.surface_modifiers = modifiers;
    
    // Copy parameters
    for (size_t i = 0; i < params.size() && i < 8; i++) {
        buffer.eqn_params[i] = params[i];
    }
    
    // Default values
    buffer.roughness = 128;
    buffer.metallic = 0;
    buffer.emissive_strength = 0;
    buffer.specular_tint = 0;
    
    return buffer;
}

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateCombination(SurfaceEquationType primaryType, const std::vector<ProceduralSurfaceBuffer*>& buffers, CombinationOperation combo, uint8_t modifiers) {
    // Validate sub-buffers
    for (const ProceduralSurfaceBuffer* subBuffer : buffers) {
        if (!subBuffer) {
            throw std::runtime_error("Null sub-buffer in CreateCombination");
        }
    }
    
    // Create fixed buffer
    ProceduralSurfaceBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    
    // Set surface definition index
    buffer.surface_definition_index = PackSurfaceIndex(primaryType, combo, modifiers);
    buffer.surface_modifiers = modifiers;
    
    // Set combination param (default blend weight 0.5)
    buffer.combination_param = 128; // 0.5 * 255
    
    // If we have at least one sub-buffer, copy its parameters
    if (!buffers.empty() && buffers[0]) {
        // Copy primary params from first buffer
        for (int i = 0; i < 4; i++) {
            buffer.eqn_params[i] = buffers[0]->eqn_params[i];
        }
        
        // If we have at least two sub-buffers, copy second one's params
        if (buffers.size() > 1 && buffers[1]) {
            // Use the second buffer's surface type
            buffer.secondary_surface_type = SurfaceBufferUtils::ExtractBaseType(buffers[1]);
            
            // Copy secondary params
            for (int i = 0; i < 4; i++) {
                buffer.eqn_params[i + 4] = buffers[1]->eqn_params[i];
            }
        }
    }
    
    // Default values
    buffer.roughness = 128;
    buffer.metallic = 0;
    buffer.emissive_strength = 0;
    buffer.specular_tint = 0;
    
    return buffer;
}

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateWithAnchors(SurfaceEquationType type, const uint16_t anchorIndices[4], const float params[8], uint8_t modifiers) {
    // Create fixed buffer
    ProceduralSurfaceBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    
    // Set surface definition index
    buffer.surface_definition_index = PackSurfaceIndex(type, COMBO_NONE, modifiers);
    buffer.secondary_surface_type = static_cast<uint8_t>(type);
    buffer.surface_modifiers = modifiers;
    
    // Copy parameters
    for (int i = 0; i < 8; i++) {
        buffer.eqn_params[i] = params[i];
        
        // Validate parameter
        if (!std::isfinite(params[i])) {
            throw std::runtime_error("Invalid parameter in CreateWithAnchors");
        }
    }
    
    // Copy anchor indices
    for (int i = 0; i < 4; i++) {
        buffer.anchor_point_indices[i] = anchorIndices[i];
    }
    
    // Default values
    buffer.roughness = 128;
    buffer.metallic = 0;
    buffer.emissive_strength = 0;
    buffer.specular_tint = 0;
    
    return buffer;
}

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateWithAnchors(SurfaceEquationType type, const std::vector<int>& anchorIndices, const std::vector<float>& params, uint8_t modifiers) {
    // Validate anchor indices
    for (int idx : anchorIndices) {
        if (idx < 0) {
            throw std::runtime_error("Negative anchor index in CreateWithAnchors");
        }
    }
    
    // Validate parameters
    for (float param : params) {
        if (!std::isfinite(param)) {
            throw std::runtime_error("Invalid parameter in CreateWithAnchors");
        }
    }
    
    // Start with a single surface buffer
    ProceduralSurfaceBuffer buffer = CreateSingle(type, params, modifiers);
    
    // Set anchor indices
    for (size_t i = 0; i < anchorIndices.size() && i < 4; i++) {
        buffer.anchor_point_indices[i] = static_cast<uint16_t>(anchorIndices[i]);
    }
    
    // Ensure modifiers and type are set
    buffer.secondary_surface_type = static_cast<uint8_t>(type);
    buffer.surface_modifiers = modifiers;
    
    return buffer;
}

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateMetaball(const float center[3], float radius, uint8_t modifiers) {
    ProceduralSurfaceBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    
    buffer.secondary_surface_type = static_cast<uint8_t>(EQUATION_METABALL);
    buffer.surface_modifiers = modifiers;
    
    buffer.eqn_params[0] = center[0];
    buffer.eqn_params[1] = center[1];
    buffer.eqn_params[2] = center[2];
    buffer.eqn_params[3] = radius;
    for (int i = 4; i < 8; ++i) {
        buffer.eqn_params[i] = 0.0f;
    }
    
    buffer.surface_definition_index = PackSurfaceIndex(EQUATION_METABALL, COMBO_NONE, modifiers);
    
    buffer.roughness = 128;
    buffer.metallic = 0;
    buffer.emissive_strength = 0;
    buffer.specular_tint = 0;
    
    return buffer;
}

uint16_t SurfaceBufferUtils::PackSurfaceIndex(SurfaceEquationType type, CombinationOperation combo, uint8_t modifiers) {
    uint16_t index = 0;
    index |= (static_cast<uint16_t>(type) & 0x0F);
    index |= (static_cast<uint16_t>(combo) & 0x0F) << 4;
    index |= (static_cast<uint16_t>(modifiers) & 0xFF) << 8;
    return index;
}