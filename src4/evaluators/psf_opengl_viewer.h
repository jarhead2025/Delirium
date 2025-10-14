#pragma once

// PSF OpenGL Viewer - Lightweight visualization for PSF files
// Dependencies: GLEW, GLFW3, GLM

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include "psf_realtime_renderer.h"
#include "object_descriptor_buffers.h"
#include <vector>
#include <memory>
#include <string>
#include <chrono>

// ================================
// PSF OPENGL VIEWER CLASS
// ================================

class PSFOpenGLViewer {
public:
    struct ViewerConfig {
        int window_width = 1280;
        int window_height = 720;
        const char* window_title = "PSF Viewer - Real-time Procedural Surface Visualization";
        bool enable_vsync = true;
        bool show_fps = true;
        bool show_stats = true;
        bool enable_mouse_controls = true;
        bool enable_keyboard_controls = true;
        float point_size = 2.0f;
        float camera_speed = 5.0f;
        float mouse_sensitivity = 0.1f;
    };

    struct ViewerStats {
        float fps = 0.0f;
        float frame_time_ms = 0.0f;
        float render_time_ms = 0.0f;
        uint32_t visible_points = 0;
        uint32_t total_surfaces = 0;
        uint32_t total_anchors = 0;
        bool is_loaded = false;
    };

    PSFOpenGLViewer(const ViewerConfig& config = ViewerConfig{});
    ~PSFOpenGLViewer();

    // Main functions
    bool Initialize();
    bool LoadPSFFile(const std::string& filename);
    void Run(); // Main render loop
    void Shutdown();

    // Configuration
    void UpdateConfig(const ViewerConfig& new_config);
    const ViewerStats& GetStats() const { return m_stats; }

    // Camera controls
    void SetCameraPosition(const glm::vec3& position);
    void SetCameraTarget(const glm::vec3& target);
    void ResetCamera();

private:
    // OpenGL setup
    bool CreateWindow();
    bool InitializeOpenGL();
    bool CreateShaders();
    bool CreateBuffers();
    void CleanupOpenGL();

    // Rendering
    void RenderFrame();
    void UpdateCamera(float delta_time);
    void RenderPoints();
    void RenderUI();
    void UpdateStats(float delta_time);

    // Input handling
    void HandleInput(float delta_time);
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

    // PSF integration
    bool ConvertPSFToPointCloud();
    void UpdatePSFRenderer(float delta_time);

    // Configuration and state
    ViewerConfig m_config;
    ViewerStats m_stats;
    
    // OpenGL objects
    GLFWwindow* m_window = nullptr;
    GLuint m_shader_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo_positions = 0;
    GLuint m_vbo_colors = 0;
    GLuint m_vbo_normals = 0;
    
    // Shader uniforms
    GLint m_uniform_mvp = -1;
    GLint m_uniform_point_size = -1;
    GLint m_uniform_camera_pos = -1;
    
    // Camera state
    glm::vec3 m_camera_pos = glm::vec3(5.0f, 5.0f, 5.0f);
    glm::vec3 m_camera_target = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 m_camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
    float m_camera_yaw = -90.0f;
    float m_camera_pitch = 0.0f;
    float m_camera_distance = 10.0f;
    
    // Input state
    bool m_first_mouse = true;
    double m_last_mouse_x = 0.0;
    double m_last_mouse_y = 0.0;
    bool m_keys[1024] = {};
    
    // PSF data
    ProceduralObjectCollection m_psf_collection;
    std::unique_ptr<PSFRealtimeRenderer> m_psf_renderer;
    std::vector<PSFRealtimePoint> m_point_cloud;
    std::vector<PSFRealtimeLight> m_lights;
    
    // Timing
    std::chrono::high_resolution_clock::time_point m_last_frame_time;
    float m_frame_timer = 0.0f;
    int m_frame_count = 0;
    
    // OpenGL point data
    std::vector<float> m_gl_positions;
    std::vector<float> m_gl_colors;
    std::vector<float> m_gl_normals;
    size_t m_point_count = 0;
};

// ================================
// UTILITY FUNCTIONS
// ================================

class PSFViewerUtils {
public:
    // Shader compilation helpers
    static GLuint CompileShader(const std::string& source, GLenum shader_type);
    static GLuint CreateShaderProgram(const std::string& vertex_source, const std::string& fragment_source);
    
    // Error handling
    static bool CheckGLError(const std::string& operation);
    static void PrintOpenGLInfo();
    
    // PSF file utilities
    static bool ValidatePSFFile(const std::string& filename);
    static std::string GetPSFFileInfo(const std::string& filename);
};

// ================================
// INTEGRATION HELPER
// ================================

class PSFViewerIntegration {
public:
    // Easy integration with your main.cpp
    static bool ViewPSFFile(const std::string& psf_filename, 
                           const PSFOpenGLViewer::ViewerConfig& config = PSFOpenGLViewer::ViewerConfig{});
    
    // Batch visualization
    static bool ComparePSFFiles(const std::vector<std::string>& psf_filenames);
    
    // Export frame as image (if you need screenshots)
    static bool ExportFrame(const std::string& output_filename, int width, int height);
};

// Default shader sources
namespace PSFShaders {
    extern const char* VERTEX_SHADER_SOURCE;
    extern const char* FRAGMENT_SHADER_SOURCE;
}