#include "anchorpoint_evaluator.h"
#include <cmath>





void AnchorPointEvaluator::EvaluateAnchorPoint(
    const AnchorPointBuffer& anchor,
    const std::vector<Matrix4x4>& boneTransforms,
    float currentTime,
    Vec3& outPosition,
    Vec3& outNormal)
{
    const Matrix4x4& boneMatrix = (anchor.bone_index < boneTransforms.size())
                                  ? boneTransforms[anchor.bone_index]
                                  : Matrix4x4{};

    Vec3 localPos = DecodePosition(anchor.pos_xyz);
    Vec3 localNorm = DecodeNormal(anchor.normal);
    
    Vec3 localDelta = Vec3{
        static_cast<float>(anchor.delta_xyz[0]) / 1024.0f,
        static_cast<float>(anchor.delta_xyz[1]) / 1024.0f,
        static_cast<float>(anchor.delta_xyz[2]) / 1024.0f
    };

    Vec3 finalLocalPos = Vec3{
        localPos.x + localDelta.x,
        localPos.y + localDelta.y,
        localPos.z + localDelta.z
    };

    outPosition = Transform(finalLocalPos, boneMatrix, false);
    outNormal = Transform(localNorm, boneMatrix, true);
}

Vec3 AnchorPointEvaluator::DecodeNormal(const uint8_t compressedNormal[3]) {
    Vec3 norm = Vec3{
        (static_cast<float>(compressedNormal[0]) / 127.5f) - 1.0f,
        (static_cast<float>(compressedNormal[1]) / 127.5f) - 1.0f,
        (static_cast<float>(compressedNormal[2]) / 127.5f) - 1.0f
    };
    
    NormalizeVector(norm);
    return norm;
}

Vec3 AnchorPointEvaluator::DecodePosition(const uint16_t halfFloatPos[3]) {
    // CORRECCIÓN: Decodificación correcta
    // [0, 4095] mapeado de vuelta a [-2, +2]
    return Vec3{
        (static_cast<float>(halfFloatPos[0]) / 4095.0f) * 4.0f - 2.0f,
        (static_cast<float>(halfFloatPos[1]) / 4095.0f) * 4.0f - 2.0f,
        (static_cast<float>(halfFloatPos[2]) / 4095.0f) * 4.0f - 2.0f
    };
}
void AnchorPointEvaluator::EncodePosition(const Vec3& position, uint16_t halfFloatPos[3]) {
    // Map [-2, +2] to [0, 4095]
    halfFloatPos[0] = static_cast<uint16_t>((position.x + 2.0f) / 4.0f * 4095.0f);
    halfFloatPos[1] = static_cast<uint16_t>((position.y + 2.0f) / 4.0f * 4095.0f);
    halfFloatPos[2] = static_cast<uint16_t>((position.z + 2.0f) / 4.0f * 4095.0f);
}
