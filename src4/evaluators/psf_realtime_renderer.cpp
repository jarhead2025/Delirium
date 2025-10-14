#include "psf_realtime_renderer.h"
#include "surface_buffers.h"
#include "anchorpoint_evaluator.h"
#include "mathematical_color_evaluator.h"
#include <cmath>
#include <algorithm>
#include <chrono>

// ================================
// PSFRealtimeRenderer Implementation
// ================================


void DebugColorPipeline(const ProceduralObjectCollection& collection, 
                       const std::vector<PSFRealtimePoint>& points) {
    std::cout << "\n=== COLOR PIPELINE DEBUG ===\n";
    
    // Debug first few surfaces
    for (size_t surf_idx = 0; surf_idx < std::min<size_t>(3, collection.surface_pool.size()); ++surf_idx) {
        const auto& surface = collection.surface_pool[surf_idx];
        
        std::cout << "\nSURFACE " << surf_idx << ":\n";
        std::cout << "  Equations: R=" << static_cast<int>(surface.color_eqn_r)
                  << " G=" << static_cast<int>(surface.color_eqn_g) 
                  << " B=" << static_cast<int>(surface.color_eqn_b) << "\n";
        std::cout << "  Params: R=" << static_cast<int>(surface.color_param_r)
                  << " G=" << static_cast<int>(surface.color_param_g) 
                  << " B=" << static_cast<int>(surface.color_param_b) << "\n";
        
        // Test mathematical evaluation
        float test_color[3];
        MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, 0.5f, 0.5f, test_color);
        std::cout << "  Math Eval (0.5,0.5): (" << test_color[0] << ", " << test_color[1] << ", " << test_color[2] << ")\n";
        
        // Find points for this surface
        int point_count = 0;
        Vec3 avg_albedo(0,0,0), avg_color(0,0,0);
        
        for (const auto& point : points) {
            if (point.surface_id == surf_idx && !(point.flags & 0x1)) {
                avg_albedo.x += point.albedo[0];
                avg_albedo.y += point.albedo[1]; 
                avg_albedo.z += point.albedo[2];
                
                avg_color.x += point.color[0];
                avg_color.y += point.color[1];
                avg_color.z += point.color[2];
                
                if (point_count < 3) {
                    std::cout << "    Point " << point_count << " albedo: (" << point.albedo[0] 
                              << ", " << point.albedo[1] << ", " << point.albedo[2] << ")\n";
                    std::cout << "    Point " << point_count << " final: (" << point.color[0] 
                              << ", " << point.color[1] << ", " << point.color[2] << ")\n";
                }
                point_count++;
            }
        }
        
        if (point_count > 0) {
            avg_albedo.x /= point_count; avg_albedo.y /= point_count; avg_albedo.z /= point_count;
            avg_color.x /= point_count; avg_color.y /= point_count; avg_color.z /= point_count;
            
            std::cout << "  " << point_count << " points generated\n";
            std::cout << "  Avg albedo: (" << avg_albedo.x << ", " << avg_albedo.y << ", " << avg_albedo.z << ")\n";
            std::cout << "  Avg final: (" << avg_color.x << ", " << avg_color.y << ", " << avg_color.z << ")\n";
            
            // Check if colors match mathematical evaluation
            float diff = abs(avg_albedo.x - test_color[0]) + abs(avg_albedo.y - test_color[1]) + abs(avg_albedo.z - test_color[2]);
            if (diff > 0.1f) {
                std::cout << "  ⚠️ WARNING: Point colors don't match math eval! Diff: " << diff << "\n";
            } else {
                std::cout << "  ✅ Point colors match math evaluation\n";
            }
        }
    }
    std::cout << "========================\n";
}










PSFRealtimeRenderer::PSFRealtimeRenderer(const PSFRealtimeConfig& config) 
    : m_config(config) {
    m_stats.Reset();
    m_last_camera_pos[0] = m_last_camera_pos[1] = m_last_camera_pos[2] = 0.0f;
    m_cache_validity_threshold = 2.0f;
}

bool PSFRealtimeRenderer::PSFRealtimeRenderFrame(
    const ProceduralObjectCollection& psf_collection,
    const std::vector<PSFRealtimeLight>& lights,
    const PSFRealtimeCamera& camera,
    std::vector<PSFRealtimePoint>& output_points,
    float delta_time
) {
    auto frame_start = std::chrono::high_resolution_clock::now();
    m_stats.Reset();
    
    // CRITICAL: Validate input collection
    if (psf_collection.surface_pool.empty()) {
        std::cerr << "WARNING: Empty surface pool in PSF collection" << std::endl;
        return true; // Not an error, just empty
    }
    
    if (psf_collection.anchor_pool.empty()) {
        std::cerr << "ERROR: Empty anchor pool in PSF collection" << std::endl;
        return false; // This is an error - surfaces need anchors
    }
    
    m_stats.total_surfaces = static_cast<uint32_t>(psf_collection.surface_pool.size());
    
    // Step 1: Point Generation with validation
    auto gen_start = std::chrono::high_resolution_clock::now();
    
    bool should_regenerate = PSFRealtimeShouldRegeneratePoints(camera);
    if (should_regenerate || m_point_cache.empty()) {
        try {
            m_point_cache = PSFRealtimePointGenerator::PSFRealtimeGeneratePoints(
                psf_collection, camera, m_config, m_stats
            );
            PSFRealtimeUpdateCameraCache(camera);
        } catch (const std::exception& e) {
            std::cerr << "ERROR in point generation: " << e.what() << std::endl;
            return false;
        }
    }
    
    // CRITICAL: Validate point cache before copying
    if (m_point_cache.empty()) {
        std::cerr << "WARNING: Point cache is empty after generation" << std::endl;
        output_points.clear();
        return true; // Not an error, just no points
    }
    
    output_points = m_point_cache;
    
    auto gen_end = std::chrono::high_resolution_clock::now();
    m_stats.generation_time_ms = std::chrono::duration<float, std::milli>(gen_end - gen_start).count();
    
    // Step 2: Lighting Computation
    auto lighting_start = std::chrono::high_resolution_clock::now();
    
    try {
        PSFRealtimeLightingEngine::PSFRealtimeComputeLighting(
            output_points, lights, psf_collection, camera, m_config, m_stats
        );
    } catch (const std::exception& e) {
        std::cerr << "ERROR in lighting computation: " << e.what() << std::endl;
        return false;
    }
    
    auto lighting_end = std::chrono::high_resolution_clock::now();
    m_stats.lighting_time_ms = std::chrono::duration<float, std::milli>(lighting_end - lighting_start).count();
    
    // Step 3: Culling
    auto culling_start = std::chrono::high_resolution_clock::now();
    
    try {
        PSFRealtimeCullingEngine::PSFRealtimePerformCulling(
            output_points, camera, m_config, m_stats
        );
    } catch (const std::exception& e) {
        std::cerr << "ERROR in culling: " << e.what() << std::endl;
        return false;
    }
    
    auto culling_end = std::chrono::high_resolution_clock::now();
    m_stats.culling_time_ms = std::chrono::duration<float, std::milli>(culling_end - culling_start).count();
    
    // Calculate final stats
    m_stats.visible_points = 0;
    m_stats.transparent_points = 0;
    for (const auto& point : output_points) {
        if (!(point.flags & 0x1)) {
            m_stats.visible_points++;
            if (point.alpha < m_config.transparency_threshold) {
                m_stats.transparent_points++;
            }
        }
    }
    m_stats.culled_points = m_stats.total_points_generated - m_stats.visible_points;
    
    auto frame_end = std::chrono::high_resolution_clock::now();
    m_stats.total_frame_time_ms = std::chrono::duration<float, std::milli>(frame_end - frame_start).count();
     DebugColorPipeline(psf_collection, output_points);
    return true;
}


void PSFRealtimeRenderer::PSFRealtimeUpdateConfig(const PSFRealtimeConfig& new_config) {
    m_config = new_config;
}

bool PSFRealtimeRenderer::PSFRealtimeShouldRegeneratePoints(const PSFRealtimeCamera& camera) {
    float dx = camera.position[0] - m_last_camera_pos[0];
    float dy = camera.position[1] - m_last_camera_pos[1];
    float dz = camera.position[2] - m_last_camera_pos[2];
    float distance = sqrt(dx*dx + dy*dy + dz*dz);
    
    return distance > m_cache_validity_threshold;
}

void PSFRealtimeRenderer::PSFRealtimeUpdateCameraCache(const PSFRealtimeCamera& camera) {
    m_last_camera_pos[0] = camera.position[0];
    m_last_camera_pos[1] = camera.position[1];
    m_last_camera_pos[2] = camera.position[2];
}

// Camera utility functions
PSFRealtimeCamera PSFRealtimeRenderer::PSFRealtimeCreateCamera(
    const float position[3],
    const float target[3],
    float fov_degrees,
    float aspect_ratio,
    float near_plane,
    float far_plane
) {
    PSFRealtimeCamera camera;
    
    camera.position[0] = position[0];
    camera.position[1] = position[1];
    camera.position[2] = position[2];
    
    camera.target[0] = target[0];
    camera.target[1] = target[1];
    camera.target[2] = target[2];
    
    camera.up[0] = 0; camera.up[1] = 1; camera.up[2] = 0;
    
    camera.fov_degrees = fov_degrees;
    camera.aspect_ratio = aspect_ratio;
    camera.near_plane = near_plane;
    camera.far_plane = far_plane;
    
    return camera;
}

// Light utility functions
PSFRealtimeLight PSFRealtimeRenderer::PSFRealtimeCreatePointLight(
    const float position[3],
    const float color[3],
    float intensity,
    float radius
) {
    PSFRealtimeLight light;
    
    light.position[0] = position[0];
    light.position[1] = position[1];
    light.position[2] = position[2];
    
    light.color[0] = color[0];
    light.color[1] = color[1];
    light.color[2] = color[2];
    
    light.intensity = intensity;
    light.radius = radius;
    light.is_directional = false;
    light.cast_shadows = true;
    
    return light;
}

PSFRealtimeLight PSFRealtimeRenderer::PSFRealtimeCreateDirectionalLight(
    const float direction[3],
    const float color[3],
    float intensity
) {
    PSFRealtimeLight light;
    
    float length = sqrt(direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2]);
    if (length > 1e-6f) {
        light.position[0] = direction[0] / length;
        light.position[1] = direction[1] / length;
        light.position[2] = direction[2] / length;
    } else {
        light.position[0] = 0; light.position[1] = -1; light.position[2] = 0;
    }
    
    light.color[0] = color[0];
    light.color[1] = color[1];
    light.color[2] = color[2];
    
    light.intensity = intensity;
    light.radius = 0.0f;
    light.is_directional = true;
    light.cast_shadows = true;
    
    return light;
}

// ================================
// PSFRealtimePointGenerator Implementation
// ================================

std::vector<PSFRealtimePoint> PSFRealtimePointGenerator::PSFRealtimeGeneratePoints(
    const ProceduralObjectCollection& psf_collection,
    const PSFRealtimeCamera& camera,
    const PSFRealtimeConfig& config,
    PSFRealtimeStats& stats
) {
    std::vector<PSFRealtimePoint> all_points;
    
    // CRITICAL: Validate inputs
    if (psf_collection.surface_pool.empty()) {
        std::cerr << "WARNING: Empty surface pool in point generation" << std::endl;
        return all_points;
    }
    
    if (psf_collection.anchor_pool.empty()) {
        std::cerr << "ERROR: Empty anchor pool in point generation" << std::endl;
        return all_points;
    }
    
    // CRITICAL: Pre-reserve memory to prevent reallocations
    size_t estimated_points = psf_collection.surface_pool.size() * config.max_points_per_surface;
    all_points.reserve(estimated_points);
    
    for (size_t i = 0; i < psf_collection.surface_pool.size(); ++i) {
        try {
            auto surface_points = PSFRealtimeGenerateFromSurface(
                psf_collection.surface_pool[i],
                psf_collection.anchor_pool,
                static_cast<uint32_t>(i),
                camera,
                config
            );
            
            // CRITICAL: Check for valid points before adding
            if (!surface_points.empty()) {
                all_points.insert(all_points.end(), surface_points.begin(), surface_points.end());
            }
        } catch (const std::exception& e) {
            std::cerr << "ERROR generating points for surface " << i << ": " << e.what() << std::endl;
            continue; // Skip this surface but continue with others
        }
    }
    
    stats.total_points_generated = static_cast<uint32_t>(all_points.size());
    return all_points;
}

std::vector<PSFRealtimePoint> PSFRealtimePointGenerator::PSFRealtimeGenerateFromSurface(
    const ProceduralSurfaceBuffer& surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    uint32_t surface_index,
    const PSFRealtimeCamera& camera,
    const PSFRealtimeConfig& config
) {
    std::vector<PSFRealtimePoint> points;
    
    // Calculate surface center for LOD
    float surface_center[3] = {0, 0, 0};
    int valid_anchors = 0;
    for (int i = 0; i < 4; ++i) {
        if (surface.anchor_point_indices[i] < anchor_pool.size()) {
            Vec3 anchor_pos = AnchorPointEvaluator::DecodePosition(
                anchor_pool[surface.anchor_point_indices[i]].pos_xyz
            );
            surface_center[0] += anchor_pos.x;
            surface_center[1] += anchor_pos.y;
            surface_center[2] += anchor_pos.z;
            valid_anchors++;
        }
    }
    
    if (valid_anchors > 0) {
        surface_center[0] /= valid_anchors;
        surface_center[1] /= valid_anchors;
        surface_center[2] /= valid_anchors;
    }
    
    float point_spacing = PSFRealtimeCalculateLODSpacing(surface_center, camera, config);
    
    // Generate points based on surface type
    SurfaceEquationType surface_type = SurfaceBufferUtils::ExtractBaseType(&surface);
    
    switch (surface_type) {
        case EQUATION_PLANE:
            PSFRealtimeGeneratePlanePoints(surface, anchor_pool, surface_index, point_spacing, points);
            break;
        case EQUATION_METABALL:
            PSFRealtimeGenerateMetaballPoints(surface, surface_index, point_spacing, points);
            break;
        default:
            PSFRealtimeGenerateProceduralPoints(surface, anchor_pool, surface_index, point_spacing, points);
            break;
    }
    
    // Limit points per surface
    if (points.size() > static_cast<size_t>(config.max_points_per_surface)) {
        points.resize(config.max_points_per_surface);
    }
    
    return points;
}

float PSFRealtimePointGenerator::PSFRealtimeCalculateLODSpacing(
    const float surface_center[3],
    const PSFRealtimeCamera& camera,
    const PSFRealtimeConfig& config
) {
    float dx = surface_center[0] - camera.position[0];
    float dy = surface_center[1] - camera.position[1];
    float dz = surface_center[2] - camera.position[2];
    float distance = sqrt(dx*dx + dy*dy + dz*dz);
    
    // LOD factor: closer = smaller spacing (more points), farther = larger spacing (fewer points)
    float lod_factor = std::max(0.1f, std::min(1.0f, distance / config.lod_distance_factor));
    
    return config.min_point_spacing + (config.max_point_spacing - config.min_point_spacing) * lod_factor;
}



void PSFRealtimePointGenerator::PSFRealtimeGenerateProceduralPoints(
    const ProceduralSurfaceBuffer& surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    uint32_t surface_index,
    float point_spacing,
    std::vector<PSFRealtimePoint>& output_points
) {
    // Simple procedural point generation
    int resolution = static_cast<int>(2.0f / point_spacing);
    resolution = std::max(4, std::min(resolution, 32));
    
    for (int i = 0; i < resolution; ++i) {
        for (int j = 0; j < resolution; ++j) {
            float u = static_cast<float>(i) / (resolution - 1);
            float v = static_cast<float>(j) / (resolution - 1);
            
            PSFRealtimePoint point;
            
            point.position[0] = (u - 0.5f) * 2.0f;
            point.position[1] = sinf(u * 3.14159f) * 0.2f;
            point.position[2] = (v - 0.5f) * 2.0f;
            
            point.normal[0] = 0.0f;
            point.normal[1] = 1.0f;
            point.normal[2] = 0.0f;
            
float finalColor[3] = {0.5f, 0.5f, 0.5f}; // Default fallback

// Ensure UV coordinates are valid
float safe_u = std::max(0.0f, std::min(1.0f, u));
float safe_v = std::max(0.0f, std::min(1.0f, v));

// Call mathematical evaluator with error checking
try {
    MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, safe_u, safe_v, finalColor);
    
    // Validate the result
    if (!std::isfinite(finalColor[0]) || !std::isfinite(finalColor[1]) || !std::isfinite(finalColor[2])) {
        std::cerr << "WARNING: Invalid color from math evaluator for surface " << surface_index 
                  << " UV(" << safe_u << "," << safe_v << ")\n";
        finalColor[0] = finalColor[1] = finalColor[2] = 0.5f;
    }
    
    // Clamp to valid range
    finalColor[0] = std::max(0.0f, std::min(1.0f, finalColor[0]));
    finalColor[1] = std::max(0.0f, std::min(1.0f, finalColor[1]));
    finalColor[2] = std::max(0.0f, std::min(1.0f, finalColor[2]));
    
} catch (const std::exception& e) {
    std::cerr << "ERROR in MathematicalColorEvaluator: " << e.what() << "\n";
    finalColor[0] = finalColor[1] = finalColor[2] = 0.5f;
}

// Debug output for first few points
if (surface_index < 3 && i < 3 && j < 3) {
    std::cout << "Surface " << surface_index << " UV(" << safe_u << "," << safe_v 
              << ") -> Color(" << finalColor[0] << "," << finalColor[1] << "," << finalColor[2] << ")\n";
}

point.albedo[0] = finalColor[0];
point.albedo[1] = finalColor[1];
point.albedo[2] = finalColor[2];

point.alpha = 1.0f;
point.surface_id = surface_index;
point.flags = 0;

// Initially set color equal to albedo (lighting will modify this later)
point.color[0] = point.albedo[0];
point.color[1] = point.albedo[1];
point.color[2] = point.albedo[2];
            
            output_points.push_back(point);
        }
    }
}

void PSFRealtimePointGenerator::PSFRealtimeGeneratePlanePoints(
    const ProceduralSurfaceBuffer& surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    uint32_t surface_index,
    float point_spacing,
    std::vector<PSFRealtimePoint>& output_points
) {
    // CRITICAL: Validate ALL anchor indices FIRST
    for (int i = 0; i < 4; ++i) {
        if (surface.anchor_point_indices[i] >= anchor_pool.size()) {
            std::cerr << "ERROR: Invalid anchor index " << surface.anchor_point_indices[i] 
                      << " >= pool size " << anchor_pool.size() << " for surface " << surface_index << std::endl;
            return; // Skip this surface entirely
        }
    }
    
    // CRITICAL: Check for empty anchor pool
    if (anchor_pool.empty()) {
        std::cerr << "ERROR: Empty anchor pool for surface " << surface_index << std::endl;
        return;
    }
    
    // Now safely get anchor positions
    Vec3 anchor_positions[4];
    for (int i = 0; i < 4; ++i) {
        anchor_positions[i] = AnchorPointEvaluator::DecodePosition(
            anchor_pool[surface.anchor_point_indices[i]].pos_xyz
        );
    }
    
    // Calculate resolution with safety bounds
    float edge1_length = CalculateDistance(anchor_positions[0], anchor_positions[1]);
    float edge2_length = CalculateDistance(anchor_positions[0], anchor_positions[3]);
    
    int resolution_u = static_cast<int>(edge1_length / point_spacing) + 1;
    int resolution_v = static_cast<int>(edge2_length / point_spacing) + 1;
    
    // CRITICAL: Enforce strict resolution limits to prevent memory issues
    resolution_u = std::max(2, std::min(resolution_u, 32));
    resolution_v = std::max(2, std::min(resolution_v, 32));
    
    // CRITICAL: Pre-reserve output vector to prevent reallocations
    size_t expected_points = static_cast<size_t>(resolution_u) * resolution_v;
    output_points.reserve(output_points.size() + expected_points);
    
    // Generate points with exact bilinear interpolation
    for (int i = 0; i < resolution_u; ++i) {
        for (int j = 0; j < resolution_v; ++j) {
            float u = static_cast<float>(i) / (resolution_u - 1);
            float v = static_cast<float>(j) / (resolution_v - 1);
            
            // Bilinear interpolation between anchor points
            Vec3 top_edge = Vec3(
                anchor_positions[0].x * (1.0f - u) + anchor_positions[1].x * u,
                anchor_positions[0].y * (1.0f - u) + anchor_positions[1].y * u,
                anchor_positions[0].z * (1.0f - u) + anchor_positions[1].z * u
            );
            
            Vec3 bottom_edge = Vec3(
                anchor_positions[3].x * (1.0f - u) + anchor_positions[2].x * u,
                anchor_positions[3].y * (1.0f - u) + anchor_positions[2].y * u,
                anchor_positions[3].z * (1.0f - u) + anchor_positions[2].z * u
            );
            
            Vec3 final_position = Vec3(
                top_edge.x * (1.0f - v) + bottom_edge.x * v,
                top_edge.y * (1.0f - v) + bottom_edge.y * v,
                top_edge.z * (1.0f - v) + bottom_edge.z * v
            );
            
            PSFRealtimePoint point;
            point.position[0] = final_position.x;
            point.position[1] = final_position.y;
            point.position[2] = final_position.z;
            
            // Calculate surface normal
            Vec3 edge1 = Vec3(
                anchor_positions[1].x - anchor_positions[0].x,
                anchor_positions[1].y - anchor_positions[0].y,
                anchor_positions[1].z - anchor_positions[0].z
            );
            Vec3 edge2 = Vec3(
                anchor_positions[3].x - anchor_positions[0].x,
                anchor_positions[3].y - anchor_positions[0].y,
                anchor_positions[3].z - anchor_positions[0].z
            );
            
            Vec3 normal = Cross(edge1, edge2);
            NormalizeVector(normal);
            
            point.normal[0] = normal.x;
            point.normal[1] = normal.y;
            point.normal[2] = normal.z;
            
            // Evaluate color using mathematical color evaluator
bool uses_constant_colors = (surface.color_eqn_r == 0 && 
                            surface.color_eqn_g == 0 && 
                            surface.color_eqn_b == 0);

float finalColor[3];

if (uses_constant_colors) {
    // For constant color surfaces, evaluate once and use for all points
    // This prevents UV-based variation that shouldn't exist
    static float cached_color[3] = {-1, -1, -1};
    static uint32_t cached_surface = UINT32_MAX;
    
    if (cached_surface != surface_index) {
        // Evaluate at center UV (0.5, 0.5) once per surface
        MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, 0.5f, 0.5f, cached_color);
        cached_surface = surface_index;
        
        if (surface_index < 3) {
            std::cout << "Cached constant color for surface " << surface_index 
                      << ": (" << cached_color[0] << ", " << cached_color[1] << ", " << cached_color[2] << ")\n";
        }
    }
    
    finalColor[0] = cached_color[0];
    finalColor[1] = cached_color[1]; 
    finalColor[2] = cached_color[2];
    
} else {
    // For non-constant surfaces, evaluate per-point
    float safe_u = std::max(0.0f, std::min(1.0f, u));
    float safe_v = std::max(0.0f, std::min(1.0f, v));
    
    MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, safe_u, safe_v, finalColor);
}

// Validate and clamp the result
for (int c = 0; c < 3; ++c) {
    if (!std::isfinite(finalColor[c])) {
        finalColor[c] = 0.5f;
    }
    finalColor[c] = std::max(0.0f, std::min(1.0f, finalColor[c]));
}

point.albedo[0] = finalColor[0];
point.albedo[1] = finalColor[1];
point.albedo[2] = finalColor[2];

point.alpha = 1.0f;
point.surface_id = surface_index;
point.flags = 0;

// Set initial color equal to albedo (no lighting boost yet)
point.color[0] = point.albedo[0];
point.color[1] = point.albedo[1];
point.color[2] = point.albedo[2];            
            output_points.push_back(point);
        }
    }
}
// Add this missing function to psf_realtime_renderer.cpp
// Add it anywhere after the other PSFRealtimeGenerateXXXPoints functions:

void PSFRealtimePointGenerator::PSFRealtimeGenerateMetaballPoints(
    const ProceduralSurfaceBuffer& surface,
    uint32_t surface_index,
    float point_spacing,
    std::vector<PSFRealtimePoint>& output_points
) {
    // Extract metaball parameters from surface
    const float* params = SurfaceBufferUtils::ExtractParameters(&surface);
    
    // Metaball center and radius
    Vec3 center = Vec3(params[0], params[1], params[2]);
    float radius = params[3];
    
    // If radius is too small, use a default
    if (radius < 0.1f) {
        radius = 1.0f;
    }
    
    // Calculate resolution based on radius and point spacing
    int phi_steps = static_cast<int>((2.0f * 3.14159f * radius) / point_spacing);
    int theta_steps = static_cast<int>((3.14159f * radius) / point_spacing);
    
    // Clamp resolution for performance
    phi_steps = std::max(8, std::min(phi_steps, 32));
    theta_steps = std::max(4, std::min(theta_steps, 16));
    
    // Generate points on sphere surface
    for (int i = 0; i < theta_steps; ++i) {
        for (int j = 0; j < phi_steps; ++j) {
            float theta = static_cast<float>(i) * 3.14159f / (theta_steps - 1);
            float phi = static_cast<float>(j) * 2.0f * 3.14159f / phi_steps;
            
            PSFRealtimePoint point;
            
            // Spherical coordinates to cartesian
            float sin_theta = sinf(theta);
            point.position[0] = center.x + radius * sin_theta * cosf(phi);
            point.position[1] = center.y + radius * cosf(theta);
            point.position[2] = center.z + radius * sin_theta * sinf(phi);
            
            // Normal points outward from center
            point.normal[0] = sin_theta * cosf(phi);
            point.normal[1] = cosf(theta);
            point.normal[2] = sin_theta * sinf(phi);
            
            // Simple color based on position
            point.albedo[0] = 0.8f + 0.2f * sin_theta;
            point.albedo[1] = 0.6f + 0.4f * cosf(phi);
            point.albedo[2] = 0.9f;
            
            point.alpha = 1.0f;
            point.surface_id = surface_index;
            point.flags = 0;
            
            point.color[0] = point.albedo[0];
            point.color[1] = point.albedo[1];
            point.color[2] = point.albedo[2];
            
            output_points.push_back(point);
        }
    }
}
// ================================
// PSFRealtimeLightingEngine Implementation
// ================================

// REPLACE this function in psf_realtime_renderer.cpp
void PSFRealtimeLightingEngine::PSFRealtimeComputeLighting(
    std::vector<PSFRealtimePoint>& points,
    const std::vector<PSFRealtimeLight>& lights,
    const ProceduralObjectCollection& scene,
    const PSFRealtimeCamera& camera,
    const PSFRealtimeConfig& config,
    PSFRealtimeStats& stats
) {
    // FIXED: Preserve mathematical colors with NO brightness changes
    for (auto& point : points) {
        if (point.flags & 0x1) continue; // Skip culled points
        
        // Use the mathematically evaluated albedo colors EXACTLY as-is
        point.color[0] = point.albedo[0];
        point.color[1] = point.albedo[1];
        point.color[2] = point.albedo[2];
        
        // NO lighting modifications - preserve exact mathematical colors
    }
}
void PSFRealtimeLightingEngine::PSFRealtimeComputeDirectLighting(
    PSFRealtimePoint& point,
    const std::vector<PSFRealtimeLight>& lights,
    const ProceduralObjectCollection& scene,
    const PSFRealtimeConfig& config
) {
    for (const auto& light : lights) {
        float light_vector[3];
        float light_intensity;
        
        if (light.is_directional) {
            light_vector[0] = -light.position[0];
            light_vector[1] = -light.position[1];
            light_vector[2] = -light.position[2];
            light_intensity = light.intensity;
        } else {
            light_vector[0] = light.position[0] - point.position[0];
            light_vector[1] = light.position[1] - point.position[1];
            light_vector[2] = light.position[2] - point.position[2];
            
            float distance = sqrt(light_vector[0] * light_vector[0] + 
                                 light_vector[1] * light_vector[1] + 
                                 light_vector[2] * light_vector[2]);
            
            if (distance > light.radius || distance < 1e-6f) continue;
            
            light_vector[0] /= distance;
            light_vector[1] /= distance;
            light_vector[2] /= distance;
            
            float attenuation = 1.0f - (distance / light.radius);
            light_intensity = light.intensity * attenuation * attenuation;
        }
        
        float dot_product = light_vector[0] * point.normal[0] + 
                           light_vector[1] * point.normal[1] + 
                           light_vector[2] * point.normal[2];
        
        if (dot_product > 0.0f) {
            point.color[0] += point.albedo[0] * light.color[0] * light_intensity * dot_product;
            point.color[1] += point.albedo[1] * light.color[1] * light_intensity * dot_product;
            point.color[2] += point.albedo[2] * light.color[2] * light_intensity * dot_product;
        }
    }
}

void PSFRealtimeLightingEngine::PSFRealtimeComputeGlobalIllumination(
    PSFRealtimePoint& point,
    const std::vector<PSFRealtimeLight>& lights,
    const ProceduralObjectCollection& scene,
    const PSFRealtimeConfig& config
) {
    float gi_contribution[3] = {
        config.gi_intensity * 0.1f,
        config.gi_intensity * 0.1f,
        config.gi_intensity * 0.1f
    };
    
    point.color[0] += point.albedo[0] * gi_contribution[0];
    point.color[1] += point.albedo[1] * gi_contribution[1];
    point.color[2] += point.albedo[2] * gi_contribution[2];
}

void PSFRealtimeLightingEngine::PSFRealtimeApplyTransparency(
    PSFRealtimePoint& point,
    const float direct_light[3],
    const PSFRealtimeConfig& config
) {
    float transparency = 1.0f - point.alpha;
    
    point.color[0] = point.color[0] * point.alpha + transparency * 0.2f;
    point.color[1] = point.color[1] * point.alpha + transparency * 0.2f;
    point.color[2] = point.color[2] * point.alpha + transparency * 0.2f;
}

// ================================
// PSFRealtimeCullingEngine Implementation
// ================================

void PSFRealtimeCullingEngine::PSFRealtimePerformCulling(
    std::vector<PSFRealtimePoint>& points,
    const PSFRealtimeCamera& camera,
    const PSFRealtimeConfig& config,
    PSFRealtimeStats& stats
) {
    for (auto& point : points) {
        point.flags = 0; // Reset flags
        
        // Simple distance-based culling
        float dx = point.position[0] - camera.position[0];
        float dy = point.position[1] - camera.position[1];
        float dz = point.position[2] - camera.position[2];
        float distance = sqrt(dx*dx + dy*dy + dz*dz);
        
        if (distance > camera.far_plane) {
            point.flags |= 0x1; // Mark as culled
        }
    }
}

// ================================
// PSFRealtimeIntegration Implementation
// ================================

std::unique_ptr<PSFRealtimeRenderer> PSFRealtimeIntegration::PSFRealtimeCreateDefaultRenderer() {
    PSFRealtimeConfig config;
    config.max_points_per_surface = 2000;
    config.enable_global_illumination = true;
    config.debug_timing = true;
    return std::make_unique<PSFRealtimeRenderer>(config);
}

bool PSFRealtimeIntegration::PSFRealtimeConvertAndRender(
    const std::string& psf_filename,
    const std::string& output_format
) {
    ProceduralObjectCollection collection;
    if (!collection.LoadFromFile(psf_filename)) {
        return false;
    }
    
    auto renderer = PSFRealtimeCreateDefaultRenderer();
    
    float camera_pos[3] = {5.0f, 5.0f, 5.0f};
    float camera_target[3] = {0.0f, 0.0f, 0.0f};
    PSFRealtimeCamera camera = PSFRealtimeRenderer::PSFRealtimeCreateCamera(
        camera_pos, camera_target
    );
    
    float light_pos[3] = {2.0f, 4.0f, 2.0f};
    float light_color[3] = {1.0f, 1.0f, 1.0f};
    std::vector<PSFRealtimeLight> lights = {
        PSFRealtimeRenderer::PSFRealtimeCreatePointLight(
            light_pos, light_color, 2.0f, 15.0f
        )
    };
    
    std::vector<PSFRealtimePoint> points;
    bool success = renderer->PSFRealtimeRenderFrame(collection, lights, camera, points);
    
    if (success && output_format == "preview") {
        PSFRealtimePrintStats(renderer->PSFRealtimeGetLastFrameStats());
    }
    
    return success;
}

void PSFRealtimeIntegration::PSFRealtimePrintStats(const PSFRealtimeStats& stats) {
    printf("\n=== PSF REALTIME RENDER STATS ===\n");
    printf("Surfaces: %u\n", stats.total_surfaces);
    printf("Points Generated: %u\n", stats.total_points_generated);
    printf("Visible Points: %u\n", stats.visible_points);
    printf("Total Frame: %.2f ms (%.1f FPS)\n", 
           stats.total_frame_time_ms, 1000.0f / stats.total_frame_time_ms);
    printf("================================\n");
}

bool PSFRealtimeIntegration::PSFRealtimeExportPoints(
    const std::vector<PSFRealtimePoint>& points,
    const std::string& filename,
    const std::string& format
) {
    // Simple implementation - just return true for now
    return true;
}