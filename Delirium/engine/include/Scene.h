// =============================================================================
// Scene.h - Scene graph management
// Location: engine/include/delirium/Scene.h
// Part of DeliRium Engine
// =============================================================================

#pragma once
#include "GameObject.h"
#include "math_types.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace DeliRium {

// Forward declaration
class ProceduralObject;

class Scene {
public:
    Scene(const std::string& name);
    ~Scene() = default;
    
    // Object management
    std::shared_ptr<GameObject> CreateGameObject(const std::string& name);
    std::shared_ptr<ProceduralObject> CreateProceduralObject(
        const std::string& name,
        const std::string& assetPath
    );
    
    void DestroyObject(uint32_t id);
    GameObject* FindObjectByName(const std::string& name);
    GameObject* FindObjectById(uint32_t id);
    
    // Scene lifecycle
    void Update(float deltaTime);
    void FixedUpdate(float fixedDeltaTime);
    
    // Scene data access
    const std::vector<std::shared_ptr<GameObject>>& GetRootObjects() const { 
        return rootObjects; 
    }
    
    // Camera system
    struct Camera {
        Vec3 position = {0, 0, 5};
        Vec3 target = {0, 0, 0};
        Vec3 up = {0, 1, 0};
        float fov = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        
        Matrix4x4 GetViewMatrix() const;
        Matrix4x4 GetProjectionMatrix(float aspectRatio) const;
    };
    
    Camera& GetCamera() { return camera; }
    const Camera& GetCamera() const { return camera; }
    
    // Properties
    std::string sceneName;
    
private:
    std::vector<std::shared_ptr<GameObject>> rootObjects;
    std::unordered_map<uint32_t, GameObject*> objectRegistry;
    Camera camera;
    uint32_t nextObjectId;
    
    void RegisterObject(GameObject* obj);
    void UnregisterObject(uint32_t id);
};

} // namespace DeliRium