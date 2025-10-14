#include "psf_opengl_viewer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

// ================================
// SHADER SOURCES
// ================================

namespace PSFShaders {
    const char* VERTEX_SHADER_SOURCE = R"(
        #version 330 core
        
        layout (location = 0) in vec3 position;
        layout (location = 1) in vec3 color;
        layout (location = 2) in vec3 normal;
        
        uniform mat4 u_mvp;
        uniform float u_point_size;
        uniform vec3 u_camera_pos;
        
        out vec3 v_color;
        out vec3 v_normal;
        out float v_distance;
        
        void main() {
            gl_Position = u_mvp * vec4(position, 1.0);
            gl_PointSize = u_point_size;
            
            v_color = color;
            v_normal = normalize(normal);
            v_distance = distance(position, u_camera_pos);
        }
    )";
    
const char* FRAGMENT_SHADER_SOURCE = R"(
    #version 330 core
    
    in vec3 v_color;
    in vec3 v_normal;
    in float v_distance;
    
    out vec4 fragColor;
    
    void main() {
        // FIXED: Preserve color accuracy with minimal lighting
        vec3 light_dir = normalize(vec3(1.0, 1.0, 1.0));
        float light_factor = 0.9 + 0.1 * max(dot(v_normal, light_dir), 0.0);
        
        // Very minimal distance fading
        float fade = 1.0 / (1.0 + v_distance * 0.0001);
        fade = max(fade, 0.95); // Never fade below 95%
        
        // Preserve the mathematical colors with minimal modification
        vec3 final_color = v_color * light_factor * fade;
        
        // Ensure visibility of dark colors
        if (dot(final_color, final_color) < 0.1) {
            final_color = final_color * 2.0 + vec3(0.1, 0.1, 0.1);
        }
        
        fragColor = vec4(clamp(final_color, 0.0, 1.0), 1.0);
    }
)";

}

// ================================
// PSFOpenGLViewer Implementation
// ================================

PSFOpenGLViewer::PSFOpenGLViewer(const ViewerConfig& config) 
    : m_config(config) {
    
    std::cout << "PSFOpenGLViewer constructor called" << std::endl;
    
    m_last_frame_time = std::chrono::high_resolution_clock::now();
    
    // Initialize PSF renderer with optimized config
PSFRealtimeConfig psf_config;
psf_config.max_points_per_surface = 2000;
psf_config.enable_global_illumination = false;  // ← DISABLE THIS
psf_config.debug_timing = true;
psf_config.min_point_spacing = 0.02f;
psf_config.max_point_spacing = 1.0f;
psf_config.lod_distance_factor = 30.0f;
psf_config.ambient_intensity = 1.0f;  // ← SET TO 100% (no dimming)
    
    m_psf_renderer = std::make_unique<PSFRealtimeRenderer>(psf_config);
    
    // Setup default lighting
    float light_pos1[3] = {5.0f, 5.0f, 5.0f};
    float light_pos2[3] = {-5.0f, 5.0f, -5.0f};
 float light_color1[3] = {0.3f, 0.3f, 0.3f};  // Dim white
float light_color2[3] = {0.2f, 0.2f, 0.3f};  // Very dim blue

m_lights = {
    PSFRealtimeRenderer::PSFRealtimeCreatePointLight(light_pos1, light_color1, 0.5f, 20.0f),  // Low intensity
    PSFRealtimeRenderer::PSFRealtimeCreatePointLight(light_pos2, light_color2, 0.3f, 15.0f)   // Very low intensity
};
    
    std::cout << "PSFOpenGLViewer constructor completed" << std::endl;
}

PSFOpenGLViewer::~PSFOpenGLViewer() {
    Shutdown();
}

bool PSFOpenGLViewer::Initialize() {
    std::cout << "Initializing PSFOpenGLViewer..." << std::endl;
    
    if (!CreateWindow()) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }
    
    if (!InitializeOpenGL()) {
        std::cerr << "Failed to initialize OpenGL" << std::endl;
        return false;
    }
    
    if (!CreateShaders()) {
        std::cerr << "Failed to create shaders" << std::endl;
        return false;
    }
    
    if (!CreateBuffers()) {
        std::cerr << "Failed to create buffers" << std::endl;
        return false;
    }
    
    PSFViewerUtils::PrintOpenGLInfo();
    std::cout << "PSF OpenGL Viewer initialized successfully!" << std::endl;
    
    return true;
}

bool PSFOpenGLViewer::CreateWindow() {
    std::cout << "Creating GLFW window..." << std::endl;
    
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    // Window hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
    
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    
    std::cout << "Creating window: " << m_config.window_width << "x" << m_config.window_height << std::endl;
    
    m_window = glfwCreateWindow(m_config.window_width, m_config.window_height, 
                               m_config.window_title, nullptr, nullptr);
    
    if (!m_window) {
        std::cerr << "Failed to create GLFW window, trying fallback..." << std::endl;
        
        // Try fallback settings
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
        
        m_window = glfwCreateWindow(800, 600, "PSF Viewer (Fallback)", nullptr, nullptr);
        
        if (!m_window) {
            std::cerr << "Failed to create fallback window" << std::endl;
            glfwTerminate();
            return false;
        }
        
        std::cout << "Created fallback window (800x600)" << std::endl;
    } else {
        std::cout << "Created main window successfully" << std::endl;
    }
    
    glfwMakeContextCurrent(m_window);
    glfwShowWindow(m_window);
    glfwFocusWindow(m_window);
    
    // Set callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwSetCursorPosCallback(m_window, MouseCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);
    glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
    
    glfwSwapInterval(m_config.enable_vsync ? 1 : 0);
    
    std::cout << "Window creation completed" << std::endl;
    return true;
}

bool PSFOpenGLViewer::InitializeOpenGL() {
    std::cout << "Initializing OpenGL..." << std::endl;
    
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    
    std::cout << "OpenGL initialized successfully" << std::endl;
    return true;
}

bool PSFOpenGLViewer::CreateShaders() {
    std::cout << "Creating shaders..." << std::endl;
    
    m_shader_program = PSFViewerUtils::CreateShaderProgram(
        PSFShaders::VERTEX_SHADER_SOURCE, 
        PSFShaders::FRAGMENT_SHADER_SOURCE
    );
    
    if (m_shader_program == 0) {
        std::cerr << "Failed to create shader program" << std::endl;
        return false;
    }
    
    m_uniform_mvp = glGetUniformLocation(m_shader_program, "u_mvp");
    m_uniform_point_size = glGetUniformLocation(m_shader_program, "u_point_size");
    m_uniform_camera_pos = glGetUniformLocation(m_shader_program, "u_camera_pos");
    
    std::cout << "Shaders created successfully" << std::endl;
    return true;
}

bool PSFOpenGLViewer::CreateBuffers() {
    std::cout << "Creating OpenGL buffers..." << std::endl;
    
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo_positions);
    glGenBuffers(1, &m_vbo_colors);
    glGenBuffers(1, &m_vbo_normals);
    
    glBindVertexArray(m_vao);
    
    // Position buffer (location 0)
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_positions);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    
    // Color buffer (location 1)
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_colors);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);
    
    // Normal buffer (location 2)
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_normals);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
    
    std::cout << "OpenGL buffers created successfully" << std::endl;
    return PSFViewerUtils::CheckGLError("Create buffers");
}

bool PSFOpenGLViewer::LoadPSFFile(const std::string& filename) {
    std::cout << "Loading PSF file: " << filename << std::endl;
    
    if (!PSFViewerUtils::ValidatePSFFile(filename)) {
        std::cerr << "Invalid PSF file: " << filename << std::endl;
        return false;
    }
    
    if (!m_psf_collection.LoadFromFile(filename)) {
        std::cerr << "Failed to load PSF file: " << filename << std::endl;
        return false;
    }
    
    std::cout << "PSF file loaded successfully!" << std::endl;
    std::cout << "Surfaces: " << m_psf_collection.surface_pool.size() << std::endl;
    std::cout << "Anchors: " << m_psf_collection.anchor_pool.size() << std::endl;
    std::cout << "Bones: " << m_psf_collection.bone_pool.size() << std::endl;
    
    m_stats.total_surfaces = static_cast<uint32_t>(m_psf_collection.surface_pool.size());
    m_stats.total_anchors = static_cast<uint32_t>(m_psf_collection.anchor_pool.size());
    m_stats.is_loaded = true;
    
    ResetCamera();
    return ConvertPSFToPointCloud();
}

bool PSFOpenGLViewer::ConvertPSFToPointCloud() {
    std::cout << "Converting PSF to point cloud..." << std::endl;
    
    if (m_psf_collection.surface_pool.empty()) {
        std::cerr << "ERROR: No surfaces to convert" << std::endl;
        return false;
    }
    
    if (m_psf_collection.anchor_pool.empty()) {
        std::cerr << "ERROR: No anchors available" << std::endl;
        return false;
    }
    
    // Create camera for PSF renderer
    float camera_pos[3] = {m_camera_pos.x, m_camera_pos.y, m_camera_pos.z};
    float camera_target[3] = {m_camera_target.x, m_camera_target.y, m_camera_target.z};
    PSFRealtimeCamera psf_camera = PSFRealtimeRenderer::PSFRealtimeCreateCamera(
        camera_pos, camera_target, 60.0f, 
        static_cast<float>(m_config.window_width) / m_config.window_height
    );
    
    m_point_cloud.clear();
    
    try {
        if (!m_psf_renderer->PSFRealtimeRenderFrame(m_psf_collection, m_lights, psf_camera, m_point_cloud)) {
            std::cerr << "ERROR: Failed to generate point cloud" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION in point cloud generation: " << e.what() << std::endl;
        return false;
    }
    
    if (m_point_cloud.empty()) {
        std::cerr << "WARNING: Point cloud is empty" << std::endl;
        m_point_count = 0;
        return true;
    }
    
    // Convert to OpenGL format
    m_gl_positions.clear();
    m_gl_colors.clear();
    m_gl_normals.clear();
    
    size_t visible_count = 0;
    for (const auto& point : m_point_cloud) {
        if (!(point.flags & 0x1)) visible_count++;
    }
    
    m_gl_positions.reserve(visible_count * 3);
    m_gl_colors.reserve(visible_count * 3);
    m_gl_normals.reserve(visible_count * 3);
    
    for (const auto& point : m_point_cloud) {
        if (point.flags & 0x1) continue; // Skip culled points
        
        // Validate point data
        bool valid_position = std::isfinite(point.position[0]) && 
                             std::isfinite(point.position[1]) && 
                             std::isfinite(point.position[2]);
        
        bool valid_color = std::isfinite(point.color[0]) && 
                          std::isfinite(point.color[1]) && 
                          std::isfinite(point.color[2]);
        
        bool valid_normal = std::isfinite(point.normal[0]) && 
                           std::isfinite(point.normal[1]) && 
                           std::isfinite(point.normal[2]);
        
        if (!valid_position || !valid_color || !valid_normal) {
            continue;
        }
        
        // Add position
        m_gl_positions.push_back(point.position[0]);
        m_gl_positions.push_back(point.position[1]);
        m_gl_positions.push_back(point.position[2]);
        
        // Add color (clamped)
        m_gl_colors.push_back(std::max(0.0f, std::min(1.0f, point.color[0])));
        m_gl_colors.push_back(std::max(0.0f, std::min(1.0f, point.color[1])));
        m_gl_colors.push_back(std::max(0.0f, std::min(1.0f, point.color[2])));
        
        // Add normal
        m_gl_normals.push_back(point.normal[0]);
        m_gl_normals.push_back(point.normal[1]);
        m_gl_normals.push_back(point.normal[2]);
    }
    
    m_point_count = m_gl_positions.size() / 3;
    
    if (m_point_count > 0) {
        // Upload to GPU
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_positions);
        glBufferData(GL_ARRAY_BUFFER, m_gl_positions.size() * sizeof(float), 
                     m_gl_positions.data(), GL_DYNAMIC_DRAW);
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_colors);
        glBufferData(GL_ARRAY_BUFFER, m_gl_colors.size() * sizeof(float), 
                     m_gl_colors.data(), GL_DYNAMIC_DRAW);
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_normals);
        glBufferData(GL_ARRAY_BUFFER, m_gl_normals.size() * sizeof(float), 
                     m_gl_normals.data(), GL_DYNAMIC_DRAW);
        
        std::cout << "Generated " << m_point_count << " points for rendering" << std::endl;
    } else {
        std::cout << "No valid points generated" << std::endl;
    }
    
    return true;
}

void PSFOpenGLViewer::Run() {
    std::cout << "Starting main loop..." << std::endl;
    std::cout << "Controls: WASD - Move, Mouse - Look, Scroll - Zoom, R - Reset, ESC - Exit" << std::endl;
    
    while (!glfwWindowShouldClose(m_window)) {
        auto current_time = std::chrono::high_resolution_clock::now();
        float delta_time = std::chrono::duration<float>(current_time - m_last_frame_time).count();
        m_last_frame_time = current_time;
        
        HandleInput(delta_time);
        UpdateCamera(delta_time);
        UpdatePSFRenderer(delta_time);
        RenderFrame();
        UpdateStats(delta_time);
        
        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
}

void PSFOpenGLViewer::RenderFrame() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (m_point_count > 0) {
        RenderPoints();
    }
    
    if (m_config.show_stats) {
        RenderUI();
    }
}

void PSFOpenGLViewer::RenderPoints() {
    glUseProgram(m_shader_program);
    
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = glm::lookAt(m_camera_pos, m_camera_target, m_camera_up);
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f), 
        static_cast<float>(m_config.window_width) / m_config.window_height, 
        0.1f, 100.0f
    );
    glm::mat4 mvp = projection * view * model;
    
    glUniformMatrix4fv(m_uniform_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform1f(m_uniform_point_size, m_config.point_size);
    glUniform3f(m_uniform_camera_pos, m_camera_pos.x, m_camera_pos.y, m_camera_pos.z);
    
    glBindVertexArray(m_vao);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_point_count));
    glBindVertexArray(0);
    glUseProgram(0);
}

void PSFOpenGLViewer::RenderUI() {
    if (m_config.show_fps) {
        std::stringstream title;
        title << m_config.window_title 
              << " | FPS: " << static_cast<int>(m_stats.fps)
              << " | Points: " << m_stats.visible_points;
        glfwSetWindowTitle(m_window, title.str().c_str());
    }
}

void PSFOpenGLViewer::UpdateStats(float delta_time) {
    m_frame_timer += delta_time;
    m_frame_count++;
    
    if (m_frame_timer >= 1.0f) {
        m_stats.fps = m_frame_count / m_frame_timer;
        m_stats.frame_time_ms = (m_frame_timer / m_frame_count) * 1000.0f;
        m_frame_timer = 0.0f;
        m_frame_count = 0;
    }
    
    m_stats.visible_points = static_cast<uint32_t>(m_point_count);
}

void PSFOpenGLViewer::UpdateCamera(float delta_time) {
    // Update camera target based on yaw and pitch
    glm::vec3 direction;
    direction.x = cos(glm::radians(m_camera_yaw)) * cos(glm::radians(m_camera_pitch));
    direction.y = sin(glm::radians(m_camera_pitch));
    direction.z = sin(glm::radians(m_camera_yaw)) * cos(glm::radians(m_camera_pitch));
    
    glm::vec3 camera_front = glm::normalize(direction);
    m_camera_target = m_camera_pos + camera_front;
}

void PSFOpenGLViewer::UpdatePSFRenderer(float delta_time) {
    if (!m_stats.is_loaded) return;
    
    // Update PSF renderer camera
    float camera_pos[3] = {m_camera_pos.x, m_camera_pos.y, m_camera_pos.z};
    float camera_target[3] = {m_camera_target.x, m_camera_target.y, m_camera_target.z};
    PSFRealtimeCamera psf_camera = PSFRealtimeRenderer::PSFRealtimeCreateCamera(
        camera_pos, camera_target, 60.0f, 
        static_cast<float>(m_config.window_width) / m_config.window_height
    );
    
    // Regenerate point cloud if camera moved significantly
    static glm::vec3 last_camera_pos = m_camera_pos;
    float camera_movement = glm::length(m_camera_pos - last_camera_pos);
    
    if (camera_movement > 0.5f) {
        ConvertPSFToPointCloud();
        last_camera_pos = m_camera_pos;
    }
}

void PSFOpenGLViewer::HandleInput(float delta_time) {
    float velocity = m_config.camera_speed * delta_time;
    
    glm::vec3 camera_front = glm::normalize(m_camera_target - m_camera_pos);
    glm::vec3 camera_right = glm::normalize(glm::cross(camera_front, m_camera_up));
    
    if (m_keys[GLFW_KEY_W]) m_camera_pos += velocity * camera_front;
    if (m_keys[GLFW_KEY_S]) m_camera_pos -= velocity * camera_front;
    if (m_keys[GLFW_KEY_A]) m_camera_pos -= velocity * camera_right;
    if (m_keys[GLFW_KEY_D]) m_camera_pos += velocity * camera_right;
    if (m_keys[GLFW_KEY_Q]) m_camera_pos += velocity * m_camera_up;
    if (m_keys[GLFW_KEY_E]) m_camera_pos -= velocity * m_camera_up;
}

void PSFOpenGLViewer::ResetCamera() {
    // Calculate bounding box of PSF data
    if (!m_stats.is_loaded || m_psf_collection.anchor_pool.empty()) {
        m_camera_pos = glm::vec3(5.0f, 5.0f, 5.0f);
        m_camera_target = glm::vec3(0.0f, 0.0f, 0.0f);
        return;
    }
    
    glm::vec3 min_pos(FLT_MAX), max_pos(-FLT_MAX);
    
    for (const auto& anchor : m_psf_collection.anchor_pool) {
        Vec3 pos = AnchorPointEvaluator::DecodePosition(anchor.pos_xyz);
        min_pos = glm::min(min_pos, glm::vec3(pos.x, pos.y, pos.z));
        max_pos = glm::max(max_pos, glm::vec3(pos.x, pos.y, pos.z));
    }
    
    glm::vec3 center = (min_pos + max_pos) * 0.5f;
    float radius = glm::length(max_pos - min_pos) * 0.5f;
    
    m_camera_target = center;
    m_camera_pos = center + glm::vec3(radius * 1.5f, radius * 1.0f, radius * 1.5f);
    m_camera_distance = glm::length(m_camera_pos - m_camera_target);
    
    std::cout << "Camera reset - Center: (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;
    std::cout << "Camera distance: " << m_camera_distance << std::endl;
}

void PSFOpenGLViewer::Shutdown() {
    CleanupOpenGL();
    
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    
    glfwTerminate();
    std::cout << "PSF Viewer shut down successfully" << std::endl;
}

void PSFOpenGLViewer::CleanupOpenGL() {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    
    if (m_vbo_positions) {
        glDeleteBuffers(1, &m_vbo_positions);
        m_vbo_positions = 0;
    }
    
    if (m_vbo_colors) {
        glDeleteBuffers(1, &m_vbo_colors);
        m_vbo_colors = 0;
    }
    
    if (m_vbo_normals) {
        glDeleteBuffers(1, &m_vbo_normals);
        m_vbo_normals = 0;
    }
    
    if (m_shader_program) {
        glDeleteProgram(m_shader_program);
        m_shader_program = 0;
    }
}

// ================================
// CALLBACK FUNCTIONS
// ================================

void PSFOpenGLViewer::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    PSFOpenGLViewer* viewer = static_cast<PSFOpenGLViewer*>(glfwGetWindowUserPointer(window));
    
    if (action == GLFW_PRESS) {
        viewer->m_keys[key] = true;
        
        switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            case GLFW_KEY_R:
                viewer->ResetCamera();
                break;
            case GLFW_KEY_F:
                viewer->m_config.show_fps = !viewer->m_config.show_fps;
                break;
            case GLFW_KEY_TAB:
                viewer->m_config.show_stats = !viewer->m_config.show_stats;
                break;
            case GLFW_KEY_EQUAL: // Plus key
                viewer->m_config.point_size = std::min(10.0f, viewer->m_config.point_size + 0.5f);
                break;
            case GLFW_KEY_MINUS:
                viewer->m_config.point_size = std::max(0.5f, viewer->m_config.point_size - 0.5f);
                break;
        }
    } else if (action == GLFW_RELEASE) {
        viewer->m_keys[key] = false;
    }
}

void PSFOpenGLViewer::MouseCallback(GLFWwindow* window, double xpos, double ypos) {
    PSFOpenGLViewer* viewer = static_cast<PSFOpenGLViewer*>(glfwGetWindowUserPointer(window));
    
    if (!viewer->m_config.enable_mouse_controls) return;
    
    if (viewer->m_first_mouse) {
        viewer->m_last_mouse_x = xpos;
        viewer->m_last_mouse_y = ypos;
        viewer->m_first_mouse = false;
    }
    
    double xoffset = xpos - viewer->m_last_mouse_x;
    double yoffset = viewer->m_last_mouse_y - ypos; // Reversed since y-coordinates go from bottom to top
    viewer->m_last_mouse_x = xpos;
    viewer->m_last_mouse_y = ypos;
    
    xoffset *= viewer->m_config.mouse_sensitivity;
    yoffset *= viewer->m_config.mouse_sensitivity;
    
    viewer->m_camera_yaw += static_cast<float>(xoffset);
    viewer->m_camera_pitch += static_cast<float>(yoffset);
    
    // Constrain pitch
    viewer->m_camera_pitch = std::max(-89.0f, std::min(89.0f, viewer->m_camera_pitch));
}

void PSFOpenGLViewer::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    PSFOpenGLViewer* viewer = static_cast<PSFOpenGLViewer*>(glfwGetWindowUserPointer(window));
    
    float zoom_speed = 0.1f;
    glm::vec3 camera_front = glm::normalize(viewer->m_camera_target - viewer->m_camera_pos);
    viewer->m_camera_pos += camera_front * static_cast<float>(yoffset) * zoom_speed * viewer->m_camera_distance;
    viewer->m_camera_distance = glm::length(viewer->m_camera_pos - viewer->m_camera_target);
}

void PSFOpenGLViewer::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    PSFOpenGLViewer* viewer = static_cast<PSFOpenGLViewer*>(glfwGetWindowUserPointer(window));
    viewer->m_config.window_width = width;
    viewer->m_config.window_height = height;
    glViewport(0, 0, width, height);
}

// ================================
// PSFViewerUtils Implementation
// ================================

GLuint PSFViewerUtils::CompileShader(const std::string& source, GLenum shader_type) {
    GLuint shader = glCreateShader(shader_type);
    const char* source_cstr = source.c_str();
    glShaderSource(shader, 1, &source_cstr, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, info_log);
        std::cerr << "Shader compilation error: " << info_log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint PSFViewerUtils::CreateShaderProgram(const std::string& vertex_source, const std::string& fragment_source) {
    GLuint vertex_shader = CompileShader(vertex_source, GL_VERTEX_SHADER);
    if (vertex_shader == 0) return 0;
    
    GLuint fragment_shader = CompileShader(fragment_source, GL_FRAGMENT_SHADER);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[1024];
        glGetProgramInfoLog(program, 1024, nullptr, info_log);
        std::cerr << "Shader program linking error: " << info_log << std::endl;
        glDeleteProgram(program);
        program = 0;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    return program;
}

bool PSFViewerUtils::CheckGLError(const std::string& operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error in " << operation << ": " << error << std::endl;
        return false;
    }
    return true;
}

void PSFViewerUtils::PrintOpenGLInfo() {
    std::cout << "OpenGL Info:" << std::endl;
    std::cout << "  Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "  Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "  Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "  GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

bool PSFViewerUtils::ValidatePSFFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    return magic == PSF_MAGIC;
}

std::string PSFViewerUtils::GetPSFFileInfo(const std::string& filename) {
    ProceduralObjectCollection collection;
    if (!collection.LoadFromFile(filename)) {
        return "Failed to load PSF file";
    }
    
    std::stringstream info;
    info << "PSF File: " << filename << "\n";
    info << "Objects: " << collection.object_descriptors.size() << "\n";
    info << "Surfaces: " << collection.surface_pool.size() << "\n";
    info << "Anchors: " << collection.anchor_pool.size() << "\n";
    info << "Bones: " << collection.bone_pool.size() << "\n";
    info << "Name: " << collection.object_name << "\n";
    info << "Creator: " << collection.creator_info;
    
    return info.str();
}

// ================================
// PSFViewerIntegration Implementation
// ================================

bool PSFViewerIntegration::ViewPSFFile(const std::string& psf_filename, const PSFOpenGLViewer::ViewerConfig& config) {
    std::cout << "PSFViewerIntegration::ViewPSFFile called with: " << psf_filename << std::endl;
    
    PSFOpenGLViewer viewer(config);
    
    if (!viewer.Initialize()) {
        std::cerr << "Failed to initialize PSF Viewer" << std::endl;
        return false;
    }
    
    if (!viewer.LoadPSFFile(psf_filename)) {
        std::cerr << "Failed to load PSF file: " << psf_filename << std::endl;
        return false;
    }
    
    viewer.Run();
    return true;
}

bool PSFViewerIntegration::ComparePSFFiles(const std::vector<std::string>& psf_filenames) {
    // Simple implementation - view files one by one
    for (const auto& filename : psf_filenames) {
        std::cout << "\n=== Viewing: " << filename << " ===" << std::endl;
        std::cout << PSFViewerUtils::GetPSFFileInfo(filename) << std::endl;
        std::cout << "Press any key to continue to next file..." << std::endl;
        std::cin.get();
        
        PSFOpenGLViewer::ViewerConfig config;
        if (!ViewPSFFile(filename, config)) {
            return false;
        }
    }
    return true;
}

bool PSFViewerIntegration::ExportFrame(const std::string& output_filename, int width, int height) {
    // Placeholder for screenshot functionality
    // Would require additional libraries like SOIL2 or stb_image_write
    std::cout << "Screenshot export not implemented yet" << std::endl;
    return false;
}