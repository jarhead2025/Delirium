// =============================================================================
// AssetManager.cpp - Asset loading and caching implementation
// Location: engine/src/assets/AssetManager.cpp
// Part of DeliRium Engine
// =============================================================================

#include "AssetManager.h"
#include <iostream>

namespace DeliRium {

AssetManager& AssetManager::Instance() {
    static AssetManager instance;
    return instance;
}

ProceduralObjectCollection* AssetManager::LoadProceduralAsset(const std::string& path) {
    auto it = assetCache.find(path);
    if (it != assetCache.end()) {
        std::cout << "Asset already cached: " << path << std::endl;
        return &it->second;
    }
    
    std::cout << "Loading asset from disk: " << path << std::endl;
    
    ProceduralObjectCollection collection;
    if (!collection.LoadFromFile(path)) {
        std::cerr << "Failed to load asset: " << path << std::endl;
        return nullptr;
    }
    
    if (!collection.Validate()) {
        std::cerr << "Asset validation failed: " << path << std::endl;
        return nullptr;
    }
    
    assetCache[path] = std::move(collection);
    
    std::cout << "  Successfully loaded and cached!" << std::endl;
    std::cout << "    File size: " << collection.CalculateFileSize() << " bytes" << std::endl;
    std::cout << "    Objects: " << collection.object_descriptors.size() << std::endl;
    std::cout << "    Surfaces: " << collection.surface_pool.size() << std::endl;
    std::cout << "    Anchors: " << collection.anchor_pool.size() << std::endl;
    std::cout << "    Bones: " << collection.bone_pool.size() << std::endl;
    
    return &assetCache[path];
}

void AssetManager::UnloadAsset(const std::string& path) {
    auto it = assetCache.find(path);
    if (it != assetCache.end()) {
        std::cout << "Unloading asset: " << path << std::endl;
        assetCache.erase(it);
    }
}

void AssetManager::UnloadAll() {
    std::cout << "Unloading all assets (" << assetCache.size() << " cached)" << std::endl;
    assetCache.clear();
}

bool AssetManager::IsLoaded(const std::string& path) const {
    return assetCache.find(path) != assetCache.end();
}

void AssetManager::PrintCacheInfo() const {
    std::cout << "\n=== Asset Manager Cache ===\n";
    std::cout << "Total assets cached: " << assetCache.size() << "\n";
    
    if (assetCache.empty()) {
        std::cout << "  (No assets loaded)\n";
        return;
    }
    
    size_t totalMemory = 0;
    for (const auto& pair : assetCache) {
        size_t assetSize = pair.second.CalculateFileSize();
        totalMemory += assetSize;
        
        std::cout << "  " << pair.first << "\n";
        std::cout << "    Size: " << assetSize << " bytes\n";
        std::cout << "    Surfaces: " << pair.second.surface_pool.size() << "\n";
    }
    
    std::cout << "Total memory: " << totalMemory << " bytes ("
              << (totalMemory / 1024.0f) << " KB)\n";
    std::cout << "===========================\n\n";
}

} // namespace DeliRium