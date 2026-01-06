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



    for (uint32_t i = 0; i < bonePool.size(); ++i) {

        const auto& bone = bonePool[i];

        if (bone.parent_bone_index == i || bone.parent_bone_index >= bonePool.size()) {

            Matrix4x4 identityMatrix{};

            EvaluateBoneRecursive(i, identityMatrix, bonePool, outWorldTransforms);

        }

    }

}



void BoneEvaluator::EvaluateBoneRecursive(

    uint32_t boneIndex,

    const Matrix4x4& parentWorldTransform,

    const std::vector<BoneBuffer>& bonePool,

    std::vector<Matrix4x4>& outWorldTransforms)

{

    const BoneBuffer& currentBone = bonePool[boneIndex];



    Vec3 localPos = DecodePosition(currentBone.pos_xyz);

    Quaternion localRot = DecodeOrientation(currentBone.orientation);



    Matrix4x4 localTransform = CreateLocalTransform(localPos, localRot);

    Matrix4x4 worldTransform = MultiplyMatrices(parentWorldTransform, localTransform);

    

    outWorldTransforms[boneIndex] = worldTransform;



    if (currentBone.child_bone_index != boneIndex && currentBone.child_bone_index < bonePool.size()) {

        EvaluateBoneRecursive(currentBone.child_bone_index, worldTransform, bonePool, outWorldTransforms);

    }

}



Quaternion BoneEvaluator::DecodeOrientation(uint32_t packedOrientation) {

    return Quaternion{0.0f, 0.0f, 0.0f, 1.0f};

}



Vec3 BoneEvaluator::DecodePosition(const uint16_t halfFloatPos[3]) {

    return Vec3{

        (static_cast<float>(halfFloatPos[0]) / 1024.0f) - 2.0f,

        (static_cast<float>(halfFloatPos[1]) / 1024.0f) - 2.0f,

        (static_cast<float>(halfFloatPos[2]) / 1024.0f) - 2.0f

    };

}



Matrix4x4 BoneEvaluator::CreateLocalTransform(const Vec3& position, const Quaternion& orientation) {

    Matrix4x4 mat{};

    float qx = orientation.x, qy = orientation.y, qz = orientation.z, qw = orientation.w;

    

    mat.m[0] = 1 - 2*qy*qy - 2*qz*qz; mat.m[4] = 2*qx*qy - 2*qz*qw; mat.m[8] = 2*qx*qz + 2*qy*qw;

    mat.m[1] = 2*qx*qy + 2*qz*qw; mat.m[5] = 1 - 2*qx*qx - 2*qz*qz; mat.m[9] = 2*qy*qz - 2*qx*qw;

    mat.m[2] = 2*qx*qz - 2*qy*qw; mat.m[6] = 2*qy*qz + 2*qx*qw; mat.m[10] = 1 - 2*qx*qx - 2*qy*qy;



    mat.m[12] = position.x;

    mat.m[13] = position.y;

    mat.m[14] = position.z;



    return mat;

}



Matrix4x4 BoneEvaluator::MultiplyMatrices(const Matrix4x4& matA, const Matrix4x4& matB) {

    return ::MultiplyMatrices(matA, matB); // Use global function

}
