// =============================================================================
// Renderer.h - Abstract renderer base class
// Location: engine/include/delirium/Renderer.h
// Part of DeliRium Engine
// =============================================================================

#pragma once
#include "Scene.h"

namespace DeliRium {

class Renderer {
public:
    virtual ~Renderer() = default;
    
    virtual bool Initialize(int width, int height) = 0;
    virtual void Shutdown() = 0;
    
    virtual void BeginFrame() = 0;
    virtual void RenderScene(Scene& scene) = 0;
    virtual void EndFrame() = 0;
    
    void SetWindowSize(int width, int height) {
        windowWidth = width;
        windowHeight = height;
        OnResize(width, height);
    }
    
    int GetWindowWidth() const { return windowWidth; }
    int GetWindowHeight() const { return windowHeight; }
    
protected:
    int windowWidth = 1280;
    int windowHeight = 720;
    
    virtual void OnResize(int width, int height) {}
};

} // namespace DeliRium
