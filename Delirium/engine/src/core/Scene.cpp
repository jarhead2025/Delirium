// =============================================================================
// Scene.cpp - Scene graph implementation
// Location: engine/src/core/Scene.cpp
// Part of DeliRium Engine
// =============================================================================

#include "Scene.h"
#include "ProceduralObject.h"
#include <cmath>
#include <iostream>
#include <algorithm>
namespace DeliRium {

Scene::Scene(const std::string& name)
    : sceneName(name)
    , nextObjectId(1)
{
}

std::shared_ptr<GameObject> Scene::CreateGameObject(const std::string& name) {
    auto obj = std::make_shared<GameObject>(name);
    obj->id = nextObjectId++;
    
    RegisterObject(obj.get());
    rootObjects.push_back(obj);
    
    return obj;
}

std::shared_ptr<ProceduralObject> Scene::CreateProceduralObject(
    const std::string& name,
    const std::string& assetPath)
{
    auto obj = std::make_shared<ProceduralObject>(name, assetPath);
    obj->id = nextObjectId++;
    
    RegisterObject(obj.get());
    rootObjects.push_back(obj);
    
    return obj;
}

void Scene::DestroyObject(uint32_t id) {
    auto it = objectRegistry.find(id);
    if (it == objectRegistry.end()) return;
    
    GameObject* obj = it->second;
    
    if (obj->GetParent()) {
        obj->GetParent()->RemoveChild(obj);
    } else {
        rootObjects.erase(
            std::remove_if(rootObjects.begin(), rootObjects.end(),
                [id](const std::shared_ptr<GameObject>& o) {
                    return o->id == id;
                }),
            rootObjects.end()
        );
    }
    
    UnregisterObject(id);
}

GameObject* Scene::FindObjectByName(const std::string& name) {
    for (auto& obj : rootObjects) {
        if (obj->name == name) return obj.get();
    }
    return nullptr;
}

GameObject* Scene::FindObjectById(uint32_t id) {
    auto it = objectRegistry.find(id);
    return (it != objectRegistry.end()) ? it->second : nullptr;
}

void Scene::Update(float deltaTime) {
    for (auto& obj : rootObjects) {
        if (obj->active) {
            obj->Update(deltaTime);
            
            for (auto& child : obj->GetChildren()) {
                if (child->active) {
                    child->Update(deltaTime);
                }
            }
        }
    }
}

void Scene::FixedUpdate(float fixedDeltaTime) {
    for (auto& obj : rootObjects) {
        if (obj->active) {
            obj->FixedUpdate(fixedDeltaTime);
            
            for (auto& child : obj->GetChildren()) {
                if (child->active) {
                    child->FixedUpdate(fixedDeltaTime);
                }
            }
        }
    }
}

void Scene::RegisterObject(GameObject* obj) {
    if (obj) {
        objectRegistry[obj->id] = obj;
    }
}

void Scene::UnregisterObject(uint32_t id) {
    objectRegistry.erase(id);
}

Matrix4x4 Scene::Camera::GetViewMatrix() const {
    Vec3 forward = {
        target.x - position.x,
        target.y - position.y,
        target.z - position.z
    };
    NormalizeVector(forward);
    
    Vec3 right = Cross(forward, up);
    NormalizeVector(right);
    
    Vec3 realUp = Cross(right, forward);
    
    Matrix4x4 view;
    view.m[0] = right.x;
    view.m[1] = realUp.x;
    view.m[2] = -forward.x;
    view.m[3] = 0;
    
    view.m[4] = right.y;
    view.m[5] = realUp.y;
    view.m[6] = -forward.y;
    view.m[7] = 0;
    
    view.m[8] = right.z;
    view.m[9] = realUp.z;
    view.m[10] = -forward.z;
    view.m[11] = 0;
    
    view.m[12] = -Dot(right, position);
    view.m[13] = -Dot(realUp, position);
    view.m[14] = Dot(forward, position);
    view.m[15] = 1;
    
    return view;
}

Matrix4x4 Scene::Camera::GetProjectionMatrix(float aspectRatio) const {
    float tanHalfFov = tan((fov * M_PI / 180.0f) * 0.5f);
    
    Matrix4x4 proj;
    for (int i = 0; i < 16; ++i) proj.m[i] = 0.0f;
    
    proj.m[0] = 1.0f / (aspectRatio * tanHalfFov);
    proj.m[5] = 1.0f / tanHalfFov;
    proj.m[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    proj.m[11] = -1.0f;
    proj.m[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    
    return proj;
}

} // namespace DeliRium