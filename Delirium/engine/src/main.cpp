// =============================================================================
// main.cpp - DeliRium Engine Test Program
// Location: engine/src/main.cpp
// =============================================================================

#include "Engine.h"
#include <iostream>

int main(int argc, char* argv[]) {
    using namespace DeliRium;
    
    Engine& engine = Engine::Instance();
    if (!engine.Initialize(1280, 720, "DeliRium Engine v0.1")) {
        std::cerr << "Failed to initialize engine!\n";
        return -1;
    }
    
    Scene* scene = engine.CreateScene("TestScene");
    
    auto testObject = scene->CreateGameObject("TestCube");
    testObject->GetTransform().SetPosition({0, 0, -5});
    
    std::cout << "\n=== TEST SCENE ===\n";
    std::cout << "Created scene: " << scene->sceneName << "\n";
    std::cout << "Objects: " << scene->GetRootObjects().size() << "\n";
    std::cout << "  - " << testObject->name << " at position ("
              << testObject->GetTransform().GetPosition().x << ", "
              << testObject->GetTransform().GetPosition().y << ", "
              << testObject->GetTransform().GetPosition().z << ")\n";
    std::cout << "\nCamera position: ("
              << scene->GetCamera().position.x << ", "
              << scene->GetCamera().position.y << ", "
              << scene->GetCamera().position.z << ")\n";
    std::cout << "\nPress ESC or close window to exit.\n\n";
    
    engine.Run();
    
    engine.Shutdown();
    
    return 0;
}