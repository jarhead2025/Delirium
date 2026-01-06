// =============================================================================
// AssetManager.h - Asset loading and caching system
// Location: engine/include/delirium/AssetManager.h
// Part of DeliRium Engine
// =============================================================================

#pragma once
#include "object_descriptor_buffers.h"
#include <string>
#include <unordered_map>

namespace DeliRium {

class AssetManager {
public:
    static AssetManager& Instance();
    
    ProceduralObjectCollection* LoadProceduralAsset(const std::string& path);
    void UnloadAsset(const std::string& path);
    void UnloadAll();
    
    bool IsLoaded(const std::string& path) const;
    size_t GetCacheSize() const { return assetCache.size(); }
    void PrintCacheInfo() const;
    
    static std::string GetAssetsDirectory() { return "assets/"; }
    static std::string GetModelsDirectory() { return "assets/models/"; }
    
private:
    AssetManager() = default;
    ~AssetManager() = default;
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    
    std::unordered_map<std::string, ProceduralObjectCollection> assetCache;
};

} // namespace DeliRium