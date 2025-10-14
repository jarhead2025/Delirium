#ifndef SURFACE_BUFFER_EVALUATOR_H
#define SURFACE_BUFFER_EVALUATOR_H

#include <vector>
#include "surface_buffers.h"
#include "math_types.h"
#include "anchorpoint_evaluator.h"
#include "object_descriptor_buffers.h"

// Forward declare to avoid circular dependency issues
void NormalizeVector(Vec3& v);

class SurfaceBufferEvaluator {
public:
    static Vec3 GetClosestPointOnSurface(const ProceduralSurfaceBuffer* surface, const Vec3& point,
                                        const std::vector<AnchorPointBuffer>& anchor_pool,
                                        float& u, float& v);
    
    static void EvaluateSurfaceMesh(const ProceduralSurfaceBuffer* surface,
                                   const std::vector<AnchorPointBuffer>& anchor_pool,
                                   const std::vector<BoneBuffer>& bone_pool,
                                   int resolution,
                                   std::vector<float>& vertices,
                                   std::vector<uint32_t>& indices,
                                   std::vector<float>& normals,
                                   float time);
};

#endif // SURFACE_BUFFER_EVALUATOR_H
