// =============================================================================
// Transform.cpp - Transform component implementation
// Location: engine/src/core/Transform.cpp
// Part of DeliRium Engine
// =============================================================================

#include "Transform.h"
#include <cmath>

namespace DeliRium {

Transform::Transform() 
    : position({0, 0, 0})
    , rotation({0, 0, 0})
    , scale({1, 1, 1})
    , isDirty(true)
{
    // Initialize with identity matrix
    for (int i = 0; i < 16; ++i) {
        localMatrix.m[i] = 0.0f;
    }
    localMatrix.m[0] = localMatrix.m[5] = localMatrix.m[10] = localMatrix.m[15] = 1.0f;
}

void Transform::Translate(const Vec3& delta) {
    position.x += delta.x;
    position.y += delta.y;
    position.z += delta.z;
    isDirty = true;
}

void Transform::SetPosition(const Vec3& pos) {
    position = pos;
    isDirty = true;
}

void Transform::Rotate(const Vec3& eulerDelta) {
    rotation.x += eulerDelta.x;
    rotation.y += eulerDelta.y;
    rotation.z += eulerDelta.z;
    
    // Normalize angles to [-180, 180] range
    while (rotation.x > 180.0f) rotation.x -= 360.0f;
    while (rotation.x < -180.0f) rotation.x += 360.0f;
    while (rotation.y > 180.0f) rotation.y -= 360.0f;
    while (rotation.y < -180.0f) rotation.y += 360.0f;
    while (rotation.z > 180.0f) rotation.z -= 360.0f;
    while (rotation.z < -180.0f) rotation.z += 360.0f;
    
    isDirty = true;
}

void Transform::SetRotation(const Vec3& rot) {
    rotation = rot;
    isDirty = true;
}

void Transform::SetScale(const Vec3& scl) {
    scale = scl;
    isDirty = true;
}

Vec3 Transform::GetForward() const {
    // Calculate forward vector from rotation
    // Forward is -Z in OpenGL convention
    float yaw = rotation.y * (M_PI / 180.0f);
    float pitch = rotation.x * (M_PI / 180.0f);
    
    return {
        sin(yaw) * cos(pitch),
        -sin(pitch),
        -cos(yaw) * cos(pitch)
    };
}

Vec3 Transform::GetRight() const {
    // Right vector (perpendicular to forward in XZ plane)
    float yaw = rotation.y * (M_PI / 180.0f);
    
    return {
        cos(yaw),
        0,
        sin(yaw)
    };
}

Vec3 Transform::GetUp() const {
    // Up vector (cross product of right and forward)
    Vec3 forward = GetForward();
    Vec3 right = GetRight();
    return Cross(right, forward);
}

Matrix4x4 Transform::GetMatrix() {
    if (isDirty) {
        RecalculateMatrix();
        isDirty = false;
    }
    return localMatrix;
}

void Transform::RecalculateMatrix() {
    // Convert Euler angles from degrees to radians
    float rx = rotation.x * (M_PI / 180.0f);
    float ry = rotation.y * (M_PI / 180.0f);
    float rz = rotation.z * (M_PI / 180.0f);
    
    // Calculate sin/cos values
    float cx = cos(rx), sx = sin(rx);
    float cy = cos(ry), sy = sin(ry);
    float cz = cos(rz), sz = sin(rz);
    
    // Build rotation matrix (ZYX Euler order)
    // This is the standard order for most game engines
    Matrix4x4 rotMat;
    rotMat.m[0] = cy * cz;
    rotMat.m[1] = cy * sz;
    rotMat.m[2] = -sy;
    rotMat.m[3] = 0;
    
    rotMat.m[4] = sx * sy * cz - cx * sz;
    rotMat.m[5] = sx * sy * sz + cx * cz;
    rotMat.m[6] = sx * cy;
    rotMat.m[7] = 0;
    
    rotMat.m[8] = cx * sy * cz + sx * sz;
    rotMat.m[9] = cx * sy * sz - sx * cz;
    rotMat.m[10] = cx * cy;
    rotMat.m[11] = 0;
    
    rotMat.m[12] = 0;
    rotMat.m[13] = 0;
    rotMat.m[14] = 0;
    rotMat.m[15] = 1;
    
    // Build scale matrix
    Matrix4x4 scaleMat;
    for (int i = 0; i < 16; ++i) scaleMat.m[i] = 0.0f;
    scaleMat.m[0] = scale.x;
    scaleMat.m[5] = scale.y;
    scaleMat.m[10] = scale.z;
    scaleMat.m[15] = 1.0f;
    
    // Combine: Scale * Rotation (multiply in this order)
    localMatrix = MultiplyMatrices(scaleMat, rotMat);
    
    // Add translation (last column)
    localMatrix.m[12] = position.x;
    localMatrix.m[13] = position.y;
    localMatrix.m[14] = position.z;
}

} // namespace DeliRium