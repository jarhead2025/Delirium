// File: src/evaluators/psf_opengl_viewer_stub.cpp
// Minimal stub implementation to satisfy linker when OpenGL viewer is not available

#include "../core/object_descriptor_buffers.h"
#include <iostream>
#include <string>

// Minimal stub structures to match the interface
struct PSFOpenGLViewer {
    struct ViewerConfig {
        int window_width = 1280;
        int window_height = 720;
        const char* window_title = "PSF Viewer";
        bool show_fps = true;
        bool show_stats = true;
        float point_size = 2.0f;
        float camera_speed = 5.0f;
        float mouse_sensitivity = 0.1f;
        bool enable_vsync = true;
        bool enable_mouse_controls = true;
        bool enable_keyboard_controls = true;
    };
};

// Stub implementation that provides helpful feedback
class PSFViewerIntegration {
public:
    static bool ViewPSFFile(const std::string& psf_filename, 
                           const PSFOpenGLViewer::ViewerConfig& config = PSFOpenGLViewer::ViewerConfig{}) {
        std::cout << "\n=== PSF OpenGL Viewer (Stub) ===\n";
        std::cout << "❌ OpenGL viewer not available in this build\n";
        std::cout << "\nRequested file: " << psf_filename << "\n";
        std::cout << "Window config: " << config.window_width << "x" << config.window_height << "\n";
        std::cout << "Point size: " << config.point_size << "\n";
        std::cout << "Show FPS: " << (config.show_fps ? "Yes" : "No") << "\n";
        
        std::cout << "\n🔧 To enable full OpenGL viewer:\n";
        std::cout << "1. Install OpenGL dependencies:\n";
        std::cout << "   - Download GLEW from: http://glew.sourceforge.net/\n";
        std::cout << "   - Download GLFW from: https://www.glfw.org/download.html\n";
        std::cout << "   - Download GLM from: https://github.com/g-truc/glm\n";
        std::cout << "   - Extract all to your 'lib' folder\n";
        std::cout << "\n2. Copy OpenGL viewer files:\n";
        std::cout << "   - psf_opengl_viewer.h -> src/evaluators/\n";
        std::cout << "   - psf_opengl_viewer.cpp -> src/evaluators/\n";
        std::cout << "\n3. Rebuild the project:\n";
        std::cout << "   cd build && cmake .. && make\n";
        
        std::cout << "\n🔄 Alternative: Use console viewer\n";
        std::cout << "Command: " << "procedural_engine --view " << psf_filename << "\n";
        
        std::cout << "\n📁 Expected lib folder structure:\n";
        std::cout << "lib/\n";
        std::cout << "├── glew/\n";
        std::cout << "│   ├── include/GL/glew.h\n";
        std::cout << "│   └── lib/Release/x64/glew32s.lib\n";
        std::cout << "├── glfw3/\n";
        std::cout << "│   ├── include/GLFW/glfw3.h\n";
        std::cout << "│   └── lib-vc2022/glfw3.lib\n";
        std::cout << "└── glm/\n";
        std::cout << "    └── glm/glm.hpp\n";
        
        return false;  // Indicate that actual viewing didn't happen
    }
};