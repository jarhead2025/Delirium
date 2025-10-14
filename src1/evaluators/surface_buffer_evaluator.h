#ifndef SURFACE_BUFFER_EVALUATOR_H
#define SURFACE_BUFFER_EVALUATOR_H

#include <vector>
#include "surface_buffers.h"
#include "math_types.h"

class SurfaceBufferEvaluator {
public:
    // =============================================================================
    // FUNCIONES PRINCIPALES (EXISTENTES)
    // =============================================================================
 static void EvaluateUVMappedPatch(
    const ProceduralSurfaceBuffer* surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    const std::vector<Matrix4x4>& bone_transforms,
    float u, float v,
    float* outPosition);
    static void EvaluateSurface(
        const ProceduralSurfaceBuffer* surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        const std::vector<Matrix4x4>& bone_transforms,
        float u, float v,
        float* outPosition, float* outNormal);

    static void EvaluateSurfaceMesh(
        const ProceduralSurfaceBuffer* surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        const std::vector<Matrix4x4>& bone_transforms,
        uint32_t resolution,
        std::vector<float>& outVertices,
        std::vector<uint32_t>& outIndices,
        std::vector<float>& outNormals,
        float time);

    static void CalculateSurfaceNormal(
        const ProceduralSurfaceBuffer* surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        const std::vector<Matrix4x4>& bone_transforms,
        float u, float v,
        float* outNormal);

    static void EvaluateBasePatch(
        const ProceduralSurfaceBuffer* surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        const std::vector<Matrix4x4>& bone_transforms,
        float u, float v,
        float* outPosition);

    static void ApplyCombination(
        const ProceduralSurfaceBuffer* surface,
        float* position);

    static void ApplyModifiers(
        const ProceduralSurfaceBuffer* surface,
        float* position,
        float u, float v);

    static void EvaluatePrimitive_Absolute(
        const ProceduralSurfaceBuffer* surface,
        float* position);

    static void EvaluatePatch_Displacement(
        const ProceduralSurfaceBuffer* surface,
        float* position,
        float u, float v);

    // =============================================================================
    // NUEVAS FUNCIONES AGREGADAS (PARA CONTINUIDAD)
    // =============================================================================
    
    // ← NUEVA: Helper para obtener normal de anchor más cercano
    static Vec3 GetClosestAnchorNormal(
        const ProceduralSurfaceBuffer* surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        float u, float v);

    // =============================================================================
    // FUNCIONES DE DEBUG Y UTILIDAD
    // =============================================================================
    
    // ← NUEVA: Debug de continuidad entre faces
    static void DebugFaceContinuity(
        const ProceduralSurfaceBuffer* surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        const std::vector<Matrix4x4>& bone_transforms = {});
    
    // ← NUEVA: Verificar calidad de mesh
    static void ValidateMeshQuality(
        const std::vector<float>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<float>& normals);
    
    // ← NUEVA: Calcular estadísticas de superficie
    static void CalculateSurfaceStats(
        const ProceduralSurfaceBuffer* surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        const std::vector<Matrix4x4>& bone_transforms,
        uint32_t sample_resolution = 10);

private:
    // =============================================================================
    // FUNCIONES HELPER PRIVADAS
    // =============================================================================
    
    // ← NUEVA: Smooth normal calculation
    static Vec3 CalculateSmoothedNormal(
        const std::vector<Vec3>& vertex_normals,
        uint32_t center_idx,
        uint32_t grid_width,
        uint32_t grid_height);
    
    // ← NUEVA: Verificar validez de normal
    static bool IsValidNormal(const Vec3& normal);
    
    // ← NUEVA: Clamp UV coordinates safely
    static void ClampUV(float& u, float& v, float epsilon = 0.001f);
};

// =============================================================================
// FUNCIONES GLOBALES DE UTILIDAD
// =============================================================================

// ← NUEVA: Debug completo de colección procedural
void DebugProceduralCollection(const class ProceduralObjectCollection& collection);

// ← NUEVA: Export mejorado con opciones
bool ExportToOBJAdvanced(
    const std::string& filename,
    const std::vector<float>& vertices,
    const std::vector<uint32_t>& indices,
    const std::vector<float>& normals,
    bool smooth_shading = true,
    bool export_mtl = true);

#endif