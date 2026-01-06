// =============================================================================
// Engine.h - Main engine class
// Location: engine/include/delirium/Engine.h
// Part of DeliRium Engine
// =============================================================================

#pragma once
#include "Scene.h"
#include <memory>
#include <string>
#include <SDL2/SDL.h>

namespace DeliRium {

class Renderer;
class PhysicsWorld;

class Engine {
public:
    static Engine& Instance();
    
    bool Initialize(int width, int height, const std::string& title);
    void Shutdown();
    
    void Run();
    void Stop() { running = false; }
    bool IsRunning() const { return running; }
    
    Scene* CreateScene(const std::string& name);
    void LoadScene(Scene* scene);
    Scene* GetCurrentScene() { return currentScene.get(); }
    
    Renderer* GetRenderer() { return renderer.get(); }
    PhysicsWorld* GetPhysicsWorld() { return physicsWorld.get(); }
    
    float GetDeltaTime() const { return deltaTime; }
    float GetTime() const { return totalTime; }
    float GetFPS() const { return fps; }
    
    SDL_Window* GetWindow() { return window; }
    int GetWindowWidth() const { return windowWidth; }
    int GetWindowHeight() const { return windowHeight; }
    
private:
    Engine() = default;
    ~Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    
    SDL_Window* window;
    SDL_GLContext glContext;
    int windowWidth;
    int windowHeight;
    
    std::unique_ptr<Scene> currentScene;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<PhysicsWorld> physicsWorld;
    
    bool running;
    float deltaTime;
    float totalTime;
    float fixedTimeStep;
    float fps;
    uint32_t frameCount;
    float fpsUpdateTimer;
    
    void ProcessInput();
    void Update();
    void FixedUpdate();
    void Render();
    void CalculateFPS(float dt);
};

} // namespace DeliRium
