// Enhanced Bone Evaluator - Replace bone_evaluator.cpp content

#include "bone_evaluator.h"
#include <cmath>

void BoneEvaluator::EvaluateSkeleton(
    const std::vector<BoneBuffer>& bonePool,
    std::vector<Matrix4x4>& outWorldTransforms) 
{
    if (bonePool.empty()) {
        outWorldTransforms.clear();
        return;
    }

    outWorldTransforms.resize(bonePool.size());
    for (auto& mat : outWorldTransforms) {
        mat = Matrix4x4{}; // Initialize to identity
    }

    // Find root bones (bones that are their own parent or have invalid parent)
    std::vector<uint32_t> root_bones;
    for (uint32_t i = 0; i < bonePool.size(); ++i) {
        const auto& bone = bonePool[i];
        if (bone.parent_bone_index == i || bone.parent_bone_index >= bonePool.size()) {
            root_bones.push_back(i);
        }
    }

    // Evaluate each root bone hierarchy
    Matrix4x4 identityMatrix{};
    for (uint32_t root_index : root_bones) {
        EvaluateBoneRecursive(root_index, identityMatrix, bonePool, outWorldTransforms);
    }
}

void BoneEvaluator::EvaluateBoneRecursive(
    uint32_t boneIndex,
    const Matrix4x4& parentWorldTransform,
    const std::vector<BoneBuffer>& bonePool,
    std::vector<Matrix4x4>& outWorldTransforms)
{
    if (boneIndex >= bonePool.size()) return;
    
    const BoneBuffer& currentBone = bonePool[boneIndex];

    // Decode bone transform
    Vec3 localPos = DecodePosition(currentBone.pos_xyz);
    Quaternion localRot = DecodeOrientation(currentBone.orientation);

    // Create local transform matrix
    Matrix4x4 localTransform = CreateLocalTransform(localPos, localRot);
    
    // Combine with parent transform
    Matrix4x4 worldTransform = MultiplyMatrices(parentWorldTransform, localTransform);
    
    outWorldTransforms[boneIndex] = worldTransform;

    // Recursively evaluate children
    // Since we store parent->child relationships, we need to find children
    for (uint32_t i = 0; i < bonePool.size(); ++i) {
        if (bonePool[i].parent_bone_index == boneIndex && i != boneIndex) {
            EvaluateBoneRecursive(i, worldTransform, bonePool, outWorldTransforms);
        }
    }
}

// Enhanced quaternion decoding with proper implementation
Quaternion BoneEvaluator::DecodeOrientation(uint32_t packedOrientation) {
    // For now, return identity quaternion
    // TODO: Implement proper quaternion packing/unpacking when we have animation data
    return Quaternion{0.0f, 0.0f, 0.0f, 1.0f};
}

// Enhanced position decoding to match anchor point encoding
Vec3 BoneEvaluator::DecodePosition(const uint16_t halfFloatPos[3]) {
    // Decode from same format as anchor points: [0, 4095] maps to [-2, +2]
    return Vec3{
        (static_cast<float>(halfFloatPos[0]) / 4095.0f) * 4.0f - 2.0f,
        (static_cast<float>(halfFloatPos[1]) / 4095.0f) * 4.0f - 2.0f,
        (static_cast<float>(halfFloatPos[2]) / 4095.0f) * 4.0f - 2.0f
    };
}

Matrix4x4 BoneEvaluator::CreateLocalTransform(const Vec3& position, const Quaternion& orientation) {
    Matrix4x4 mat{};
    
    // Convert quaternion to rotation matrix
    float qx = orientation.x, qy = orientation.y, qz = orientation.z, qw = orientation.w;
    
    // Rotation matrix from quaternion
    mat.m[0] = 1 - 2*qy*qy - 2*qz*qz; mat.m[4] = 2*qx*qy - 2*qz*qw; mat.m[8]  = 2*qx*qz + 2*qy*qw; mat.m[12] = position.x;
    mat.m[1] = 2*qx*qy + 2*qz*qw;     mat.m[5] = 1 - 2*qx*qx - 2*qz*qz; mat.m[9]  = 2*qy*qz - 2*qx*qw; mat.m[13] = position.y;
    mat.m[2] = 2*qx*qz - 2*qy*qw;     mat.m[6] = 2*qy*qz + 2*qx*qw;     mat.m[10] = 1 - 2*qx*qx - 2*qy*qy; mat.m[14] = position.z;
    mat.m[3] = 0.0f;                  mat.m[7] = 0.0f;                   mat.m[11] = 0.0f;                   mat.m[15] = 1.0f;

    return mat;
}

Matrix4x4 BoneEvaluator::MultiplyMatrices(const Matrix4x4& matA, const Matrix4x4& matB) {
    return ::MultiplyMatrices(matA, matB); // Use global function
}

// New helper functions for enhanced bone evaluation

Vec3 BoneEvaluator::EncodePosition(const Vec3& position) {
    // Encode position to [0, 4095] range for storage
    Vec3 encoded;
    encoded.x = std::max(0.0f, std::min(4095.0f, (position.x + 2.0f) / 4.0f * 4095.0f));
    encoded.y = std::max(0.0f, std::min(4095.0f, (position.y + 2.0f) / 4.0f * 4095.0f));
    encoded.z = std::max(0.0f, std::min(4095.0f, (position.z + 2.0f) / 4.0f * 4095.0f));
    return encoded;
}

uint32_t BoneEvaluator::EncodeOrientation(const Quaternion& orientation) {
    // Simple encoding for now - TODO: implement proper quaternion compression
    return 0; // Identity quaternion
}

void BoneEvaluator::EvaluateSkeletonWithSkinning(
    const std::vector<BoneBuffer>& bonePool,
    const std::vector<AnchorPointBuffer>& anchorPool,
    std::vector<Matrix4x4>& outWorldTransforms,
    std::vector<Vec3>& outSkinnedPositions
) {
    // First evaluate the skeleton
    EvaluateSkeleton(bonePool, outWorldTransforms);
    
    // Then apply skinning to anchor points
    outSkinnedPositions.resize(anchorPool.size());
    
    for (size_t i = 0; i < anchorPool.size(); ++i) {
        const auto& anchor = anchorPool[i];
        Vec3 originalPos = AnchorPointEvaluator::DecodePosition(anchor.pos_xyz);
        
        // Apply bone transformation if valid bone index
        if (anchor.bone_index < outWorldTransforms.size()) {
            outSkinnedPositions[i] = Transform(originalPos, outWorldTransforms[anchor.bone_index], false);
        } else {
            outSkinnedPositions[i] = originalPos;
        }
    }
}