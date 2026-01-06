#pragma once

#include "math_types.h"

#include "object_descriptor_buffers.h"

#include <vector>



class BoneEvaluator {

public:

    static void EvaluateSkeleton(

        const std::vector<BoneBuffer>& bonePool,

        std::vector<Matrix4x4>& outWorldTransforms);



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
