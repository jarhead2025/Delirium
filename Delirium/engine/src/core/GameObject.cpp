// =============================================================================
// GameObject.cpp - Base game object implementation
// Location: engine/src/core/GameObject.cpp
// Part of DeliRium Engine
// =============================================================================

#include "GameObject.h"
#include <algorithm>

namespace DeliRium {

GameObject::GameObject(const std::string& name)
    : name(name)
    , active(true)
    , id(0)
    , parent(nullptr)
    , worldMatrixDirty(true)
{
    // Initialize world matrix to identity
    for (int i = 0; i < 16; ++i) {
        cachedWorldMatrix.m[i] = 0.0f;
    }
    cachedWorldMatrix.m[0] = cachedWorldMatrix.m[5] = 
    cachedWorldMatrix.m[10] = cachedWorldMatrix.m[15] = 1.0f;
}

void GameObject::SetParent(GameObject* newParent) {
    if (parent == newParent) return;
    
    // Remove from old parent's children list
    if (parent) {
        auto& siblings = parent->children;
        siblings.erase(
            std::remove_if(siblings.begin(), siblings.end(),
                [this](const std::shared_ptr<GameObject>& child) {
                    return child.get() == this;
                }),
            siblings.end()
        );
    }
    
    parent = newParent;
    worldMatrixDirty = true;
    
    // Note: Adding to new parent's children is handled by AddChild()
}

void GameObject::AddChild(std::shared_ptr<GameObject> child) {
    if (!child) return;
    
    // Prevent adding self as child
    if (child.get() == this) return;
    
    // Prevent circular dependencies
    GameObject* ancestor = parent;
    while (ancestor) {
        if (ancestor == child.get()) {
            return;  // Would create circular reference
        }
        ancestor = ancestor->parent;
    }
    
    child->SetParent(this);
    children.push_back(child);
}

void GameObject::RemoveChild(GameObject* child) {
    if (!child) return;
    
    children.erase(
        std::remove_if(children.begin(), children.end(),
            [child](const std::shared_ptr<GameObject>& c) {
                return c.get() == child;
            }),
        children.end()
    );
    
    child->parent = nullptr;
    child->worldMatrixDirty = true;
}

Matrix4x4 GameObject::GetWorldMatrix() {
    if (worldMatrixDirty) {
        Matrix4x4 localMat = transform.GetMatrix();
        
        if (parent) {
            Matrix4x4 parentMat = parent->GetWorldMatrix();
            cachedWorldMatrix = MultiplyMatrices(parentMat, localMat);
        } else {
            cachedWorldMatrix = localMat;
        }
        
        worldMatrixDirty = false;
    }
    
    return cachedWorldMatrix;
}

} // namespace DeliRium