// PSF Real-time Renderer System
// Naming Convention: PSFRealtime[Component][Action]
#pragma once

#include "object_descriptor_buffers.h"
#include "mathematical_color_evaluator.h"
#include "surface_buffer_evaluator.h"
#include "anchorpoint_evaluator.h"
#include <vector>
#include <memory>
#include <chrono>

// ================================
// CORE DATA STRUCTURES
// ================================

struct PSFRealtimePoint {
    float position[3];         // World position
    float normal[3];          // Surface normal
    float color[3];           // Final RGB color
    float albedo[3];          // Material base color
    float alpha;              // Transparency (0=transparent, 1=opaque)
    uint32_t surface_id;      // Source surface index
    uint32_t flags;           // Culling and state flags
};

struct PSFRealtimeLight {
    float position[3];        // Light position or direction
    float color[3];           // RGB color
    float intensity;          // Light intensity multiplier
    float radius;             // Attenuation radius
    bool is_directional;      // true=directional, false=point light
    bool cast_shadows;        // Enable shadow casting
};

struct PSFRealtimeCamera {
    float position[3];        // Camera world position
    float target[3];          // Look-at target
    float up[3];              // Up vector
    float fov_degrees;        // Field of view
    float near_plane;         // Near clipping plane
    float far_plane;          // Far clipping plane
    float aspect_ratio;       // Width/height ratio
};

// ================================
// CONFIGURATION STRUCTURES
// ================================

struct PSFRealtimeConfig {
    // Point Generation
    int max_points_per_surface = 1000;
    float min_point_spacing = 0.05f;
    float max_point_spacing = 2.0f;
    float lod_distance_factor = 50.0f;
    
    // Lighting
    bool enable_global_illumination = true;
    int gi_sample_count = 12;
    float gi_sample_distance = 0.2f;
    float gi_intensity = 0.4f;
    float ambient_intensity = 0.1f;
    
    // Culling
    bool enable_frustum_culling = true;
    bool enable_backface_culling = true;
    float culling_epsilon = 0.001f;
    
    // Transparency
    bool enable_transparency = true;
    float transparency_threshold = 0.95f;
    
    // Performance
    bool enable_multithreading = true;
    int worker_thread_count = 4;
    
    // Debug
    bool debug_show_normals = false;
    bool debug_show_wireframe = false;
    bool debug_timing = false;
};

struct PSFRealtimeStats {
    uint32_t total_surfaces = 0;
    uint32_t total_points_generated = 0;
    uint32_t visible_points = 0;
    uint32_t culled_points = 0;
    uint32_t transparent_points = 0;
    
    float generation_time_ms = 0.0f;
    float lighting_time_ms = 0.0f;
    float culling_time_ms = 0.0f;
    float total_frame_time_ms = 0.0f;
    
    void Reset() {
        total_surfaces = total_points_generated = visible_points = 0;
        culled_points = transparent_points = 0;
        generation_time_ms = lighting_time_ms = culling_time_ms = total_frame_time_ms = 0.0f;
    }
};

// ================================
// POINT GENERATION ENGINE
// ================================

class PSFRealtimePointGenerator {
public:
    static std::vector<PSFRealtimePoint> PSFRealtimeGeneratePoints(
        const ProceduralObjectCollection& psf_collection,
        const PSFRealtimeCamera& camera,
        const PSFRealtimeConfig& config,
        PSFRealtimeStats& stats
    );
    
    static std::vector<PSFRealtimePoint> PSFRealtimeGenerateFromSurface(
        const ProceduralSurfaceBuffer& surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        uint32_t surface_index,
        const PSFRealtimeCamera& camera,
        const PSFRealtimeConfig& config
    );
    
private:
    static float PSFRealtimeCalculateLODSpacing(
        const float surface_center[3],
        const PSFRealtimeCamera& camera,
        const PSFRealtimeConfig& config
    );
    
    static void PSFRealtimeGeneratePlanePoints(
        const ProceduralSurfaceBuffer& surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        uint32_t surface_index,
        float point_spacing,
        std::vector<PSFRealtimePoint>& output_points
    );
    
    static void PSFRealtimeGenerateProceduralPoints(
        const ProceduralSurfaceBuffer& surface,
        const std::vector<AnchorPointBuffer>& anchor_pool,
        uint32_t surface_index,
        float point_spacing,
        std::vector<PSFRealtimePoint>& output_points
    );
    
    static void PSFRealtimeGenerateMetaballPoints(
        const ProceduralSurfaceBuffer& surface,
        uint32_t surface_index,
        float point_spacing,
        std::vector<PSFRealtimePoint>& output_points
    );
};

// ================================
// VECTOR FIELD LIGHTING ENGINE
// ================================

class PSFRealtimeLightingEngine {
public:
    static void PSFRealtimeComputeLighting(
        std::vector<PSFRealtimePoint>& points,
        const std::vector<PSFRealtimeLight>& lights,
        const ProceduralObjectCollection& scene,
        const PSFRealtimeCamera& camera,
        const PSFRealtimeConfig& config,
        PSFRealtimeStats& stats
    );
    
    static void PSFRealtimeComputeDirectLighting(
        PSFRealtimePoint& point,
        const std::vector<PSFRealtimeLight>& lights,
        const ProceduralObjectCollection& scene,
        const PSFRealtimeConfig& config
    );
    
    static void PSFRealtimeComputeGlobalIllumination(
        PSFRealtimePoint& point,
        const std::vector<PSFRealtimeLight>& lights,
        const ProceduralObjectCollection& scene,
        const PSFRealtimeConfig& config
    );
    
    static void PSFRealtimeApplyTransparency(
        PSFRealtimePoint& point,
        const float direct_light[3],
        const PSFRealtimeConfig& config
    );
    
private:
    static void PSFRealtimeCalculateVectorField(
        const PSFRealtimeLight& light,
        const float point_pos[3],
        float light_vector[3],
        float& light_intensity
    );
    
    static bool PSFRealtimeTestOcclusion(
        const float from_pos[3],
        const float to_pos[3],
        const ProceduralObjectCollection& scene,
        float threshold = 0.01f
    );
    
    static std::vector<float[3]> PSFRealtimeGenerateHemisphereSamples(
        const float normal[3],
        int sample_count
    );
};

// ================================
// CULLING ENGINE
// ================================

class PSFRealtimeCullingEngine {
public:
    static void PSFRealtimePerformCulling(
        std::vector<PSFRealtimePoint>& points,
        const PSFRealtimeCamera& camera,
        const PSFRealtimeConfig& config,
        PSFRealtimeStats& stats
    );
    
private:
    static bool PSFRealtimeTestFrustumCulling(
        const PSFRealtimePoint& point,
        const PSFRealtimeCamera& camera
    );
    
    static bool PSFRealtimeTestBackfaceCulling(
        const PSFRealtimePoint& point,
        const PSFRealtimeCamera& camera
    );
    
    static void PSFRealtimeCalculateViewMatrix(
        const PSFRealtimeCamera& camera,
        float view_matrix[16]
    );
    
    static void PSFRealtimeCalculateProjectionMatrix(
        const PSFRealtimeCamera& camera,
        float proj_matrix[16]
    );
};

// ================================
// MAIN RENDERER CLASS
// ================================

class PSFRealtimeRenderer {
public:
    PSFRealtimeRenderer(const PSFRealtimeConfig& config = PSFRealtimeConfig{});
    ~PSFRealtimeRenderer() = default;
    
    // Main rendering function
    bool PSFRealtimeRenderFrame(
        const ProceduralObjectCollection& psf_collection,
        const std::vector<PSFRealtimeLight>& lights,
        const PSFRealtimeCamera& camera,
        std::vector<PSFRealtimePoint>& output_points,
        float delta_time = 0.016f  // 60 FPS default
    );
    
    // Configuration management
    void PSFRealtimeUpdateConfig(const PSFRealtimeConfig& new_config);
    const PSFRealtimeConfig& PSFRealtimeGetConfig() const { return m_config; }
    
    // Statistics
    const PSFRealtimeStats& PSFRealtimeGetLastFrameStats() const { return m_stats; }
    void PSFRealtimeResetStats() { m_stats.Reset(); }
    
    // Camera utilities
    static PSFRealtimeCamera PSFRealtimeCreateCamera(
        const float position[3],
        const float target[3],
        float fov_degrees = 60.0f,
        float aspect_ratio = 16.0f/9.0f,
        float near_plane = 0.1f,
        float far_plane = 1000.0f
    );
    
    // Light utilities
    static PSFRealtimeLight PSFRealtimeCreatePointLight(
        const float position[3],
        const float color[3],
        float intensity = 1.0f,
        float radius = 10.0f
    );
    
    static PSFRealtimeLight PSFRealtimeCreateDirectionalLight(
        const float direction[3],
        const float color[3],
        float intensity = 1.0f
    );
    
private:
    PSFRealtimeConfig m_config;
    PSFRealtimeStats m_stats;
    
    // Cache for performance
    std::vector<PSFRealtimePoint> m_point_cache;
    float m_last_camera_pos[3] = {0, 0, 0};
    float m_cache_validity_threshold = 2.0f;
    
    bool PSFRealtimeShouldRegeneratePoints(const PSFRealtimeCamera& camera);
    void PSFRealtimeUpdateCameraCache(const PSFRealtimeCamera& camera);
};

// ================================
// INTEGRATION HELPERS FOR MAIN.CPP
// ================================

class PSFRealtimeIntegration {
public:
    // Easy setup for main.cpp
    static std::unique_ptr<PSFRealtimeRenderer> PSFRealtimeCreateDefaultRenderer();
    
    // Convert your existing PSF to renderable format
    static bool PSFRealtimeConvertAndRender(
        const std::string& psf_filename,
        const std::string& output_format = "preview"  // "preview", "obj", "ply"
    );
    
    // Debug output
    static void PSFRealtimePrintStats(const PSFRealtimeStats& stats);
    
    // Export points to standard formats
    static bool PSFRealtimeExportPoints(
        const std::vector<PSFRealtimePoint>& points,
        const std::string& filename,
        const std::string& format = "obj"
    );
};

// NO IMPLEMENTATIONS IN HEADER FILE - ALL SHOULD BE IN .CPP