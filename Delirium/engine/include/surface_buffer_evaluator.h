#pragma once

#include "math_types.h"

#include "surface_buffers.h"

#include <vector>

#include <string>



class SurfaceBufferEvaluator {

public:

    static void EvaluateSurface(const ProceduralSurfaceBuffer* buffer,

                               const std::vector<AnchorPointBuffer>& anchorPool,

                               const std::vector<Matrix4x4>& boneTransforms,

                               float u, float v,

                               float* outPosition,

                               float* outNormal = nullptr,

                               float currentTime = 0.0f);



    static void EvaluateSurfaceMesh(const ProceduralSurfaceBuffer* buffer,

                                   const std::vector<AnchorPointBuffer>& anchorPool,

                                   const std::vector<Matrix4x4>& boneTransforms,

                                   int resolution,

                                   std::vector<float>& outVertices,

                                   std::vector<uint32_t>& outIndices,

                                   std::vector<float>& outNormals,

                                   float currentTime = 0.0f);



    static Vec3 CalculateSurfaceNormal(

        const ProceduralSurfaceBuffer* buffer,

        const std::vector<AnchorPointBuffer>& anchorPool,

        const std::vector<Matrix4x4>& boneTransforms,

        float u, float v,

        float currentTime = 0.0f);



    // ADDED: Missing function declarations that are implemented in .cpp

    static void EvaluateOutwardCurvature(

        SurfaceEquationType type,

        const float* params,

        float u, float v,

        float& outDisplacement);



    static void EvaluateAnchorPointFull(

        const AnchorPointBuffer& anchor,

        const std::vector<Matrix4x4>& boneTransforms,

        float currentTime,

        Vec3& outPosition,

        Vec3& outNormal,

        Vec3& outTangent,

        Vec3& outColor);



    static void EvaluateSurfaceEnhanced(

        const ProceduralSurfaceBuffer* buffer,

        const std::vector<AnchorPointBuffer>& anchorPool,

        const std::vector<Matrix4x4>& boneTransforms,

        float u, float v,

        float* outPosition,

        float* outNormal,

        float* outColor,

        float* outMaterial, // [roughness, metallic, emissive, specular]

        float currentTime);



private:

    static void EvaluateBasePatch(const Vec3 corners[4], const Vec3 cornerNormals[4], 

                                 float u, float v, Vec3& outPosition, Vec3& outNormal);

    static void ApplyCombination(CombinationOperation op, const Vec3& dispA, const Vec3& dispB, 

                                float weight, Vec3& outDisp);

    static void ApplyModifiers(uint8_t modifiers, uint8_t strength_enc, float animPhase, 

                              float time, float u, float v, Vec3& pos, Vec3& norm);

    static void EvaluatePrimitive_Absolute(SurfaceEquationType type, const float* p, 

                                          float u, float v, Vec3& pos, Vec3& norm);

    static void EvaluatePatch_Displacement(SurfaceEquationType type, const float* p, 

                                          float u, float v, const Vec3& baseNorm, Vec3& outDisp);

};



class MeshGenerator {

public:

    static void GenerateGridIndices(int resolution, std::vector<uint32_t>& indices);

    static bool ExportToOBJ(const std::string& filename,

                           const std::vector<float>& vertices,

                           const std::vector<uint32_t>& indices,

                           const std::vector<float>& normals);

};
