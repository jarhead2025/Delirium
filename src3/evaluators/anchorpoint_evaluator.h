#pragma once
#include "math_types.h"
#include "object_descriptor_buffers.h"
#include <vector>

class AnchorPointEvaluator {
public:
    static void EvaluateAnchorPoint(
        const AnchorPointBuffer& anchor,
        const std::vector<Matrix4x4>& boneTransforms,
        float currentTime,
        Vec3& outPosition,
        Vec3& outNormal);
        
    static Vec3 DecodeNormal(const uint8_t compressedNormal[3]);
    static Vec3 DecodePosition(const uint16_t halfFloatPos[3]);
    static void EncodePosition(const Vec3& position, uint16_t halfFloatPos[3]);
};