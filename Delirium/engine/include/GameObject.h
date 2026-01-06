// =============================================================================
// GameObject.h - Base game object class
// Location: engine/include/delirium/GameObject.h
// Part of DeliRium Engine
// =============================================================================

#pragma once
#include "Transform.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace DeliRium {

class GameObject {
public:
    GameObject(const std::string& name);
    virtual ~GameObject() = default;
    
    // Hierarchy management
    void SetParent(GameObject* parent);
    void AddChild(std::shared_ptr<GameObject> child);
    void RemoveChild(GameObject* child);
    
    GameObject* GetParent() { return parent; }
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return children; }
    
    // Transform access
    Transform& GetTransform() { return transform; }
    const Transform& GetTransform() const { return transform; }
    
    // World matrix (includes parent transforms)
    Matrix4x4 GetWorldMatrix();
    void MarkWorldMatrixDirty() { worldMatrixDirty = true; }
    
    // Lifecycle callbacks (override in derived classes)
    virtual void Update(float deltaTime) {}
    virtual void FixedUpdate(float fixedDeltaTime) {}
    
    // Public properties
    std::string name;
    bool active;
    uint32_t id;
    
protected:
    Transform transform;
    GameObject* parent;
    std::vector<std::shared_ptr<GameObject>> children;
    
    // Cached world matrix
    Matrix4x4 cachedWorldMatrix;
    bool worldMatrixDirty;
};

} // namespace DeliRium