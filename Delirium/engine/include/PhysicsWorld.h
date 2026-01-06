// =============================================================================
// PhysicsWorld.h - Physics simulation system
// Location: engine/include/delirium/PhysicsWorld.h
// Part of DeliRium Engine
// =============================================================================

#pragma once
#include "math_types.h"
#include <vector>

namespace DeliRium {

class ProceduralObject;

struct CollisionInfo {
    bool hasCollision = false;
    Vec3 contactPoint = {0, 0, 0};
    Vec3 contactNormal = {0, 0, 1};
    float penetrationDepth = 0.0f;
    ProceduralObject* objectA = nullptr;
    ProceduralObject* objectB = nullptr;
};

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld() = default;
    
    void AddObject(ProceduralObject* obj);
    void RemoveObject(ProceduralObject* obj);
    void Step(float deltaTime);
    std::vector<CollisionInfo> DetectCollisions();
    
    Vec3 gravity = {0, -9.81f, 0};
    float damping = 0.99f;
    bool enabled = true;
    
private:
    std::vector<ProceduralObject*> objects;
    void IntegrateVelocities(float dt);
    void SolveCollisions(const std::vector<CollisionInfo>& collisions);
};

} // namespace DeliRium