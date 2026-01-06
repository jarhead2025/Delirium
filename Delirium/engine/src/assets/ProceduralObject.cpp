// =============================================================================
// ProceduralObject.cpp - GameObject with mathematical surfaces implementation
// Location: engine/src/assets/ProceduralObject.cpp
// Part of DeliRium Engine
// =============================================================================

#include "ProceduralObject.h"
#include "surface_buffer_evaluator.h"
#include "bone_evaluator.h"
#include <iostream>

namespace DeliRium {

ProceduralObject::ProceduralObject(const std::string& name, const std::string& assetPath)
    : GameObject(name)
    , assetPath(assetPath)
    , currentLOD(16)
    , animationTime(0.0f)
{
    if (!assetPath.empty()) {
        if (!LoadFromFile(assetPath)) {
            std::cerr << "Warning: Failed to load asset: " << assetPath << std::endl;
        }
    }
}

bool ProceduralObject::LoadFromFile(const std::string& path) {
    std::cout << "Loading procedural asset: " << path << std::endl;
    
    if (!objectData.LoadFromFile(path)) {
        std::cerr << "Failed to load: " << path << std::endl;
        return false;
    }
    
    std::cout << "  Loaded successfully!" << std::endl;
    std::cout << "    Surfaces: " << objectData.surface_pool.size() << std::endl;
    std::cout << "    Anchors: " << objectData.anchor_pool.size() << std::endl;
    std::cout << "    Bones: " << objectData.bone_pool.size() << std::endl;
    
    UpdateBoneTransforms();
    RegenerateMesh(currentLOD);
    
    return true;
}

void ProceduralObject::RegenerateMesh(int lodLevel) {
    if (objectData.surface_pool.empty()) {
        std::cerr << "Warning: No surfaces to generate mesh from" << std::endl;
        return;
    }
    
    currentLOD = lodLevel;
    
    vertices.clear();
    indices.clear();
    normals.clear();
    colors.clear();
    
    GenerateMeshFromSurfaces();
    
    std::cout << "Mesh regenerated at LOD " << lodLevel 
              << ": " << vertices.size() / 3 << " vertices, "
              << indices.size() / 3 << " triangles" << std::endl;
}

void ProceduralObject::SetLOD(int level) {
    if (level != currentLOD) {
        RegenerateMesh(level);
    }
}

void ProceduralObject::UpdateBoneTransforms() {
    if (objectData.bone_pool.empty()) {
        boneTransforms.clear();
        return;
    }
    
    BoneEvaluator::EvaluateSkeleton(objectData.bone_pool, boneTransforms);
}

void ProceduralObject::UpdateAnimation(float deltaTime) {
    animationTime += deltaTime;
    
    if (!objectData.bone_pool.empty()) {
        UpdateBoneTransforms();
        RegenerateMesh(currentLOD);
    }
}

void ProceduralObject::Update(float deltaTime) {
    GameObject::Update(deltaTime);
    UpdateAnimation(deltaTime);
}

void ProceduralObject::FixedUpdate(float fixedDeltaTime) {
    GameObject::FixedUpdate(fixedDeltaTime);
}

void ProceduralObject::GenerateMeshFromSurfaces() {
    std::vector<float> surfaceVertices;
    std::vector<uint32_t> surfaceIndices;
    std::vector<float> surfaceNormals;
    
    for (const auto& surface : objectData.surface_pool) {
        surfaceVertices.clear();
        surfaceIndices.clear();
        surfaceNormals.clear();
        
        SurfaceBufferEvaluator::EvaluateSurfaceMesh(
            &surface,
            objectData.anchor_pool,
            boneTransforms,
            currentLOD,
            surfaceVertices,
            surfaceIndices,
            surfaceNormals,
            animationTime
        );
        
        uint32_t vertexOffset = vertices.size() / 3;
        for (uint32_t idx : surfaceIndices) {
            indices.push_back(idx + vertexOffset);
        }
        
        vertices.insert(vertices.end(), surfaceVertices.begin(), surfaceVertices.end());
        normals.insert(normals.end(), surfaceNormals.begin(), surfaceNormals.end());
        
        for (size_t i = 0; i < surfaceVertices.size(); i += 3) {
            float r = static_cast<float>(surface.color_param_r) / 255.0f;
            float g = static_cast<float>(surface.color_param_g) / 255.0f;
            float b = static_cast<float>(surface.color_param_b) / 255.0f;
            
            colors.push_back(r);
            colors.push_back(g);
            colors.push_back(b);
        }
    }
}

} // namespace DeliRium