// =============================================================================
// PhysicsWorld.cpp - Physics simulation implementation
// Location: engine/src/physics/PhysicsWorld.cpp
// Part of DeliRium Engine
// =============================================================================

#include "PhysicsWorld.h"
#include "ProceduralObject.h"
#include <algorithm>

namespace DeliRium {

PhysicsWorld::PhysicsWorld() {
    gravity = {0, -9.81f, 0};
}

void PhysicsWorld::AddObject(ProceduralObject* obj) {
    if (!obj) return;
    
    auto it = std::find(objects.begin(), objects.end(), obj);
    if (it == objects.end()) {
        objects.push_back(obj);
    }
}

void PhysicsWorld::RemoveObject(ProceduralObject* obj) {
    if (!obj) return;
    
    objects.erase(
        std::remove(objects.begin(), objects.end(), obj),
        objects.end()
    );
}

void PhysicsWorld::Step(float deltaTime) {
    if (!enabled) return;
    
    IntegrateVelocities(deltaTime);
    auto collisions = DetectCollisions();
    SolveCollisions(collisions);
}

std::vector<CollisionInfo> PhysicsWorld::DetectCollisions() {
    std::vector<CollisionInfo> collisions;
    
    for (size_t i = 0; i < objects.size(); ++i) {
        for (size_t j = i + 1; j < objects.size(); ++j) {
            ProceduralObject* objA = objects[i];
            ProceduralObject* objB = objects[j];
            
            if (!objA->usePhysics || !objB->usePhysics) continue;
            
            CollisionInfo info;
            info.objectA = objA;
            info.objectB = objB;
            info.hasCollision = false;
            
            Vec3 posA = objA->GetTransform().GetPosition();
            Vec3 posB = objB->GetTransform().GetPosition();
            
            Vec3 diff = {posB.x - posA.x, posB.y - posA.y, posB.z - posA.z};
            float distance = sqrt(Dot(diff, diff));
            
            float radiusA = 1.0f;
            float radiusB = 1.0f;
            
            if (distance < radiusA + radiusB) {
                info.hasCollision = true;
                info.penetrationDepth = (radiusA + radiusB) - distance;
                
                if (distance > 0.001f) {
                    NormalizeVector(diff);
                    info.contactNormal = diff;
                }
                
                collisions.push_back(info);
            }
        }
    }
    
    return collisions;
}

void PhysicsWorld::IntegrateVelocities(float dt) {
    for (auto* obj : objects) {
        if (!obj || !obj->usePhysics) continue;
        
        if (obj->useGravity) {
            obj->velocity.x += gravity.x * dt;
            obj->velocity.y += gravity.y * dt;
            obj->velocity.z += gravity.z * dt;
        }
        
        obj->velocity.x *= damping;
        obj->velocity.y *= damping;
        obj->velocity.z *= damping;
        
        Vec3 pos = obj->GetTransform().GetPosition();
        pos.x += obj->velocity.x * dt;
        pos.y += obj->velocity.y * dt;
        pos.z += obj->velocity.z * dt;
        obj->GetTransform().SetPosition(pos);
        
        if (obj->angularVelocity.x != 0 || 
            obj->angularVelocity.y != 0 || 
            obj->angularVelocity.z != 0) {
            
            Vec3 rotation = obj->GetTransform().GetRotation();
            rotation.x += obj->angularVelocity.x * dt;
            rotation.y += obj->angularVelocity.y * dt;
            rotation.z += obj->angularVelocity.z * dt;
            obj->GetTransform().SetRotation(rotation);
        }
    }
}

void PhysicsWorld::SolveCollisions(const std::vector<CollisionInfo>& collisions) {
    for (const auto& collision : collisions) {
        if (!collision.hasCollision) continue;
        
        ProceduralObject* objA = collision.objectA;
        ProceduralObject* objB = collision.objectB;
        
        if (!objA || !objB) continue;
        
        Vec3 normal = collision.contactNormal;
        
        Vec3 relVel = {
            objB->velocity.x - objA->velocity.x,
            objB->velocity.y - objA->velocity.y,
            objB->velocity.z - objA->velocity.z
        };
        
        float velAlongNormal = Dot(relVel, normal);
        
        if (velAlongNormal > 0) continue;
        
        float restitution = 0.5f;
        float impulseMagnitude = -(1.0f + restitution) * velAlongNormal;
        impulseMagnitude /= 2.0f;
        
        Vec3 impulse = {
            normal.x * impulseMagnitude,
            normal.y * impulseMagnitude,
            normal.z * impulseMagnitude
        };
        
        objA->velocity.x -= impulse.x;
        objA->velocity.y -= impulse.y;
        objA->velocity.z -= impulse.z;
        
        objB->velocity.x += impulse.x;
        objB->velocity.y += impulse.y;
        objB->velocity.z += impulse.z;
        
        float separationAmount = collision.penetrationDepth * 0.5f;
        
        Vec3 posA = objA->GetTransform().GetPosition();
        Vec3 posB = objB->GetTransform().GetPosition();
        
        posA.x -= normal.x * separationAmount;
        posA.y -= normal.y * separationAmount;
        posA.z -= normal.z * separationAmount;
        
        posB.x += normal.x * separationAmount;
        posB.y += normal.y * separationAmount;
        posB.z += normal.z * separationAmount;
        
        objA->GetTransform().SetPosition(posA);
        objB->GetTransform().SetPosition(posB);
    }
}

} // namespace DeliRium