#include "surface_buffers.h"
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <vector>
#include <stdexcept>

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateSingle(SurfaceEquationType type, const std::vector<float>& params, uint8_t modifiers)
{
    // Validate parameters
    for (float param : params) {
        if (!std::isfinite(param)) {
            throw std::runtime_error("Invalid parameter in CreateSingle");
        }
    }
    
    // Create fixed buffer
    ProceduralSurfaceBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    
    // Set surface definition index (packs type, combination, and modifiers)
    buffer.surface_definition_index = PackSurfaceIndex(type, COMBO_NONE, modifiers);
    
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

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateCombination(SurfaceEquationType primaryType, const std::vector<ProceduralSurfaceBuffer*>& buffers, CombinationOperation combo, uint8_t modifiers)
{
    // Validate sub-buffers
    for (const ProceduralSurfaceBuffer* subBuffer : buffers) {
        if (!subBuffer) {
            throw std::runtime_error("Null sub-buffer in CreateCombination");
        }
    }
    
    // Create fixed buffer
    ProceduralSurfaceBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    
    // Set surface definition index (packs type, combination, and modifiers)
    buffer.surface_definition_index = PackSurfaceIndex(primaryType, combo, modifiers);
    
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
            // Use the second buffer's surface type - FIXED: using global scope resolution operator
            buffer.secondary_surface_type = ::ExtractBaseType(buffers[1]->surface_definition_index);
            
            // Copy secondary params
            for (int i = 0; i < 4; i++) {
                buffer.eqn_params[i+4] = buffers[1]->eqn_params[i];
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

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateWithAnchors(SurfaceEquationType type, const std::vector<int>& anchorIndices, const std::vector<float>& params, uint8_t modifiers)
{
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
    
    return buffer;
}

std::vector<float> SurfaceBufferUtils::ExtractParameters(const ProceduralSurfaceBuffer* buffer)
{
    if (!buffer) {
        throw std::runtime_error("Null buffer in ExtractParameters");
    }
    
    std::vector<float> params;
    for (int i = 0; i < 8; i++) {
        params.push_back(buffer->eqn_params[i]);
    }
    
    return params;
}

SurfaceEquationType SurfaceBufferUtils::ExtractBaseType(const ProceduralSurfaceBuffer* buffer)
{
    if (!buffer) {
        throw std::runtime_error("Null buffer in ExtractBaseType");
    }
    
    return ::ExtractBaseType(buffer->surface_definition_index);
}

CombinationOperation SurfaceBufferUtils::ExtractCombination(const ProceduralSurfaceBuffer* buffer)
{
    if (!buffer) {
        throw std::runtime_error("Null buffer in ExtractCombination");
    }
    
    return ::ExtractCombination(buffer->surface_definition_index);
}

uint8_t SurfaceBufferUtils::ExtractModifiers(const ProceduralSurfaceBuffer* buffer)
{
    if (!buffer) {
        throw std::runtime_error("Null buffer in ExtractModifiers");
    }
    
    return ::ExtractModifiers(buffer->surface_definition_index);
}

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateMetaball(const std::vector<float>& params)
{
    return CreateSingle(EQUATION_METABALL, params, 0);
}