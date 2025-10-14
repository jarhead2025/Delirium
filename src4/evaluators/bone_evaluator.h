// Enhanced bone_evaluator.h - Replace the existing content

#pragma once
#include "math_types.h"
#include "object_descriptor_buffers.h"
#include "anchorpoint_evaluator.h"
#include <vector>

class BoneEvaluator {
public:
    // Main skeleton evaluation function
    static void EvaluateSkeleton(
        const std::vector<BoneBuffer>& bonePool,
        std::vector<Matrix4x4>& outWorldTransforms);
    
    // Enhanced skeleton evaluation with skinning support
    static void EvaluateSkeletonWithSkinning(
        const std::vector<BoneBuffer>& bonePool,
        const std::vector<AnchorPointBuffer>& anchorPool,
        std::vector<Matrix4x4>& outWorldTransforms,
        std::vector<Vec3>& outSkinnedPositions
    );

    // Encoding functions for converting FBX data to PSF format
    static Vec3 EncodePosition(const Vec3& position);
    static uint32_t EncodeOrientation(const Quaternion& orientation);

private:
    static void EvaluateBoneRecursive(
        uint32_t boneIndex,
        const Matrix4x4& parentWorldTransform,
        const std::vector<BoneBuffer>& bonePool,
        std::vector<Matrix4x4>& outWorldTransforms);

    static Quaternion DecodeOrientation(uint32_t packedOrientation);
    static Vec3 DecodePosition(const uint16_t halfFloatPos[3]);
    static Matrix4x4 CreateLocalTransform(const Vec3& position, const Quaternion& orientation);
    static Matrix4x4 MultiplyMatrices(const Matrix4x4& matA, const Matrix4x4& matB);
};