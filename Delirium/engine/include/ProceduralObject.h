// =============================================================================
// ProceduralObject.h - GameObject with mathematical surface data
// Location: engine/include/delirium/ProceduralObject.h
// Part of DeliRium Engine
// =============================================================================

#pragma once
#include "GameObject.h"
#include "object_descriptor_buffers.h"
#include <vector>
#include <string>

namespace DeliRium {

class ProceduralObject : public GameObject {
public:
    ProceduralObject(const std::string& name, const std::string& assetPath);
    ~ProceduralObject() = default;
    
    bool LoadFromFile(const std::string& path);
    const std::string& GetAssetPath() const { return assetPath; }
    
    ProceduralObjectCollection& GetObjectData() { return objectData; }
    const ProceduralObjectCollection& GetObjectData() const { return objectData; }
    
    void RegenerateMesh(int lodLevel = 16);
    void SetLOD(int level);
    int GetLOD() const { return currentLOD; }
    
    const std::vector<float>& GetVertices() const { return vertices; }
    const std::vector<uint32_t>& GetIndices() const { return indices; }
    const std::vector<float>& GetNormals() const { return normals; }
    const std::vector<float>& GetColors() const { return colors; }
    bool HasMeshData() const { return !vertices.empty() && !indices.empty(); }
    
    void UpdateAnimation(float deltaTime);
    void UpdateBoneTransforms();
    const std::vector<Matrix4x4>& GetBoneTransforms() const { return boneTransforms; }
    
    Vec3 velocity = {0, 0, 0};
    Vec3 angularVelocity = {0, 0, 0};
    bool useGravity = true;
    bool usePhysics = false;
    float mass = 1.0f;
    
    void Update(float deltaTime) override;
    void FixedUpdate(float fixedDeltaTime) override;
    
private:
    std::string assetPath;
    ProceduralObjectCollection objectData;
    std::vector<Matrix4x4> boneTransforms;
    
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::vector<float> normals;
    std::vector<float> colors;
    
    int currentLOD;
    float animationTime;
    
    void GenerateMeshFromSurfaces();
};

} // namespace DeliRium