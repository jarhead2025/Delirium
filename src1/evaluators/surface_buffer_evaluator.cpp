#include "surface_buffer_evaluator.h"
#include "anchorpoint_evaluator.h"
#include "mathematical_color_evaluator.h"
#include "surface_buffers.h"
#include <cmath>
#include <algorithm>









Vec3 SurfaceBufferEvaluator::GetClosestAnchorNormal(
    const ProceduralSurfaceBuffer* surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    float u, float v) {
    
    // Determinar qué anchor está más cerca basado en UV
    int closest_anchor = 0;
    
    if (u <= 0.5f && v <= 0.5f) {
        closest_anchor = 0;  // bottom-left
    } else if (u > 0.5f && v <= 0.5f) {
        closest_anchor = 1;  // bottom-right
    } else if (u > 0.5f && v > 0.5f) {
        closest_anchor = 2;  // top-right
    } else {
        closest_anchor = 3;  // top-left
    }
    
    uint16_t anchor_idx = surface->anchor_point_indices[closest_anchor];
    if (anchor_idx < anchor_pool.size()) {
        return AnchorPointEvaluator::DecodeNormal(anchor_pool[anchor_idx].normal);
    }
    
    // Ultimate fallback
    return Vec3{0, 0, 1};
}

void SurfaceBufferEvaluator::DebugFaceContinuity(
    const ProceduralSurfaceBuffer* surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    const std::vector<Matrix4x4>& bone_transforms) {
    
    std::cout << "\n=== DEBUGGING FACE CONTINUITY ===\n";
    
    if (!surface) {
        std::cout << "No surface to debug\n";
        return;
    }
    
    // Test continuidad en bordes compartidos
    std::cout << "Testing edge continuity...\n";
    
    // Edge bottom: (u=0.5, v=0) vs (u=0.5, v=0.1)
    float pos1[3], pos2[3], norm1[3], norm2[3];
    
    EvaluateSurface(surface, anchor_pool, bone_transforms, 0.5f, 0.0f, pos1, norm1);
    EvaluateSurface(surface, anchor_pool, bone_transforms, 0.5f, 0.1f, pos2, norm2);
    
    float pos_diff = sqrt(pow(pos2[0]-pos1[0], 2) + pow(pos2[1]-pos1[1], 2) + pow(pos2[2]-pos1[2], 2));
    float norm_dot = norm1[0]*norm2[0] + norm1[1]*norm2[1] + norm1[2]*norm2[2];
    
    std::cout << "Edge discontinuity: position=" << pos_diff << ", normal_similarity=" << norm_dot << "\n";
    
    if (pos_diff > 0.1f) {
        std::cout << "⚠️  Large position jump detected!\n";
    }
    if (norm_dot < 0.9f) {
        std::cout << "⚠️  Sharp normal change detected!\n";
    }
    
    // Test corners
    std::cout << "\nCorner positions:\n";
    float corners[4][2] = {{0,0}, {1,0}, {1,1}, {0,1}};
    for (int i = 0; i < 4; ++i) {
        float pos[3], norm[3];
        EvaluateSurface(surface, anchor_pool, bone_transforms,
                       corners[i][0], corners[i][1], pos, norm);
        std::cout << "Corner " << i << ": (" << pos[0] << "," << pos[1] << "," << pos[2] 
                  << ") normal(" << norm[0] << "," << norm[1] << "," << norm[2] << ")\n";
    }
    
    std::cout << "=== END DEBUG ===\n\n";
}

// =============================================================================
// FUNCIÓN: ValidateMeshQuality
// =============================================================================

void SurfaceBufferEvaluator::ValidateMeshQuality(
    const std::vector<float>& vertices,
    const std::vector<uint32_t>& indices,
    const std::vector<float>& normals) {
    
    std::cout << "\n=== MESH QUALITY VALIDATION ===\n";
    
    if (vertices.size() % 3 != 0) {
        std::cout << "❌ Invalid vertex array size: " << vertices.size() << "\n";
        return;
    }
    
    if (indices.size() % 4 != 0) {
        std::cout << "❌ Invalid index array size: " << indices.size() << "\n";
        return;
    }
    
    if (normals.size() != vertices.size()) {
        std::cout << "❌ Normal count mismatch: " << normals.size() << " vs " << vertices.size() << "\n";
        return;
    }
    
    uint32_t vertex_count = vertices.size() / 3;
    uint32_t face_count = indices.size() / 4;
    
    std::cout << "Mesh: " << vertex_count << " vertices, " << face_count << " faces\n";
    
    // Verificar indices válidos
    uint32_t invalid_indices = 0;
    for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] >= vertex_count) {
            invalid_indices++;
        }
    }
    
    if (invalid_indices > 0) {
        std::cout << "❌ Invalid indices found: " << invalid_indices << "\n";
    } else {
        std::cout << "✅ All indices valid\n";
    }
    
    // Verificar valores finitos
    uint32_t infinite_vertices = 0;
    uint32_t infinite_normals = 0;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (!std::isfinite(vertices[i])) infinite_vertices++;
    }
    
    for (size_t i = 0; i < normals.size(); ++i) {
        if (!std::isfinite(normals[i])) infinite_normals++;
    }
    
    if (infinite_vertices > 0) {
        std::cout << "❌ Non-finite vertices: " << infinite_vertices << "\n";
    } else {
        std::cout << "✅ All vertices finite\n";
    }
    
    if (infinite_normals > 0) {
        std::cout << "❌ Non-finite normals: " << infinite_normals << "\n";
    } else {
        std::cout << "✅ All normals finite\n";
    }
    
    // Calcular bounding box
    if (vertex_count > 0) {
        float min_x = vertices[0], max_x = vertices[0];
        float min_y = vertices[1], max_y = vertices[1];
        float min_z = vertices[2], max_z = vertices[2];
        
        for (size_t i = 3; i < vertices.size(); i += 3) {
            min_x = std::min(min_x, vertices[i]);
            max_x = std::max(max_x, vertices[i]);
            min_y = std::min(min_y, vertices[i+1]);
            max_y = std::max(max_y, vertices[i+1]);
            min_z = std::min(min_z, vertices[i+2]);
            max_z = std::max(max_z, vertices[i+2]);
        }
        
        std::cout << "Bounding box: (" << min_x << "," << min_y << "," << min_z 
                  << ") to (" << max_x << "," << max_y << "," << max_z << ")\n";
        
        float size_x = max_x - min_x;
        float size_y = max_y - min_y;
        float size_z = max_z - min_z;
        std::cout << "Dimensions: " << size_x << " x " << size_y << " x " << size_z << "\n";
    }
    
    std::cout << "=== END VALIDATION ===\n\n";
}

// =============================================================================
// FUNCIÓN: CalculateSurfaceStats
// =============================================================================

void SurfaceBufferEvaluator::CalculateSurfaceStats(
    const ProceduralSurfaceBuffer* surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    const std::vector<Matrix4x4>& bone_transforms,
    uint32_t sample_resolution) {
    
    std::cout << "\n=== SURFACE STATISTICS ===\n";
    
    if (!surface) {
        std::cout << "No surface provided\n";
        return;
    }
    
    uint32_t total_samples = sample_resolution * sample_resolution;
    std::cout << "Sampling " << total_samples << " points...\n";
    
    float min_x = 1e10f, max_x = -1e10f;
    float min_y = 1e10f, max_y = -1e10f;
    float min_z = 1e10f, max_z = -1e10f;
    
    uint32_t valid_samples = 0;
    uint32_t invalid_samples = 0;
    
    for (uint32_t row = 0; row < sample_resolution; ++row) {
        for (uint32_t col = 0; col < sample_resolution; ++col) {
            float u = static_cast<float>(col) / (sample_resolution - 1);
            float v = static_cast<float>(row) / (sample_resolution - 1);
            
            float pos[3], norm[3];
            EvaluateSurface(surface, anchor_pool, bone_transforms, u, v, pos, norm);
            
            if (std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2])) {
                min_x = std::min(min_x, pos[0]);
                max_x = std::max(max_x, pos[0]);
                min_y = std::min(min_y, pos[1]);
                max_y = std::max(max_y, pos[1]);
                min_z = std::min(min_z, pos[2]);
                max_z = std::max(max_z, pos[2]);
                valid_samples++;
            } else {
                invalid_samples++;
            }
        }
    }
    
    std::cout << "Valid samples: " << valid_samples << "/" << total_samples << "\n";
    if (invalid_samples > 0) {
        std::cout << "⚠️  Invalid samples: " << invalid_samples << "\n";
    }
    
    if (valid_samples > 0) {
        std::cout << "Surface bounds:\n";
        std::cout << "  X: [" << min_x << ", " << max_x << "] (size: " << (max_x - min_x) << ")\n";
        std::cout << "  Y: [" << min_y << ", " << max_y << "] (size: " << (max_y - min_y) << ")\n";
        std::cout << "  Z: [" << min_z << ", " << max_z << "] (size: " << (max_z - min_z) << ")\n";
        
        float center_x = (min_x + max_x) / 2.0f;
        float center_y = (min_y + max_y) / 2.0f;
        float center_z = (min_z + max_z) / 2.0f;
        std::cout << "Center: (" << center_x << ", " << center_y << ", " << center_z << ")\n";
    }
    
    std::cout << "=== END STATISTICS ===\n\n";
}

// =============================================================================
// FUNCIONES HELPER PRIVADAS
// =============================================================================

Vec3 SurfaceBufferEvaluator::CalculateSmoothedNormal(
    const std::vector<Vec3>& vertex_normals,
    uint32_t center_idx,
    uint32_t grid_width,
    uint32_t grid_height) {
    
    if (center_idx >= vertex_normals.size()) {
        return Vec3{0, 0, 1};
    }
    
    uint32_t row = center_idx / grid_width;
    uint32_t col = center_idx % grid_width;
    
    Vec3 avg_normal = {0, 0, 0};
    int neighbor_count = 0;
    
    // Average with 8-connected neighbors
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            int nr = static_cast<int>(row) + dr;
            int nc = static_cast<int>(col) + dc;
            
            if (nr >= 0 && nr < static_cast<int>(grid_height) && 
                nc >= 0 && nc < static_cast<int>(grid_width)) {
                uint32_t neighbor_idx = nr * grid_width + nc;
                
                if (neighbor_idx < vertex_normals.size()) {
                    avg_normal.x += vertex_normals[neighbor_idx].x;
                    avg_normal.y += vertex_normals[neighbor_idx].y;
                    avg_normal.z += vertex_normals[neighbor_idx].z;
                    neighbor_count++;
                }
            }
        }
    }
    
    if (neighbor_count > 0) {
        avg_normal.x /= neighbor_count;
        avg_normal.y /= neighbor_count;
        avg_normal.z /= neighbor_count;
        NormalizeVector(avg_normal);
    } else {
        avg_normal = vertex_normals[center_idx];
    }
    
    return avg_normal;
}

bool SurfaceBufferEvaluator::IsValidNormal(const Vec3& normal) {
    return std::isfinite(normal.x) && std::isfinite(normal.y) && std::isfinite(normal.z) &&
           (normal.x*normal.x + normal.y*normal.y + normal.z*normal.z) > 0.01f;
}

void SurfaceBufferEvaluator::ClampUV(float& u, float& v, float epsilon) {
    u = std::max(epsilon, std::min(1.0f - epsilon, u));
    v = std::max(epsilon, std::min(1.0f - epsilon, v));
}




void SurfaceBufferEvaluator::EvaluateSurface(
    const ProceduralSurfaceBuffer* surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    const std::vector<Matrix4x4>& bone_transforms,
    float u, float v,
    float* outPosition, float* outNormal)
{
    if (!surface || !outPosition || !outNormal) {
        return;
    }

    // Initialize output arrays
    outPosition[0] = outPosition[1] = outPosition[2] = 0.0f;
    outNormal[0] = outNormal[1] = outNormal[2] = 0.0f;

    // Evaluate the base surface patch
    EvaluateBasePatch(surface, anchor_pool, bone_transforms, u, v, outPosition);

    // Apply any combination operations
    ApplyCombination(surface, outPosition);

    // Apply surface modifiers
    ApplyModifiers(surface, outPosition, u, v);

    // Calculate surface normal
    CalculateSurfaceNormal(surface, anchor_pool, bone_transforms, u, v, outNormal);
}

void SurfaceBufferEvaluator::EvaluateSurfaceMesh(
    const ProceduralSurfaceBuffer* surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    const std::vector<Matrix4x4>& bone_transforms,
    uint32_t resolution,
    std::vector<float>& outVertices,
    std::vector<uint32_t>& outIndices,
    std::vector<float>& outNormals,
    float time) {
    
    if (!surface || resolution < 2) {
        return;
    }

    outVertices.clear();
    outIndices.clear();
    outNormals.clear();

    // INCREASED minimum resolution to reduce stretching
    uint32_t safe_resolution = std::max(resolution, 8u);  // Minimum 8x8 grid
    safe_resolution = std::min(safe_resolution, 32u);     // Maximum 32x32 for performance
    
    std::cout << "Generating IMPROVED mesh: " << safe_resolution << "x" << safe_resolution << "\n";

    // Pre-allocate
    uint32_t vertex_count = (safe_resolution + 1) * (safe_resolution + 1);
    outVertices.reserve(vertex_count * 3);
    outNormals.reserve(vertex_count * 3);

    // STEP 1: Generate vertices with CONSISTENT UV mapping
    std::vector<Vec3> vertex_positions(vertex_count);
    std::vector<Vec3> vertex_normals(vertex_count);
    
    for (uint32_t row = 0; row <= safe_resolution; ++row) {
        for (uint32_t col = 0; col <= safe_resolution; ++col) {
            uint32_t vertex_idx = row * (safe_resolution + 1) + col;
            
            // CORRECTED UV mapping
            float u = static_cast<float>(col) / static_cast<float>(safe_resolution);
            float v = static_cast<float>(row) / static_cast<float>(safe_resolution);

            float position[3], normal[3];
            EvaluateSurface(surface, anchor_pool, bone_transforms, u, v, position, normal);

            vertex_positions[vertex_idx] = Vec3{position[0], position[1], position[2]};
            vertex_normals[vertex_idx] = Vec3{normal[0], normal[1], normal[2]};
        }
    }
    
    // STEP 2: Smooth normals with improved algorithm
    std::vector<Vec3> smoothed_normals = vertex_normals;
    
    for (uint32_t row = 1; row < safe_resolution; ++row) {
        for (uint32_t col = 1; col < safe_resolution; ++col) {
            uint32_t center_idx = row * (safe_resolution + 1) + col;
            
            // Average with 4-connected neighbors (not 8-connected to avoid over-smoothing)
            Vec3 avg_normal = vertex_normals[center_idx];
            int neighbor_count = 1;
            
            // Add neighboring normals
            int neighbors[][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}};
            for (auto& neighbor : neighbors) {
                int nr = row + neighbor[0];
                int nc = col + neighbor[1];
                
                if (nr >= 0 && nr <= safe_resolution && nc >= 0 && nc <= safe_resolution) {
                    uint32_t neighbor_idx = nr * (safe_resolution + 1) + nc;
                    avg_normal.x += vertex_normals[neighbor_idx].x;
                    avg_normal.y += vertex_normals[neighbor_idx].y;
                    avg_normal.z += vertex_normals[neighbor_idx].z;
                    neighbor_count++;
                }
            }
            
            if (neighbor_count > 1) {
                avg_normal.x /= neighbor_count;
                avg_normal.y /= neighbor_count;
                avg_normal.z /= neighbor_count;
                NormalizeVector(avg_normal);
                smoothed_normals[center_idx] = avg_normal;
            }
        }
    }

    // STEP 3: Convert to arrays
    for (uint32_t i = 0; i < vertex_count; ++i) {
        outVertices.push_back(vertex_positions[i].x);
        outVertices.push_back(vertex_positions[i].y);
        outVertices.push_back(vertex_positions[i].z);
        
        outNormals.push_back(smoothed_normals[i].x);
        outNormals.push_back(smoothed_normals[i].y);
        outNormals.push_back(smoothed_normals[i].z);
    }

    // STEP 4: Generate indices with CONSISTENT winding order
    for (uint32_t row = 0; row < safe_resolution; ++row) {
        for (uint32_t col = 0; col < safe_resolution; ++col) {
            uint32_t stride = safe_resolution + 1;
            uint32_t bl = row * stride + col;        // bottom-left
            uint32_t br = row * stride + col + 1;    // bottom-right
            uint32_t tl = (row + 1) * stride + col;  // top-left
            uint32_t tr = tl + 1;                    // top-right
            
            // CONSISTENT counter-clockwise winding for all quads
            outIndices.push_back(bl);
            outIndices.push_back(br);
            outIndices.push_back(tr);
            outIndices.push_back(tl);
        }
    }
    
    std::cout << "IMPROVED mesh generated: " << outVertices.size()/3 << " vertices, " 
              << outIndices.size()/4 << " faces\n";
}


void SurfaceBufferEvaluator::CalculateSurfaceNormal(
    const ProceduralSurfaceBuffer* surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    const std::vector<Matrix4x4>& bone_transforms,
    float u, float v,
    float* outNormal) {
    
    if (!surface || !outNormal) {
        return;
    }

    // MEJORA: Usar differentials más pequeños para mejor precisión
    const float epsilon = 0.001f;
    
    // Clamp UV coordinates para evitar problemas en bordes
    u = std::max(epsilon, std::min(1.0f - epsilon, u));
    v = std::max(epsilon, std::min(1.0f - epsilon, v));
    
    // Calculate positions for tangent vectors
    float pos_center[3], pos_u_plus[3], pos_v_plus[3];
    float pos_u_minus[3], pos_v_minus[3];
    
    // Central position
    EvaluateBasePatch(surface, anchor_pool, bone_transforms, u, v, pos_center);
    
    // Forward differences
    EvaluateBasePatch(surface, anchor_pool, bone_transforms, 
                     std::min(1.0f, u + epsilon), v, pos_u_plus);
    EvaluateBasePatch(surface, anchor_pool, bone_transforms, 
                     u, std::min(1.0f, v + epsilon), pos_v_plus);
                     
    // Backward differences para mejor estabilidad numérica
    EvaluateBasePatch(surface, anchor_pool, bone_transforms,
                     std::max(0.0f, u - epsilon), v, pos_u_minus);
    EvaluateBasePatch(surface, anchor_pool, bone_transforms,
                     u, std::max(0.0f, v - epsilon), pos_v_minus);

    // MEJORA: Usar central differences para mejor precisión
    Vec3 tangent_u = {
        (pos_u_plus[0] - pos_u_minus[0]) / (2.0f * epsilon),
        (pos_u_plus[1] - pos_u_minus[1]) / (2.0f * epsilon),
        (pos_u_plus[2] - pos_u_minus[2]) / (2.0f * epsilon)
    };
    
    Vec3 tangent_v = {
        (pos_v_plus[0] - pos_v_minus[0]) / (2.0f * epsilon),
        (pos_v_plus[1] - pos_v_minus[1]) / (2.0f * epsilon),
        (pos_v_plus[2] - pos_v_minus[2]) / (2.0f * epsilon)
    };

    // Cross product para normal
    Vec3 normal = Cross(tangent_u, tangent_v);
    
    // CRÍTICO: Verificar que la normal es válida
    float length_sq = normal.x*normal.x + normal.y*normal.y + normal.z*normal.z;
    if (length_sq < 1e-6f) {
        // Fallback: usar normal desde anchor más cercano
        Vec3 anchor_normal = GetClosestAnchorNormal(surface, anchor_pool, u, v);
        outNormal[0] = anchor_normal.x;
        outNormal[1] = anchor_normal.y;
        outNormal[2] = anchor_normal.z;
        return;
    }
    
    NormalizeVector(normal);
    
    // ASEGURAR orientación consistente (hacia afuera)
    // Si la normal apunta hacia abajo, invertirla
    if (normal.z < 0.0f) {
        normal.x = -normal.x;
        normal.y = -normal.y;
        normal.z = -normal.z;
    }

    outNormal[0] = normal.x;
    outNormal[1] = normal.y;
    outNormal[2] = normal.z;
}

void SurfaceBufferEvaluator::EvaluateBasePatch(

    const ProceduralSurfaceBuffer* surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    const std::vector<Matrix4x4>& bone_transforms,
    float u, float v,
    float* outPosition) {
    
    if (!surface || !outPosition) {
        return;
    }
    
    // Clamp UV coordinates to valid range
    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));

    // Extract surface type
    SurfaceEquationType type = ExtractBaseType(surface->surface_definition_index);
    if (surface->eqn_params[4] != 0.0f || surface->eqn_params[5] != 0.0f || 
    surface->eqn_params[6] != 0.0f || surface->eqn_params[7] != 0.0f) {
    EvaluateUVMappedPatch(surface, anchor_pool, bone_transforms, u, v, outPosition);
    return;
}
    // Get anchor points with CORRECTED ordering and validation
    Vec3 anchors[4];
    bool valid_anchors = true;
    
    for (int i = 0; i < 4; ++i) {
        uint16_t anchorIndex = surface->anchor_point_indices[i];
        if (anchorIndex < anchor_pool.size()) {
            Vec3 pos, normal;
            AnchorPointEvaluator::EvaluateAnchorPoint(
                anchor_pool[anchorIndex], bone_transforms, 0.0f, pos, normal);
            anchors[i] = pos;
        } else {
            // IMPROVED fallback: Create proper grid layout
            float corner_u = (i % 2) ? 1.0f : 0.0f;  // 0,1,1,0
            float corner_v = (i >= 2) ? 1.0f : 0.0f; // 0,0,1,1
            anchors[i] = Vec3{
                corner_u * 2.0f - 1.0f,  // [-1,+1] range
                corner_v * 2.0f - 1.0f,  // [-1,+1] range
                0.0f
            };
            valid_anchors = false;
        }
    }

    // CRITICAL FIX: Ensure consistent anchor ordering
    // Standard layout: [0]=bottom-left, [1]=bottom-right, [2]=top-right, [3]=top-left
    // But we need to verify this is actually the case by sorting spatially
    
    if (valid_anchors) {
        // Sort anchors to ensure proper quad layout
        std::vector<std::pair<Vec3, int>> anchor_with_index;
        for (int i = 0; i < 4; ++i) {
            anchor_with_index.push_back({anchors[i], i});
        }
        
        // Find the anchor with smallest (x+y) - should be bottom-left
        std::sort(anchor_with_index.begin(), anchor_with_index.end(), 
                  [](const auto& a, const auto& b) {
                      return (a.first.x + a.first.y) < (b.first.x + b.first.y);
                  });
        
        Vec3 sorted_anchors[4];
        Vec3 bottom_left = anchor_with_index[0].first;
        
        // Re-sort by position relative to bottom_left
        std::sort(anchor_with_index.begin(), anchor_with_index.end(),
                  [&bottom_left](const auto& a, const auto& b) {
                      float angle_a = atan2(a.first.y - bottom_left.y, a.first.x - bottom_left.x);
                      float angle_b = atan2(b.first.y - bottom_left.y, b.first.x - bottom_left.x);
                      return angle_a < angle_b;
                  });
        
        // Assign in counter-clockwise order: bottom-left, bottom-right, top-right, top-left
        for (int i = 0; i < 4; ++i) {
            sorted_anchors[i] = anchor_with_index[i].first;
        }
        
        // Copy back to anchors array
        for (int i = 0; i < 4; ++i) {
            anchors[i] = sorted_anchors[i];
        }
    }

    // CORRECTED surface evaluation with proper interpolation
    switch (type) {
        case EQUATION_PLANE:
        case EQUATION_BILINEAR:
        default: {
            // FIXED bilinear interpolation with correct anchor ordering
            // anchors[0] = bottom-left (0,0)
            // anchors[1] = bottom-right (1,0)  
            // anchors[2] = top-right (1,1)
            // anchors[3] = top-left (0,1)
            
            // Interpolate bottom edge: anchors[0] to anchors[1]
            Vec3 bottom_edge = {
                anchors[0].x + u * (anchors[1].x - anchors[0].x),
                anchors[0].y + u * (anchors[1].y - anchors[0].y),
                anchors[0].z + u * (anchors[1].z - anchors[0].z)
            };
            
            // Interpolate top edge: anchors[3] to anchors[2] 
            Vec3 top_edge = {
                anchors[3].x + u * (anchors[2].x - anchors[3].x),
                anchors[3].y + u * (anchors[2].y - anchors[3].y),
                anchors[3].z + u * (anchors[2].z - anchors[3].z)
            };
            
            // Final interpolation between bottom and top edges
            Vec3 result = {
                bottom_edge.x + v * (top_edge.x - bottom_edge.x),
                bottom_edge.y + v * (top_edge.y - bottom_edge.y),
                bottom_edge.z + v * (top_edge.z - bottom_edge.z)
            };
            
            outPosition[0] = result.x;
            outPosition[1] = result.y;
            outPosition[2] = result.z;
            break;
        }
        
        case EQUATION_SPHERE: {
            // IMPROVED sphere evaluation
            float radius = (surface->eqn_params[0] > 0.001f) ? surface->eqn_params[0] : 1.0f;
            
            // Use first anchor as center
            Vec3 center = anchors[0];
            
            // Convert UV to spherical coordinates
            float theta = u * 2.0f * M_PI;           // Azimuth: [0, 2π]
            float phi = v * M_PI;                    // Elevation: [0, π]
            
            outPosition[0] = center.x + radius * sin(phi) * cos(theta);
            outPosition[1] = center.y + radius * sin(phi) * sin(theta);
            outPosition[2] = center.z + radius * cos(phi);
            break;
        }
        
        case EQUATION_CYLINDER: {
            // IMPROVED cylinder evaluation
            float radius = (surface->eqn_params[0] > 0.001f) ? surface->eqn_params[0] : 0.5f;
            float theta = u * 2.0f * M_PI;
            
            // Interpolate height between anchors[0] and anchors[1]
            Vec3 bottom = anchors[0];
            Vec3 top = anchors[1];
            Vec3 axis_point = {
                bottom.x + v * (top.x - bottom.x),
                bottom.y + v * (top.y - bottom.y),
                bottom.z + v * (top.z - bottom.z)
            };
            
            outPosition[0] = axis_point.x + radius * cos(theta);
            outPosition[1] = axis_point.y + radius * sin(theta);
            outPosition[2] = axis_point.z;
            break;
        }
    }
    
    // VALIDATION: Ensure output is finite
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(outPosition[i])) {
            // Fallback to simple planar interpolation
            Vec3 fallback = {
                anchors[0].x + u * (anchors[1].x - anchors[0].x) + v * (anchors[3].x - anchors[0].x),
                anchors[0].y + u * (anchors[1].y - anchors[0].y) + v * (anchors[3].y - anchors[0].y),
                anchors[0].z + u * (anchors[1].z - anchors[0].z) + v * (anchors[3].z - anchors[0].z)
            };
            outPosition[0] = fallback.x;
            outPosition[1] = fallback.y;
            outPosition[2] = fallback.z;
            break;
        }
    }
}


void SurfaceBufferEvaluator::ApplyCombination(
    const ProceduralSurfaceBuffer* surface,
    float* position)
{
    if (!surface || !position) {
        return;
    }

    CombinationOperation combo = ExtractCombination(surface->surface_definition_index);
    
    if (combo == COMBO_NONE) {
        return;
    }

    // Apply combination operations (simplified implementation)
    float blendWeight = static_cast<float>(surface->combination_param) / 255.0f;
    
    switch (combo) {
        case COMBO_BLEND: {
            // Simple blend - could be enhanced with secondary surface evaluation
            float scale = 1.0f + blendWeight * 0.1f;
            position[0] *= scale;
            position[1] *= scale;
            position[2] *= scale;
            break;
        }
        
        case COMBO_ADD: {
            // Additive displacement
            float offset = blendWeight * 0.1f;
            position[0] += offset;
            position[1] += offset;
            position[2] += offset;
            break;
        }
        
        default:
            break;
    }
}

void SurfaceBufferEvaluator::ApplyModifiers(
    const ProceduralSurfaceBuffer* surface,
    float* position,
    float u, float v)
{
    if (!surface || !position) {
        return;
    }

    uint8_t modifiers = ExtractModifiers(surface->surface_definition_index);
    
    if (modifiers & MOD_TWIST) {
        float angle = v * surface->eqn_params[6] * 0.1f;
        float cos_a = cos(angle);
        float sin_a = sin(angle);
        float new_x = position[0] * cos_a - position[1] * sin_a;
        float new_y = position[0] * sin_a + position[1] * cos_a;
        position[0] = new_x;
        position[1] = new_y;
    }
    
    if (modifiers & MOD_BEND) {
        float bend = surface->eqn_params[7] * 0.1f;
        position[2] += position[0] * position[0] * bend;
    }
    
    if (modifiers & MOD_NOISE) {
        // Simple noise displacement
        float noise = sin(u * 20.0f) * sin(v * 20.0f) * 0.01f;
        position[0] += noise;
        position[1] += noise;
        position[2] += noise;
    }
}

void SurfaceBufferEvaluator::EvaluatePrimitive_Absolute(
    const ProceduralSurfaceBuffer* surface,
    float* position)
{
    if (!surface || !position) {
        return;
    }

    // Take absolute value of coordinates
    position[0] = fabs(position[0]);
    position[1] = fabs(position[1]);
    position[2] = fabs(position[2]);
}

void SurfaceBufferEvaluator::EvaluatePatch_Displacement(
    const ProceduralSurfaceBuffer* surface,
    float* position,
    float u, float v)
{
    if (!surface || !position) {
        return;
    }

    // Apply displacement based on surface parameters
    float displacement = surface->eqn_params[0] * sin(u * M_PI) * sin(v * M_PI);
    position[2] += displacement;
}


void SurfaceBufferEvaluator::EvaluateUVMappedPatch(
    const ProceduralSurfaceBuffer* surface,
    const std::vector<AnchorPointBuffer>& anchor_pool,
    const std::vector<Matrix4x4>& bone_transforms,
    float u, float v,
    float* outPosition) {
    
    if (!surface || !outPosition) {
        return;
    }

    // For now, just call regular evaluation
    EvaluateBasePatch(surface, anchor_pool, bone_transforms, u, v, outPosition);
}
// ALSO ADD: Better anchor positioning function to SmartMeshConverter
// Add this to the CreateAnchorsFromUVPatches function in main.cpp:



// Helper function to select most separated vertices
static std::vector<Vec3> SelectMostSeparatedVertices(const std::vector<Vec3>& vertices, int count) {
    if (vertices.size() <= count) return vertices;
    
    std::vector<Vec3> selected;
    std::vector<bool> used(vertices.size(), false);
    
    // Start with first vertex
    selected.push_back(vertices[0]);
    used[0] = true;
    
    // Greedily select vertices that are farthest from already selected ones
    for (int i = 1; i < count; ++i) {
        int best_vertex = -1;
        float max_min_distance = 0;
        
        for (size_t j = 0; j < vertices.size(); ++j) {
            if (used[j]) continue;
            
            // Find minimum distance to already selected vertices
            float min_distance = 1e10f;
            for (const Vec3& sel : selected) {
                float dist = sqrt(pow(vertices[j].x - sel.x, 2) + 
                                pow(vertices[j].y - sel.y, 2) + 
                                pow(vertices[j].z - sel.z, 2));
                min_distance = std::min(min_distance, dist);
            }
            
            if (min_distance > max_min_distance) {
                max_min_distance = min_distance;
                best_vertex = static_cast<int>(j);
            }
        }
        
        if (best_vertex >= 0) {
            selected.push_back(vertices[best_vertex]);
            used[best_vertex] = true;
        }
    }
    
    return selected;
}