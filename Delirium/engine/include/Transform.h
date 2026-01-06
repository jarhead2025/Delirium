// =============================================================================
// Transform.h - Transform component header
// Location: engine/include/delirium/Transform.h
// Part of DeliRium Engine
// =============================================================================

#pragma once
#include "math_types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace DeliRium {

class Transform {
public:
    Transform();
    
    // Position manipulation
    void Translate(const Vec3& delta);
    void SetPosition(const Vec3& pos);
    Vec3 GetPosition() const { return position; }
    
    // Rotation manipulation (Euler angles in degrees)
    void Rotate(const Vec3& eulerDelta);
    void SetRotation(const Vec3& rot);
    Vec3 GetRotation() const { return rotation; }
    
    // Scale manipulation
    void SetScale(const Vec3& scl);
    Vec3 GetScale() const { return scale; }
    
    // Direction vectors (calculated from rotation)
    Vec3 GetForward() const;
    Vec3 GetRight() const;
    Vec3 GetUp() const;
    
    // Matrix access (auto-recalculates if dirty)
    Matrix4x4 GetMatrix();
    
    // Mark dirty (call this if you modify position/rotation/scale directly)
    void MarkDirty() { isDirty = true; }
    
private:
    // Transform data
    Vec3 position;
    Vec3 rotation;  // Euler angles in degrees (X=pitch, Y=yaw, Z=roll)
    Vec3 scale;
    
    // Cached matrix
    Matrix4x4 localMatrix;
    bool isDirty;
    
    // Internal recalculation
    void RecalculateMatrix();
};

} // namespace DeliRium