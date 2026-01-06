#include "surface_buffers.h" // Assumes this is the FINAL header with AnchorPoints

#include <cmath>

#include <algorithm>

#include <iostream>

#include <cstring>

#include "mathematical_color_evaluator.h" 

#include "object_descriptor_buffers.h"

// =============================================================================

// SurfaceBufferUtils Implementation - REVISED FOR FINAL ANCHOR ARCHITECTURE

// =============================================================================



// Creates a buffer for a single, non-combined surface.

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateSingle(SurfaceEquationType type,

                                                       const float params[8],

                                                       uint8_t modifiers) {

    ProceduralSurfaceBuffer buffer;

    memset(&buffer, 0, sizeof(buffer)); // Zero-initialize all fields



    // Pack the surface definition index

    buffer.surface_definition_index = PackSurfaceIndex(type, COMBO_NONE, modifiers);



    // Copy the full set of 8 equation parameters

    if (params) {

        memcpy(buffer.eqn_params, params, sizeof(float) * 8);

    }



    // Set default PBR material properties

    buffer.roughness = 128; // 0.5 roughness

    buffer.metallic = 0;    // non-metallic

    buffer.emissive_strength = 0;

    buffer.specular_tint = 0;



    // Set default procedural color to a constant gray

    buffer.color_eqn_r = buffer.color_eqn_g = buffer.color_eqn_b = COLOR_CONSTANT;

    buffer.color_param_r = buffer.color_param_g = buffer.color_param_b = 128;



    return buffer;

}



// Creates a buffer for a combination of two surfaces.

ProceduralSurfaceBuffer SurfaceBufferUtils::CreateCombination(

    SurfaceEquationType primaryType, const float primaryParams[4],

    SurfaceEquationType secondaryType, const float secondaryParams[4],

    CombinationOperation operation,

    float blendWeight,

    uint8_t modifiers) {



    ProceduralSurfaceBuffer buffer;

    memset(&buffer, 0, sizeof(buffer));



    // Pack the surface definition index with the combination operation

    buffer.surface_definition_index = PackSurfaceIndex(primaryType, operation, modifiers);



    // Store the secondary surface type

    buffer.secondary_surface_type = static_cast<uint8_t>(secondaryType);



    // Encode blend weight into a single byte

    buffer.combination_param = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, blendWeight)) * 255.0f);



    // Copy primary and secondary parameters into the single 8-float array

    if (primaryParams) {

        memcpy(&buffer.eqn_params[0], primaryParams, sizeof(float) * 4);

    }

    if (secondaryParams) {

        memcpy(&buffer.eqn_params[4], secondaryParams, sizeof(float) * 4);

    }



    // Set default PBR material properties and procedural color

    buffer.roughness = 128;

    buffer.metallic = 0;

    buffer.color_eqn_r = buffer.color_eqn_g = buffer.color_eqn_b = COLOR_CONSTANT;

    buffer.color_param_r = buffer.color_param_g = buffer.color_param_b = 128;



    return buffer;

}



// Provides a human-readable description of the buffer's contents.

const char* SurfaceBufferUtils::GetSurfaceDescription(const ProceduralSurfaceBuffer* buffer) {

    if (!buffer) return "Invalid Buffer";



    // Unpack the definition

    SurfaceEquationType primaryType = ExtractBaseType(buffer->surface_definition_index);

    CombinationOperation combo = ExtractCombination(buffer->surface_definition_index);

    uint8_t modifiers = ExtractModifiers(buffer->surface_definition_index);

    

    // (This function's logic remains largely the same, as it reads the packed index,

    // which is consistent across versions. It does not depend on the changed fields.)

    static char description[256];

    // ... Full implementation of GetSurfaceDescription would go here ...

    snprintf(description, sizeof(description), "Surface Type: %d, Combo: %d", primaryType, combo);

    

    return description;

}



// Validates the integrity of the buffer.

bool SurfaceBufferUtils::ValidateBuffer(const ProceduralSurfaceBuffer* buffer) {

    if (!buffer) return false;



    // Check if equation indices are within valid enum ranges

    SurfaceEquationType primaryType = ExtractBaseType(buffer->surface_definition_index);

    if (primaryType > EQUATION_METABALL) return false;



    CombinationOperation combo = ExtractCombination(buffer->surface_definition_index);

    if (combo > COMBO_LAYER) return false;



    // Check if equation parameters are finite numbers

    for (int i = 0; i < 8; ++i) {

        if (!std::isfinite(buffer->eqn_params[i])) return false;

    }



    return true;

}



// (The ParameterEncoder and ModifierEncoder classes are simple helpers that

// operate on raw data, so they do not need to be part of the SurfaceBufferUtils

// class and can remain as they were, as they are compatible.)