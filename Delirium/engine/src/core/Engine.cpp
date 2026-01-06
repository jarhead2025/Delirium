// =============================================================================
// Engine.cpp - Main engine implementation
// Location: engine/src/core/Engine.cpp
// Part of DeliRium Engine
// =============================================================================

#include "Engine.h"
#include "Renderer.h"
#include "PhysicsWorld.h"
#include <GL/glew.h>
#include <iostream>
#include <chrono>

namespace DeliRium {

Engine& Engine::Instance() {
    static Engine instance;
    return instance;
}

bool Engine::Initialize(int width, int height, const std::string& title) {
    std::cout << "=================================================\n";
    std::cout << "DeliRium Engine v0.1 - Initializing...\n";
    std::cout << "=================================================\n";
    
    windowWidth = width;
    windowHeight = height;
    running = false;
    deltaTime = 0.016f;
    totalTime = 0.0f;
    fixedTimeStep = 0.02f;
    fps = 0.0f;
    frameCount = 0;
    fpsUpdateTimer = 0.0f;
    
    std::cout << "Initializing SDL2... ";
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        std::cerr << "FAILED\n";
        std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
        return false;
    }
    std::cout << "OK\n";
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    
    std::cout << "Creating window (" << width << "x" << height << ")... ";
    window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        std::cerr << "FAILED\n";
        std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }
    std::cout << "OK\n";
    
    std::cout << "Creating OpenGL context... ";
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "FAILED\n";
        std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
    std::cout << "OK\n";
    
    std::cout << "Initializing GLEW... ";
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "FAILED\n";
        std::cerr << "GLEW Error: " << glewGetErrorString(glewError) << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
    std::cout << "OK\n";
    
    std::cout << "\nOpenGL Info:\n";
    std::cout << "  Vendor: " << glGetString(GL_VENDOR) << "\n";
    std::cout << "  Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "  Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "  GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n\n";
    
    SDL_GL_SetSwapInterval(1);
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    std::cout << "Creating renderer... ";
    std::cout << "OK (stub)\n";
    
    std::cout << "Creating physics world... ";
    physicsWorld = std::make_unique<PhysicsWorld>();
    std::cout << "OK\n";
    
    std::cout << "\n=================================================\n";
    std::cout << "Engine initialized successfully!\n";
    std::cout << "=================================================\n\n";
    
    return true;
}

void Engine::Shutdown() {
    std::cout << "Shutting down engine...\n";
    
    renderer.reset();
    physicsWorld.reset();
    currentScene.reset();
    
    if (glContext) {
        SDL_GL_DeleteContext(glContext);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    
    std::cout << "Engine shutdown complete.\n";
}

void Engine::Run() {
    if (!window) {
        std::cerr << "Error: Engine not initialized!\n";
        return;
    }
    
    running = true;
    auto lastTime = std::chrono::high_resolution_clock::now();
    float accumulator = 0.0f;
    
    std::cout << "Starting main loop...\n";
    
    while (running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }
        
        totalTime += deltaTime;
        CalculateFPS(deltaTime);
        
        ProcessInput();
        
        accumulator += deltaTime;
        while (accumulator >= fixedTimeStep) {
            FixedUpdate();
            accumulator -= fixedTimeStep;
        }
        
        Update();
        Render();
    }
    
    std::cout << "Main loop ended.\n";
}

Scene* Engine::CreateScene(const std::string& name) {
    auto scene = std::make_unique<Scene>(name);
    Scene* ptr = scene.get();
    currentScene = std::move(scene);
    return ptr;
}

void Engine::LoadScene(Scene* scene) {
    if (scene) {
        currentScene.reset(scene);
    }
}

void Engine::ProcessInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
                
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                break;
                
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    windowWidth = event.window.data1;
                    windowHeight = event.window.data2;
                    glViewport(0, 0, windowWidth, windowHeight);
                }
                break;
        }
    }
}

void Engine::Update() {
    if (currentScene) {
        currentScene->Update(deltaTime);
    }
}

void Engine::FixedUpdate() {
    if (currentScene) {
        currentScene->FixedUpdate(fixedTimeStep);
    }
    
    if (physicsWorld) {
        physicsWorld->Step(fixedTimeStep);
    }
}

void Engine::Render() {
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (renderer && currentScene) {
        // renderer->RenderScene(*currentScene);
    }
    
    SDL_GL_SwapWindow(window);
    frameCount++;
}

void Engine::CalculateFPS(float dt) {
    fpsUpdateTimer += dt;
    
    if (fpsUpdateTimer >= 1.0f) {
        fps = frameCount / fpsUpdateTimer;
        frameCount = 0;
        fpsUpdateTimer = 0.0f;
        
        std::string title = "DeliRium Engine - FPS: " + std::to_string(static_cast<int>(fps));
        SDL_SetWindowTitle(window, title.c_str());
    }
}

} // namespace DeliRium