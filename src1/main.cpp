#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <chrono>
#include <memory>
#include <cstring>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <map>
#include <limits>

// Conditional FBX includes
#ifndef NO_FBX_SDK
#include <fbxsdk.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Your dependencies
#include "object_descriptor_buffers.h"
#include "mathematical_color_evaluator.h"
#include "surface_buffers.h"
#include "math_types.h"
#include "surface_buffer_evaluator.h"
#include "anchorpoint_evaluator.h"
#include "bone_evaluator.h"

// Forward declarations
struct EnhancedMeshData;
struct GridPatch;
struct TessellatedSurface;

// Utility functions declarations
static float Distance(const Vec3& a, const Vec3& b);
static std::pair<Vec3, Vec3> FindBoundingBox(const std::vector<Vec3>& positions);
static void CalculatePlaneParameters(const Vec3& v0, const Vec3& v1, const Vec3& v2, float params[8]);
static float DotProduct(const Vec3& a, const Vec3& b);
static bool AreNormalsSimilar(const Vec3& n1, const Vec3& n2, float angleThresholdDegrees);
static bool IsPointOnPlane(const Vec3& point, const float params[8], float tolerance);

// Enhanced mesh data with face information
struct EnhancedMeshData {
    std::string name;
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<std::vector<int>> faces; // Each face is a list of vertex indices
    
    void AddTriangle(int v1, int v2, int v3) {
        faces.push_back({v1, v2, v3});
    }
    
    void AddQuad(int v1, int v2, int v3, int v4) {
        faces.push_back({v1, v2, v3, v4});
    }
};

// Surface patch extracted from mesh
struct MeshPatch {
    std::vector<int> vertex_indices;  // Vertices that belong to this patch
    std::vector<int> face_indices;    // Faces that belong to this patch
    Vec3 center;                      // Geometric center
    Vec3 normal;                      // Average normal
    float area;                       // Surface area
    int anchor_indices[4];            // The 4 anchors that define this patch
    SurfaceEquationType best_equation;// Best-fit surface equation
    std::vector<float> equation_params; // Parameters for the equation
};

// Grid patch structure
struct GridPatch {
    int grid_x, grid_y;
    SurfaceEquationType equation_type;
    float complexity_score;
    bool can_merge_right, can_merge_down;
    std::vector<Vec3> corner_positions;
    std::vector<Vec3> corner_normals;
    std::vector<int> mesh_vertices; // Mesh vertices assigned to this patch
};

// Tessellated surface structure
struct TessellatedSurface {
    std::vector<GridPatch> patches;
    std::vector<Vec3> grid_anchors;
    int grid_width, grid_height;
    Vec3 surface_origin;
    float patch_size_x, patch_size_y;
};

// Utility function implementations
static float Distance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrt(dx*dx + dy*dy + dz*dz);
}

static std::pair<Vec3, Vec3> FindBoundingBox(const std::vector<Vec3>& positions) {
    if (positions.empty()) return {{0,0,0}, {0,0,0}};
    
    Vec3 min_pos = positions[0], max_pos = positions[0];
    
    for (const Vec3& pos : positions) {
        min_pos.x = std::min(min_pos.x, pos.x);
        min_pos.y = std::min(min_pos.y, pos.y);
        min_pos.z = std::min(min_pos.z, pos.z);
        max_pos.x = std::max(max_pos.x, pos.x);
        max_pos.y = std::max(max_pos.y, pos.y);
        max_pos.z = std::max(max_pos.z, pos.z);
    }
    
    return {min_pos, max_pos};
}

static void CalculatePlaneParameters(const Vec3& v0, const Vec3& v1, const Vec3& v2, float params[8]) {
    // Calculate normal using cross product of (v1 - v0) and (v2 - v0)
    Vec3 edge1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    Vec3 edge2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
    
    Vec3 normal;
    normal.x = edge1.y * edge2.z - edge1.z * edge2.y;
    normal.y = edge1.z * edge2.x - edge1.x * edge2.z;
    normal.z = edge1.x * edge2.y - edge1.y * edge2.x;
    
    // Normalize the normal
    float length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length > 1e-6f) {
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
    } else {
        normal = {0.0f, 0.0f, 1.0f}; // Fallback normal
    }
    
    // Calculate d = -(ax + by + cz) using v0
    float d = -(normal.x * v0.x + normal.y * v0.y + normal.z * v0.z);
    
    // Store parameters: [a, b, c, d, 0, 0, 0, 0]
    params[0] = normal.x;
    params[1] = normal.y;
    params[2] = normal.z;
    params[3] = d;
    params[4] = 0.0f;
    params[5] = 0.0f;
    params[6] = 0.0f;
    params[7] = 0.0f;
}

static float DotProduct(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static bool AreNormalsSimilar(const Vec3& n1, const Vec3& n2, float angleThresholdDegrees) {
    float dot = DotProduct(n1, n2);
    dot = std::max(-1.0f, std::min(1.0f, dot)); // Clamp to [-1, 1]
    float angle = acos(dot) * 180.0f / M_PI;
    return angle < angleThresholdDegrees;
}

static bool IsPointOnPlane(const Vec3& point, const float params[8], float tolerance) {
    float a = params[0], b = params[1], c = params[2], d = params[3];
    float distance = fabs(a * point.x + b * point.y + c * point.z + d);
    return distance < tolerance;
}


// Surface Tessellator namespace
namespace SurfaceTessellator {
    
    class MeshTessellator {
    private:
        static void SetupTessellationGrid(const EnhancedMeshData& mesh, 
                                        int target_size,
                                        TessellatedSurface& surface, 
                                        bool verbose) {
            // Find mesh bounding box
            auto bbox = FindBoundingBox(mesh.vertices);
            Vec3 min_pos = bbox.first;
            Vec3 max_pos = bbox.second;
            
            Vec3 size = {max_pos.x - min_pos.x, max_pos.y - min_pos.y, max_pos.z - min_pos.z};
            
            // Determine grid size based on mesh proportions
            float aspect_ratio = size.x / size.y;
            
            if (aspect_ratio > 1.5f) {
                surface.grid_width = target_size;
                surface.grid_height = std::max(2, static_cast<int>(target_size / aspect_ratio));
            } else if (aspect_ratio < 0.67f) {
                surface.grid_width = std::max(2, static_cast<int>(target_size * aspect_ratio));
                surface.grid_height = target_size;
            } else {
                surface.grid_width = surface.grid_height = target_size;
            }
            
            surface.surface_origin = min_pos;
            surface.patch_size_x = size.x / surface.grid_width;
            surface.patch_size_y = size.y / surface.grid_height;
            
            // Initialize patches
            surface.patches.resize(surface.grid_width * surface.grid_height);
            
            // Project grid points onto the mesh surface
            for (int y = 0; y < surface.grid_height; ++y) {
                for (int x = 0; x < surface.grid_width; ++x) {
                    int patch_idx = y * surface.grid_width + x;
                    GridPatch& patch = surface.patches[patch_idx];
                    
                    patch.grid_x = x;
                    patch.grid_y = y;
                    patch.equation_type = EQUATION_BILINEAR;
                    patch.complexity_score = 0.0f;
                    patch.can_merge_right = patch.can_merge_down = false;
                    
                    // Calculate 4 corner positions for this grid cell
                    float u0 = static_cast<float>(x) / static_cast<float>(surface.grid_width);
                    float u1 = static_cast<float>(x + 1) / static_cast<float>(surface.grid_width);
                    float v0 = static_cast<float>(y) / static_cast<float>(surface.grid_height);
                    float v1 = static_cast<float>(y + 1) / static_cast<float>(surface.grid_height);
                    
                    float corner_uvs[4][2] = {{u0,v0}, {u1,v0}, {u1,v1}, {u0,v1}};
                    
                    patch.corner_positions.resize(4);
                    patch.corner_normals.resize(4);
                    
                    for (int c = 0; c < 4; ++c) {
                        Vec3 surface_point = ProjectUVToMeshSurface(corner_uvs[c][0], corner_uvs[c][1], mesh);
                        patch.corner_positions[c] = surface_point;
                        patch.corner_normals[c] = GetNormalAtPosition(surface_point, mesh);
                    }
                }
            }
            
            if (verbose) {
                std::cout << "Grid setup: " << surface.grid_width << "x" << surface.grid_height 
                          << " patches, each following mesh curvature\n";
            }
        }
        
        static Vec3 ProjectUVToMeshSurface(float u, float v, const EnhancedMeshData& mesh) {
            if (mesh.vertices.empty()) return {0, 0, 0};
            
            auto bbox = FindBoundingBox(mesh.vertices);
            Vec3 min_pos = bbox.first;
            Vec3 max_pos = bbox.second;
            
            float world_x = min_pos.x + u * (max_pos.x - min_pos.x);
            float world_y = min_pos.y + v * (max_pos.y - min_pos.y);
            
            std::vector<std::pair<float, int>> close_vertices;
            
            for (size_t i = 0; i < mesh.vertices.size(); ++i) {
                const Vec3& vertex = mesh.vertices[i];
                float xy_dist = sqrt(pow(vertex.x - world_x, 2) + pow(vertex.y - world_y, 2));
                if (xy_dist < (max_pos.x - min_pos.x) * 0.3f) {
                    close_vertices.push_back({xy_dist, static_cast<int>(i)});
                }
            }
            
            if (close_vertices.empty()) {
                int closest_idx = 0;
                float min_dist = Distance({world_x, world_y, 0}, mesh.vertices[0]);
                
                for (size_t i = 1; i < mesh.vertices.size(); ++i) {
                    float dist = Distance({world_x, world_y, 0}, mesh.vertices[i]);
                    if (dist < min_dist) {
                        min_dist = dist;
                        closest_idx = static_cast<int>(i);
                    }
                }
                
                return mesh.vertices[closest_idx];
            }
            
            std::sort(close_vertices.begin(), close_vertices.end());
            
            if (close_vertices.size() == 1) {
                return mesh.vertices[close_vertices[0].second];
            }
            
            Vec3 result = {world_x, world_y, 0};
            float total_weight = 0;
            float total_z = 0;
            
            int vertex_count = std::min(4, static_cast<int>(close_vertices.size()));
            
            for (int i = 0; i < vertex_count; ++i) {
                float dist = close_vertices[i].first;
                int vertex_idx = close_vertices[i].second;
                float weight = 1.0f / (dist + 0.001f);
                total_weight += weight;
                total_z += mesh.vertices[vertex_idx].z * weight;
            }
            
            if (total_weight > 0) {
                result.z = total_z / total_weight;
            } else {
                result.z = mesh.vertices[close_vertices[0].second].z;
            }
            
            return result;
        }
        
        static Vec3 GetNormalAtPosition(const Vec3& position, const EnhancedMeshData& mesh) {
            if (mesh.normals.empty() || mesh.vertices.empty()) return {0, 0, 1};
            
            std::vector<std::pair<float, int>> close_vertices;
            
            for (size_t i = 0; i < mesh.vertices.size(); ++i) {
                float dist = Distance(position, mesh.vertices[i]);
                close_vertices.push_back({dist, static_cast<int>(i)});
            }
            
            std::sort(close_vertices.begin(), close_vertices.end());
            
            Vec3 result_normal = {0, 0, 0};
            float total_weight = 0;
            
            int vertex_count = std::min(3, static_cast<int>(close_vertices.size()));
            
            for (int i = 0; i < vertex_count; ++i) {
                float dist = close_vertices[i].first;
                int vertex_idx = close_vertices[i].second;
                
                if (vertex_idx < static_cast<int>(mesh.normals.size())) {
                    float weight = 1.0f / (dist + 0.001f);
                    total_weight += weight;
                    
                    const Vec3& normal = mesh.normals[vertex_idx];
                    result_normal.x += normal.x * weight;
                    result_normal.y += normal.y * weight;
                    result_normal.z += normal.z * weight;
                }
            }
            
            if (total_weight > 0) {
                result_normal.x /= total_weight;
                result_normal.y /= total_weight;
                result_normal.z /= total_weight;
                NormalizeVector(result_normal);
            } else {
                result_normal = {0, 0, 1};
            }
            
            return result_normal;
        }
        
        static void AssignVertexesToPatches(const EnhancedMeshData& mesh, 
                                          TessellatedSurface& surface, 
                                          bool verbose) {
            int assigned_vertices = 0;
            
            for (auto& patch : surface.patches) {
                patch.mesh_vertices.clear();
            }
            
            for (size_t v_idx = 0; v_idx < mesh.vertices.size(); ++v_idx) {
                const Vec3& vertex = mesh.vertices[v_idx];
                int best_patch = -1;
                float min_distance = std::numeric_limits<float>::max();
                
                for (size_t p_idx = 0; p_idx < surface.patches.size(); ++p_idx) {
                    const GridPatch& patch = surface.patches[p_idx];
                    Vec3 patch_center = {0, 0, 0};
                    for (const Vec3& corner : patch.corner_positions) {
                        patch_center.x += corner.x;
                        patch_center.y += corner.y;
                        patch_center.z += corner.z;
                    }
                    patch_center.x /= 4.0f;
                    patch_center.y /= 4.0f;
                    patch_center.z /= 4.0f;
                    
                    float dist = Distance(vertex, patch_center);
                    if (dist < min_distance) {
                        min_distance = dist;
                        best_patch = static_cast<int>(p_idx);
                    }
                }
                
                if (best_patch >= 0) {
                    surface.patches[best_patch].mesh_vertices.push_back(static_cast<int>(v_idx));
                    assigned_vertices++;
                }
            }
            
            if (verbose) {
                std::cout << "Assigned " << assigned_vertices << " vertices to surface-conforming patches\n";
                int empty_patches = 0;
                for (const auto& patch : surface.patches) {
                    if (patch.mesh_vertices.empty()) empty_patches++;
                }
                if (empty_patches > 0) {
                    std::cout << "⚠️  " << empty_patches << " patches have no vertices\n";
                }
            }
        }
        
    public:
        static TessellatedSurface TessellateMesh(const EnhancedMeshData& mesh, int grid_size, bool verbose) {
            TessellatedSurface surface;
            
            if (verbose) {
                std::cout << "Starting mesh tessellation with grid size " << grid_size << "\n";
            }
            
            SetupTessellationGrid(mesh, grid_size, surface, verbose);
            AssignVertexesToPatches(mesh, surface, verbose);
            
            std::set<std::tuple<int,int,int>> unique_anchors;
            surface.grid_anchors.clear();
            
            for (const auto& patch : surface.patches) {
                for (size_t corner = 0; corner < patch.corner_positions.size(); ++corner) {
                    int anchor_x = patch.grid_x + (corner == 1 || corner == 2 ? 1 : 0);
                    int anchor_y = patch.grid_y + (corner == 2 || corner == 3 ? 1 : 0);
                    std::tuple<int,int,int> anchor_key = std::make_tuple(anchor_x, anchor_y, static_cast<int>(corner));
                    
                    if (unique_anchors.find(anchor_key) == unique_anchors.end()) {
                        unique_anchors.insert(anchor_key);
                        surface.grid_anchors.push_back(patch.corner_positions[corner]);
                    }
                }
            }
            
            if (verbose) {
                std::cout << "Generated " << surface.grid_anchors.size() << " unique grid anchors\n";
                std::cout << "Created " << surface.patches.size() << " tessellated patches\n";
            }
            
            return surface;
        }
    };
}

// Smart Mesh Converter
class SmartMeshConverter {
public:
    static ProceduralObjectCollection ConvertMeshToProcedural(const EnhancedMeshData& meshData, bool verbose) {
        ProceduralObjectCollection collection;
        collection.object_name = meshData.name + "_exact_conversion";
        collection.creator_info = "Exact Face-Preserving Converter";

        std::cout << "\n=== EXACT FACE-PRESERVING CONVERSION ===\n";
        std::cout << "Input mesh: " << meshData.vertices.size() << " vertices, " 
                  << meshData.faces.size() << " faces\n";

        if (meshData.vertices.empty() || meshData.faces.empty()) {
            std::cout << "❌ No vertices or faces to convert\n";
            return collection;
        }

        auto bbox = FindBoundingBox(meshData.vertices);
        Vec3 min_pos = bbox.first;
        Vec3 max_pos = bbox.second;

        for (size_t i = 0; i < meshData.vertices.size(); ++i) {
            const Vec3& vertex = meshData.vertices[i];
            AnchorPointBuffer anchor;
            memset(&anchor, 0, sizeof(anchor));

            Vec3 psf_coords = WorldToPSF(vertex, min_pos, max_pos);
            anchor.pos_xyz[0] = static_cast<uint16_t>(std::max(0.0f, std::min(4095.0f, (psf_coords.x + 2.0f) / 4.0f * 4095.0f)));
            anchor.pos_xyz[1] = static_cast<uint16_t>(std::max(0.0f, std::min(4095.0f, (psf_coords.y + 2.0f) / 4.0f * 4095.0f)));
            anchor.pos_xyz[2] = static_cast<uint16_t>(std::max(0.0f, std::min(4095.0f, (psf_coords.z + 2.0f) / 4.0f * 4095.0f)));

            if (i < meshData.normals.size()) {
                Vec3 normal = meshData.normals[i];
                anchor.normal[0] = static_cast<uint8_t>((normal.x * 0.5f + 0.5f) * 255.0f);
                anchor.normal[1] = static_cast<uint8_t>((normal.y * 0.5f + 0.5f) * 255.0f);
                anchor.normal[2] = static_cast<uint8_t>((normal.z * 0.5f + 0.5f) * 255.0f);
            } else {
                anchor.normal[0] = anchor.normal[1] = 127;
                anchor.normal[2] = 255;
            }

            anchor.color[0] = anchor.color[1] = anchor.color[2] = 255;
            anchor.roughness = 128;
            anchor.metallic = 0;

            collection.AddAnchor(anchor);
        }

        if (verbose) {
            std::cout << "Created " << collection.anchor_pool.size() << " anchors from vertices\n";
        }

        for (size_t i = 0; i < meshData.faces.size(); ++i) {
            const auto& face = meshData.faces[i];
            uint16_t anchor_indices[4] = {0, 0, 0, 0};

            if (face.size() >= 3) {
                anchor_indices[0] = static_cast<uint16_t>(face[0]);
                anchor_indices[1] = static_cast<uint16_t>(face[1]);
                anchor_indices[2] = static_cast<uint16_t>(face[2]);
                if (face.size() == 4) {
                    anchor_indices[3] = static_cast<uint16_t>(face[3]);
                } else {
                    anchor_indices[3] = anchor_indices[2];
                }

                for (int j = 0; j < 4; ++j) {
                    if (anchor_indices[j] >= collection.anchor_pool.size()) {
                        std::cout << "⚠️ Invalid anchor index for face " << i << ": " << anchor_indices[j] << "\n";
                        anchor_indices[j] = 0;
                    }
                }

                float params[8] = {0};
                if (face.size() >= 3) {
                    Vec3 v0 = meshData.vertices[face[0]];
                    Vec3 v1 = meshData.vertices[face[1]];
                    Vec3 v2 = meshData.vertices[face[2]];
                    CalculatePlaneParameters(v0, v1, v2, params);
                }

                ProceduralSurfaceBuffer surface = SurfaceBufferUtils::CreateWithAnchors(
                    EQUATION_PLANE, anchor_indices, params, MOD_NONE);

                collection.AddSurface(surface);
            }
        }

        if (verbose) {
            std::cout << "Created " << collection.surface_pool.size() << " planar surfaces from faces\n";
        }

        collection.UpdateHeader();

        std::cout << "=== EXACT CONVERSION COMPLETE ===\n";
        std::cout << "Result: " << collection.anchor_pool.size() << " anchors, " 
                  << collection.surface_pool.size() << " planar surfaces\n";

        return collection;
    }
    
private:
    static int DetermineOptimalGridSize(const EnhancedMeshData& meshData, bool verbose) {
        int vertex_count = static_cast<int>(meshData.vertices.size());
        int grid_size;
        if (vertex_count < 100) {
            grid_size = 4;
        } else if (vertex_count < 500) {
            grid_size = 6;
        } else if (vertex_count < 2000) {
            grid_size = 8;
        } else {
            grid_size = 12;
        }
        
        if (verbose) {
            std::cout << "Mesh complexity: " << vertex_count << " vertices\n";
            std::cout << "Target grid size: " << grid_size << "x" << grid_size << "\n";
        }
        
        return grid_size;
    }
    
    static void CreatePSFAnchorsFromGrid(ProceduralObjectCollection& collection,
                                       const TessellatedSurface& tessellated,
                                       bool verbose) {
        std::cout << "Creating PSF anchors from grid points...\n";
        
        auto bbox = FindBoundingBox(tessellated.grid_anchors);
        Vec3 min_pos = bbox.first;
        Vec3 max_pos = bbox.second;
        
        for (size_t i = 0; i < tessellated.grid_anchors.size(); ++i) {
            const Vec3& grid_pos = tessellated.grid_anchors[i];
            
            AnchorPointBuffer anchor;
            memset(&anchor, 0, sizeof(anchor));
            
            Vec3 psf_coords = WorldToPSF(grid_pos, min_pos, max_pos);
            anchor.pos_xyz[0] = static_cast<uint16_t>(std::max(0.0f, std::min(4095.0f, (psf_coords.x + 2.0f) / 4.0f * 4095.0f)));
            anchor.pos_xyz[1] = static_cast<uint16_t>(std::max(0.0f, std::min(4095.0f, (psf_coords.y + 2.0f) / 4.0f * 4095.0f)));
            anchor.pos_xyz[2] = static_cast<uint16_t>(std::max(0.0f, std::min(4095.0f, (psf_coords.z + 2.0f) / 4.0f * 4095.0f)));
            
            anchor.normal[0] = static_cast<uint8_t>(127);
            anchor.normal[1] = static_cast<uint8_t>(127);
            anchor.normal[2] = static_cast<uint8_t>(255);
            
            anchor.color[0] = anchor.color[1] = anchor.color[2] = 255;
            anchor.roughness = 128;
            anchor.metallic = 0;
            
            collection.AddAnchor(anchor);
        }
        
        if (verbose) {
            std::cout << "Created " << collection.anchor_pool.size() << " grid anchors\n";
        }
    }
    
    static void CreatePSFSurfacesFromGrid(ProceduralObjectCollection& collection,
                                        const TessellatedSurface& tessellated,
                                        bool verbose) {
        std::cout << "Creating PSF surfaces from grid patches...\n";
        
        int anchor_cols = tessellated.grid_width + 1;
        
        for (const auto& patch : tessellated.patches) {
            uint16_t patch_anchors[4];
            patch_anchors[0] = static_cast<uint16_t>(patch.grid_y * anchor_cols + patch.grid_x);
            patch_anchors[1] = static_cast<uint16_t>(patch.grid_y * anchor_cols + patch.grid_x + 1);
            patch_anchors[2] = static_cast<uint16_t>((patch.grid_y + 1) * anchor_cols + patch.grid_x + 1);
            patch_anchors[3] = static_cast<uint16_t>((patch.grid_y + 1) * anchor_cols + patch.grid_x);
            
            for (int i = 0; i < 4; ++i) {
                if (patch_anchors[i] >= collection.anchor_pool.size()) {
                    std::cout << "❌ Invalid anchor index: " << patch_anchors[i] << "\n";
                    patch_anchors[i] = 0;
                }
            }
            
            float params[8] = {0};
            params[0] = patch.complexity_score;
            
            ProceduralSurfaceBuffer surface = SurfaceBufferUtils::CreateWithAnchors(
                patch.equation_type, patch_anchors, params, MOD_NONE);
            
            collection.AddSurface(surface);
        }
        
        if (verbose) {
            std::cout << "Created " << collection.surface_pool.size() << " tessellated surfaces\n";
        }
    }
    
    static Vec3 WorldToPSF(const Vec3& world_pos, const Vec3& min_pos, const Vec3& max_pos) {
        Vec3 size = {max_pos.x - min_pos.x, max_pos.y - min_pos.y, max_pos.z - min_pos.z};
        float max_dimension = std::max({size.x, size.y, size.z});
        
        if (max_dimension < 1e-6f) {
            return {0, 0, 0};
        }
        
        Vec3 center = {
            (min_pos.x + max_pos.x) * 0.5f,
            (min_pos.y + max_pos.y) * 0.5f,
            (min_pos.z + max_pos.z) * 0.5f
        };
        
        Vec3 normalized = {
            ((world_pos.x - center.x) / max_dimension) * 3.0f,
            ((world_pos.y - center.y) / max_dimension) * 3.0f,
            ((world_pos.z - center.z) / max_dimension) * 3.0f
        };
        
        normalized.x = std::max(-1.8f, std::min(1.8f, normalized.x));
        normalized.y = std::max(-1.8f, std::min(1.8f, normalized.y));
        normalized.z = std::max(-1.8f, std::min(1.8f, normalized.z));
        
        return normalized;
    }
};

// FBX Converter namespace
namespace MeshAwareFBXConverter {

#ifndef NO_FBX_SDK
    static void ExtractEnhancedMeshFromNode(FbxNode* node, std::vector<EnhancedMeshData>& meshes, bool verbose) {
        if (!node) return;

        FbxMesh* mesh = node->GetMesh();
        if (mesh) {
            EnhancedMeshData meshData;
            meshData.name = node->GetName() ? node->GetName() : "UnnamedMesh";

            FbxAMatrix globalTransform = node->EvaluateGlobalTransform();
            FbxVector4* controlPoints = mesh->GetControlPoints();
            int controlPointsCount = mesh->GetControlPointsCount();
            meshData.vertices.reserve(controlPointsCount);

            for (int i = 0; i < controlPointsCount; ++i) {
                FbxVector4 vertex = controlPoints[i];
                vertex = globalTransform.MultT(vertex);
                meshData.vertices.push_back(Vec3{
                    static_cast<float>(vertex[0]),
                    static_cast<float>(vertex[1]),
                    static_cast<float>(vertex[2])
                });
            }

            meshData.normals.resize(controlPointsCount, Vec3{0, 0, 1});
            FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
            if (normalElement) {
                int polygonCount = mesh->GetPolygonCount();
                std::vector<int> vertex_normal_count(controlPointsCount, 0);
                std::vector<Vec3> vertex_normal_sum(controlPointsCount, Vec3{0, 0, 0});
                
                for (int polyIndex = 0; polyIndex < polygonCount; ++polyIndex) {
                    int polySize = mesh->GetPolygonSize(polyIndex);
                    for (int vertIndex = 0; vertIndex < polySize; ++vertIndex) {
                        int vertexIndex = mesh->GetPolygonVertex(polyIndex, vertIndex);
                        if (vertexIndex < controlPointsCount) {
                            FbxVector4 normal;
                            if (normalElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
                                int normalIndex = polyIndex * polySize + vertIndex;
                                normal = normalElement->GetDirectArray().GetAt(normalIndex);
                            } else if (normalElement->GetMappingMode() == FbxGeometryElement::eByControlPoint) {
                                normal = normalElement->GetDirectArray().GetAt(vertexIndex);
                            }
                            
                            normal = globalTransform.MultT(normal);
                            normal.Normalize();
                            
                            vertex_normal_sum[vertexIndex].x += static_cast<float>(normal[0]);
                            vertex_normal_sum[vertexIndex].y += static_cast<float>(normal[1]);
                            vertex_normal_sum[vertexIndex].z += static_cast<float>(normal[2]);
                            vertex_normal_count[vertexIndex]++;
                        }
                    }
                }
                
                for (int i = 0; i < controlPointsCount; ++i) {
                    if (vertex_normal_count[i] > 0) {
                        meshData.normals[i].x = vertex_normal_sum[i].x / vertex_normal_count[i];
                        meshData.normals[i].y = vertex_normal_sum[i].y / vertex_normal_count[i];
                        meshData.normals[i].z = vertex_normal_sum[i].z / vertex_normal_count[i];
                        NormalizeVector(meshData.normals[i]);
                    }
                }
            }

            int polygonCount = mesh->GetPolygonCount();
            meshData.faces.reserve(polygonCount);
            
            for (int polyIndex = 0; polyIndex < polygonCount; ++polyIndex) {
                int polySize = mesh->GetPolygonSize(polyIndex);
                std::vector<int> face_vertices;
                face_vertices.reserve(polySize);
                
                for (int vertIndex = 0; vertIndex < polySize; ++vertIndex) {
                    int vertexIndex = mesh->GetPolygonVertex(polyIndex, vertIndex);
                    if (vertexIndex >= 0 && vertexIndex < controlPointsCount) {
                        face_vertices.push_back(vertexIndex);
                    }
                }
                
                if (face_vertices.size() >= 3) {
                    meshData.faces.push_back(face_vertices);
                }
            }

            if (verbose) {
                std::cout << "Extracted enhanced mesh: " << meshData.name << "\n";
                std::cout << "  Vertices: " << meshData.vertices.size() << "\n";
                std::cout << "  Normals: " << meshData.normals.size() << "\n";
                std::cout << "  Faces: " << meshData.faces.size() << "\n";
            }

            if (!meshData.vertices.empty() && !meshData.faces.empty()) {
                meshes.push_back(meshData);
            }
        }

        for (int i = 0; i < node->GetChildCount(); ++i) {
            ExtractEnhancedMeshFromNode(node->GetChild(i), meshes, verbose);
        }
    }

    bool ConvertFBXToPSFEnhanced(const std::string& fbx_file, const std::string& psf_file, bool verbose) {
        std::cout << "=== MESH-AWARE FBX TO PSF CONVERTER ===\n";
        std::cout << "Strategy: Preserve exact face count with planar surfaces\n";
        std::cout << "Input:  " << fbx_file << "\n";
        std::cout << "Output: " << psf_file << "\n";
        
        FbxManager* manager = nullptr;
        FbxIOSettings* settings = nullptr;
        
        std::cout << "Initializing FBX SDK...\n";
        manager = FbxManager::Create();
        if (!manager) {
            std::cout << "❌ Failed to create FBX manager\n";
            return false;
        }
        
        settings = FbxIOSettings::Create(manager, IOSROOT);
        if (!settings) {
            std::cout << "❌ Failed to create FBX IO settings\n";
            manager->Destroy();
            return false;
        }
        
        FbxImporter* importer = FbxImporter::Create(manager, "");
        if (!importer->Initialize(fbx_file.c_str(), -1, manager->GetIOSettings())) {
            std::cout << "❌ Failed to initialize FBX importer for file: " << fbx_file << "\n";
            manager->Destroy();
            return false;
        }
        
        FbxScene* scene = FbxScene::Create(manager, "ImportedScene");
        if (!importer->Import(scene)) {
            std::cout << "❌ Failed to import FBX scene\n";
            importer->Destroy();
            manager->Destroy();
            return false;
        }
        
        importer->Destroy();
        
        FbxGeometryConverter geometryConverter(manager);
        geometryConverter.Triangulate(scene, true);
        
        std::vector<EnhancedMeshData> meshes;
        FbxNode* rootNode = scene->GetRootNode();
        if (rootNode) {
            std::cout << "Extracting enhanced meshes from FBX...\n";
            ExtractEnhancedMeshFromNode(rootNode, meshes, verbose);
        }
        
        if (meshes.empty()) {
            std::cout << "❌ No meshes found\n";
            manager->Destroy();
            return false;
        }
        
        std::cout << "Found " << meshes.size() << " mesh(es)\n";
        
        ProceduralObjectCollection collection = SmartMeshConverter::ConvertMeshToProcedural(meshes[0], verbose);
        
        if (verbose) {
            std::cout << "\n=== BASIC CONVERSION DEBUG ===\n";
            std::cout << "Created " << collection.anchor_pool.size() << " anchors\n";
            std::cout << "Created " << collection.surface_pool.size() << " surfaces\n";
            
            for (size_t i = 0; i < std::min(collection.anchor_pool.size(), size_t(5)); ++i) {
                Vec3 pos = AnchorPointEvaluator::DecodePosition(collection.anchor_pool[i].pos_xyz);
                std::cout << "  Anchor " << i << ": (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
            }
            
            for (size_t i = 0; i < std::min(collection.surface_pool.size(), size_t(3)); ++i) {
                const auto& surface = collection.surface_pool[i];
                std::cout << "  Surface " << i << " anchors: [" 
                          << surface.anchor_point_indices[0] << "," << surface.anchor_point_indices[1] << ","
                          << surface.anchor_point_indices[2] << "," << surface.anchor_point_indices[3] << "]\n";
                std::cout << "  Surface " << i << " type: " << static_cast<int>(SurfaceBufferUtils::ExtractBaseType(&surface)) << "\n";
                std::vector<float> params = SurfaceBufferUtils::ExtractParameters(&surface);
                std::cout << "  Surface " << i << " params: [";
                for (float p : params) std::cout << p << ",";
                std::cout << "]\n";
            }
        }
        
        collection.UpdateHeader();
        
        if (!collection.SaveToFile(psf_file)) {
            std::cout << "❌ Failed to save PSF file\n";
            manager->Destroy();
            return false;
        }
        
        std::cout << "✅ Exact face-preserving conversion successful!\n";
        std::cout << "Result: " << collection.anchor_pool.size() << " anchors\n";
        std::cout << "Result: " << collection.surface_pool.size() << " planar surfaces\n";
        
        manager->Destroy();
        return true;
#else
        std::cout << "❌ FBX SDK not available. Cannot convert FBX files.\n";
        return false;
#endif
    }
}

// Args structure for command line parsing
struct Args {
    std::string input_file;
    std::string output_file;
    bool verbose = false;
    bool test_mode = false;
    bool convert_mode = false;
    bool simplify_mode = false;
    
    bool Parse(int argc, char* argv[]) {
        if (argc < 2) {
            std::cout << "Procedural Surface Engine - Converter\n";
            std::cout << "Usage:\n";
            std::cout << "  Test mode:    " << argv[0] << " --test [-v] [-o output.psf]\n";
            std::cout << "  Convert PSF:  " << argv[0] << " --convert input.psf [-o output.obj] [-v]\n";
            std::cout << "  Simplify PSF: " << argv[0] << " --simplify input.psf [-o output.psf] [-v]\n";
            std::cout << "  Import FBX:   " << argv[0] << " input.fbx [-o output.psf] [-v]\n";
            std::cout << "\nOptions:\n";
            std::cout << "  -v            Verbose output\n";
            std::cout << "  -o <file>     Output file\n";
            std::cout << "  --test        Create test object\n";
            std::cout << "  --convert     Convert PSF to OBJ\n";
            std::cout << "  --simplify    Simplify PSF by combining surfaces\n";
#ifdef NO_FBX_SDK
            std::cout << "\nNote: FBX SDK not available. Only --test, --convert, and --simplify modes supported.\n";
#endif
            return false;
        }
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-v") {
                verbose = true;
            } else if (arg == "-o" && i + 1 < argc) {
                output_file = argv[++i];
            } else if (arg == "--test") {
                test_mode = true;
                input_file = "test";
            } else if (arg == "--convert") {
                convert_mode = true;
            } else if (arg == "--simplify") {
                simplify_mode = true;
            } else if (input_file.empty()) {
                input_file = arg;
            }
        }
        
        if (output_file.empty()) {
            if (test_mode) {
                output_file = "test_object.psf";
            } else if (convert_mode) {
                size_t dot_pos = input_file.find_last_of('.');
                if (dot_pos != std::string::npos) {
                    output_file = input_file.substr(0, dot_pos) + ".obj";
                } else {
                    output_file = input_file + ".obj";
                }
            } else if (simplify_mode) {
                size_t dot_pos = input_file.find_last_of('.');
                if (dot_pos != std::string::npos) {
                    output_file = input_file.substr(0, dot_pos) + "_simplified.psf";
                } else {
                    output_file = input_file + "_simplified.psf";
                }
            } else {
                size_t dot_pos = input_file.find_last_of('.');
                if (dot_pos != std::string::npos) {
                    output_file = input_file.substr(0, dot_pos) + ".psf";
                } else {
                    output_file = input_file + ".psf";
                }
            }
        }
        
        return !input_file.empty();
    }
};

// Helper functions
bool ExportToOBJ(const std::string& filename, const std::vector<float>& vertices, 
                 const std::vector<uint32_t>& indices, const std::vector<float>& normals) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cout << "❌ Failed to open OBJ file: " << filename << "\n";
        return false;
    }
    
    std::string mtl_file = filename.substr(0, filename.find_last_of('.')) + ".mtl";
    
    file << "# Procedural Surface - Exact Face-Preserving Conversion\n";
    file << "mtllib " << mtl_file << "\n";
    file << "s 1\n\n";
    
    for (size_t i = 0; i < vertices.size(); i += 3) {
        file << "v " << vertices[i] << " " << vertices[i+1] << " " << vertices[i+2] << "\n";
    }
    
    for (size_t i = 0; i < normals.size(); i += 3) {
        file << "vn " << normals[i] << " " << normals[i+1] << " " << normals[i+2] << "\n";
    }
    
    file << "usemtl MeshAwareMaterial\n";
    file << "# Faces (triangulated)\n";
    
    for (size_t i = 0; i < indices.size(); i += 4) {
        if (i + 3 < indices.size()) {
            uint32_t v1 = indices[i] + 1;
            uint32_t v2 = indices[i+1] + 1;
            uint32_t v3 = indices[i+2] + 1;
            uint32_t v4 = indices[i+3] + 1;
            
            file << "f " << v1 << "//" << v1 << " " << v2 << "//" << v2 << " " << v3 << "//" << v3 << "\n";
            file << "f " << v1 << "//" << v1 << " " << v3 << "//" << v3 << " " << v4 << "//" << v4 << "\n";
        }
    }
    
    file.close();
    
    std::ofstream mtl(mtl_file);
    if (mtl.is_open()) {
        mtl << "# Mesh-Aware Material\n";
        mtl << "newmtl MeshAwareMaterial\n";
        mtl << "Ka 0.2 0.2 0.2\n";
        mtl << "Kd 0.6 0.8 0.7\n";
        mtl << "Ks 0.4 0.4 0.4\n";
        mtl << "Ns 50\n";
        mtl << "illum 2\n";
        mtl.close();
    }
    
    std::cout << "✅ Exported exact face-preserving OBJ: " << filename << "\n";
    return true;
}

ProceduralObjectCollection CreateTestObject() {
    ProceduralObjectCollection collection;
    collection.object_name = "test_patch";
    collection.creator_info = "Test Generator";
    
    std::vector<Vec3> positions = {
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.1f},
        {-1.0f,  1.0f, 0.1f}
    };
    
    for (size_t i = 0; i < positions.size(); ++i) {
        AnchorPointBuffer anchor;
        memset(&anchor, 0, sizeof(anchor));
        
        anchor.pos_xyz[0] = static_cast<uint16_t>((positions[i].x + 2.0f) / 4.0f * 4095.0f);
        anchor.pos_xyz[1] = static_cast<uint16_t>((positions[i].y + 2.0f) / 4.0f * 4095.0f);
        anchor.pos_xyz[2] = static_cast<uint16_t>((positions[i].z + 2.0f) / 4.0f * 4095.0f);
        
        anchor.normal[0] = static_cast<uint8_t>(127);
        anchor.normal[1] = static_cast<uint8_t>(127);
        anchor.normal[2] = static_cast<uint8_t>(255);
        
        anchor.color[0] = anchor.color[1] = anchor.color[2] = 255;
        anchor.roughness = 128;
        anchor.metallic = 0;
        
        collection.AddAnchor(anchor);
    }
    
    uint16_t anchor_indices[4] = {0, 1, 2, 3};
    float params[8] = {0};
    CalculatePlaneParameters(positions[0], positions[1], positions[2], params);
    
    ProceduralSurfaceBuffer surface = SurfaceBufferUtils::CreateWithAnchors(
        EQUATION_PLANE, anchor_indices, params, MOD_NONE);
    
    collection.AddSurface(surface);
    
    collection.UpdateHeader();
    return collection;
}

bool VerifyPSF(const std::string& psf_file, bool verbose) {
    std::cout << "Verifying PSF file: " << psf_file << "\n";
    
    ProceduralObjectCollection collection;
    if (!collection.LoadFromFile(psf_file)) {
        std::cout << "❌ Failed to load PSF file\n";
        return false;
    }
    
    std::cout << "✅ PSF file loaded successfully\n";
    
    if (verbose) {
        std::cout << "Magic: 0x" << std::hex << collection.header.magic << std::dec;
        if (collection.header.magic == PSF_MAGIC) {
            std::cout << " ✅\n";
        } else {
            std::cout << " ❌\n";
        }
        
        std::cout << "Version: " << collection.header.version << "\n";
        std::cout << "Objects: " << collection.header.object_count << "\n";
        std::cout << "Anchors: " << collection.header.anchor_pool_size << "\n";
        std::cout << "Surfaces: " << collection.header.surface_pool_size << "\n";
        std::cout << "Bones: " << collection.header.bone_pool_size << "\n";
        std::cout << "File size: " << collection.CalculateFileSize() << " bytes\n";
    }
    
    return true;
}

bool ConvertPSFToOBJ(const std::string& psf_file, const std::string& obj_file, bool verbose, int resolution = 4) {
    std::cout << "Converting PSF to OBJ...\n";
    std::cout << "Input:  " << psf_file << "\n";
    std::cout << "Output: " << obj_file << "\n";
    std::cout << "Resolution: " << resolution << "\n";
    
    ProceduralObjectCollection loaded_collection;
    if (!loaded_collection.LoadFromFile(psf_file)) {
        std::cout << "❌ Error: Failed to load PSF file: " << psf_file << "\n";
        return false;
    }
    
    if (verbose) {
        std::cout << "✅ PSF loaded successfully\n";
        std::cout << "   Objects: " << loaded_collection.object_descriptors.size() << "\n";
        std::cout << "   Anchors: " << loaded_collection.anchor_pool.size() << "\n";
        std::cout << "   Surfaces: " << loaded_collection.surface_pool.size() << "\n";
        std::cout << "   Bones: " << loaded_collection.bone_pool.size() << "\n";
    }
    
    if (loaded_collection.surface_pool.empty()) {
        std::cout << "❌ Error: No surfaces found in PSF file\n";
        return false;
    }
    
    std::vector<float> combined_vertices, combined_normals;
    std::vector<uint32_t> combined_indices;
    
    for (size_t i = 0; i < loaded_collection.surface_pool.size(); ++i) {
        std::vector<float> surface_vertices, surface_normals;
        std::vector<uint32_t> surface_indices;
        
        try {
            std::cout << "Evaluating surface " << i << " (type: " 
                      << static_cast<int>(SurfaceBufferUtils::ExtractBaseType(&loaded_collection.surface_pool[i])) 
                      << ")\n";
            SurfaceBufferEvaluator::EvaluateSurfaceMesh(
                &loaded_collection.surface_pool[i],
                loaded_collection.anchor_pool,
                {},
                resolution,
                surface_vertices,
                surface_indices,
                surface_normals,
                0.0f
            );
            
            if (surface_vertices.empty() || surface_indices.empty()) {
                std::cout << "⚠️ Warning: Surface " << i << " generated no geometry\n";
                continue;
            }
            
            if (verbose) {
                std::cout << "  Surface " << i << ": " << surface_vertices.size()/3 << " vertices, " 
                          << surface_indices.size()/4 << " faces\n";
            }
            
            uint32_t vertex_offset = static_cast<uint32_t>(combined_vertices.size() / 3);
            for (uint32_t& index : surface_indices) {
                index += vertex_offset;
            }
            
            combined_vertices.insert(combined_vertices.end(), surface_vertices.begin(), surface_vertices.end());
            combined_normals.insert(combined_normals.end(), surface_normals.begin(), surface_normals.end());
            combined_indices.insert(combined_indices.end(), surface_indices.begin(), surface_indices.end());
            
        } catch (const std::exception& e) {
            std::cout << "⚠️ Warning: Failed to evaluate surface " << i << ": " << e.what() << "\n";
            continue;
        }
    }
    
    if (combined_vertices.empty() || combined_indices.empty()) {
        std::cout << "❌ Error: No geometry generated from PSF file\n";
        return false;
    }
    
    std::cout << "Generated geometry: " << combined_vertices.size()/3 << " vertices, " 
              << combined_indices.size()/4 << " faces\n";
    
    if (!ExportToOBJ(obj_file, combined_vertices, combined_indices, combined_normals)) {
        std::cout << "❌ Error: Failed to write OBJ file: " << obj_file << "\n";
        return false;
    }
    
    std::ifstream check_file(obj_file, std::ios::ate);
    if (check_file.is_open()) {
        auto file_size = check_file.tellg();
        std::cout << "   Size: " << file_size << " bytes\n";
        check_file.close();
    }
    
    std::cout << "✅ Successfully converted PSF to OBJ!\n";
    return true;
}

void DebugPSF(const ProceduralObjectCollection& collection, bool verbose) {
    if (!verbose) return;

    std::cout << "\n=== PSF DEBUG ===\n";
    std::cout << "Anchors: " << collection.anchor_pool.size() << "\n";
    for (size_t i = 0; i < std::min(collection.anchor_pool.size(), size_t(5)); ++i) {
        Vec3 pos = AnchorPointEvaluator::DecodePosition(collection.anchor_pool[i].pos_xyz);
        std::cout << "  Anchor " << i << ": pos=(" << pos.x << ", " << pos.y << ", " << pos.z << "), "
                  << "normal=(" << (int)collection.anchor_pool[i].normal[0] << ", "
                  << (int)collection.anchor_pool[i].normal[1] << ", "
                  << (int)collection.anchor_pool[i].normal[2] << ")\n";
    }

    std::cout << "Surfaces: " << collection.surface_pool.size() << "\n";
    for (size_t i = 0; i < std::min(collection.surface_pool.size(), size_t(5)); ++i) {
        const auto& surface = collection.surface_pool[i];
        std::cout << "  Surface " << i << ": type=" << static_cast<int>(SurfaceBufferUtils::ExtractBaseType(&surface))
                  << ", anchors=[" << surface.anchor_point_indices[0] << ", "
                  << surface.anchor_point_indices[1] << ", "
                  << surface.anchor_point_indices[2] << ", "
                  << surface.anchor_point_indices[3] << "], "
                  << "params=[";
        auto params = SurfaceBufferUtils::ExtractParameters(&surface);
        for (float p : params) std::cout << p << ", ";
        std::cout << "], combo=" << static_cast<int>(SurfaceBufferUtils::ExtractCombination(&surface))
                  << ", secondary_type=" << (int)surface.secondary_surface_type << "\n";
    }
}

ProceduralObjectCollection SimplifyPSF(const ProceduralObjectCollection& input_collection, bool verbose, float angleThresholdDegrees = 5.0f, float planeTolerance = 0.01f) {
    ProceduralObjectCollection simplified_collection = input_collection;
    simplified_collection.surface_pool.clear();
    
    std::map<std::pair<uint16_t, uint16_t>, std::vector<size_t>> edge_to_surfaces;
    for (size_t i = 0; i < input_collection.surface_pool.size(); ++i) {
        const auto& surface = input_collection.surface_pool[i];
        for (int j = 0; j < 4; ++j) {
            int k = (j + 1) % 4;
            auto edge = std::minmax(surface.anchor_point_indices[j], surface.anchor_point_indices[k]);
            edge_to_surfaces[edge].push_back(i);
        }
    }

    std::vector<bool> used(input_collection.surface_pool.size(), false);
    std::vector<std::vector<size_t>> surface_groups;

    for (size_t i = 0; i < input_collection.surface_pool.size(); ++i) {
        if (used[i]) continue;
        
        std::vector<size_t> group = {i};
        used[i] = true;
        
        auto params = SurfaceBufferUtils::ExtractParameters(&input_collection.surface_pool[i]);
        Vec3 normal = {params[0], params[1], params[2]};

        std::vector<size_t> to_check = {i};
        while (!to_check.empty()) {
            size_t curr = to_check.back();
            to_check.std::vector<size_t>::pop_back();;
            
            for (int j = 0; j < 4; ++j) {
                int k = (j + 1) % 4;
                auto edge = std::minmax(input_collection.surface_pool[curr].anchor_point_indices[j],
                                       input_collection.surface_pool[curr].anchor_point_indices[k]);
                for (size_t neighbor : edge_to_surfaces[edge]) {
                    if (!used[neighbor]) {
                        auto neighbor_params = SurfaceBufferUtils::ExtractParameters(&input_collection.surface_pool[neighbor]);
                        Vec3 neighbor_normal = {neighbor_params[0], neighbor_params[1], neighbor_params[2]};
                        if (AreNormalsSimilar(normal, neighbor_normal, angleThresholdDegrees)) {
                            bool coplanar = true;
                            for (int a = 0; a < 4 && coplanar; ++a) {
                                uint16_t anchor_idx = input_collection.surface_pool[neighbor].anchor_point_indices[a];
                                if (anchor_idx < input_collection.anchor_pool.size()) {
                                    Vec3 pos = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_idx].pos_xyz);
                                    if (!IsPointOnPlane(pos, params.data(), planeTolerance)) {
                                        coplanar = false;
                                    }
                                }
                            }
                            if (coplanar) {
                                group.push_back(neighbor);
                                used[neighbor] = true;
                                to_check.push_back(neighbor);
                            }
                        }
                    }
                }
            }
        }
        
        if (!group.empty()) {
            surface_groups.push_back(group);
        }
    }

    if (verbose) {
        std::cout << "Found " << surface_groups.size() << " coplanar surface groups\n";
    }

    for (const auto& group : surface_groups) {
        std::set<uint16_t> group_anchors;
        std::vector<Vec3> group_positions;
        for (size_t idx : group) {
            const auto& surface = input_collection.surface_pool[idx];
            for (int i = 0; i < 4; ++i) {
                group_anchors.insert(surface.anchor_point_indices[i]);
            }
        }

        for (uint16_t anchor_idx : group_anchors) {
            if (anchor_idx < input_collection.anchor_pool.size()) {
                group_positions.push_back(AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_idx].pos_xyz));
            }
        }

        auto bbox = FindBoundingBox(group_positions);
        Vec3 min_pos = bbox.first;
        Vec3 max_pos = bbox.second;

        std::vector<std::pair<float, uint16_t>> anchor_distances;
        for (uint16_t anchor_idx : group_anchors) {
            Vec3 pos = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_idx].pos_xyz);
            float dist = Distance(pos, min_pos);
            anchor_distances.push_back({dist, anchor_idx});
        }
        std::sort(anchor_distances.begin(), anchor_distances.end());

        uint16_t anchor_indices[4] = {0, 0, 0, 0};
        if (anchor_distances.size() >= 4) {
            anchor_indices[0] = anchor_distances[0].second;
            anchor_indices[1] = anchor_distances[1].second;
            anchor_indices[2] = anchor_distances[anchor_distances.size() - 2].second;
            anchor_indices[3] = anchor_distances[anchor_distances.size() - 1].second;
        } else {
            size_t i = 0;
            for (uint16_t idx : group_anchors) {
                anchor_indices[i++] = idx;
                if (i >= 4) break;
            }
            while (i < 4) anchor_indices[i++] = anchor_indices[0];
        }

        float params[8] = {0};
        Vec3 v0 = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_indices[0]].pos_xyz);
        Vec3 v1 = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_indices[1]].pos_xyz);
        Vec3 v2 = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_indices[2]].pos_xyz);
        CalculatePlaneParameters(v0, v1, v2, params);

        ProceduralSurfaceBuffer surface;
        if (group.size() > 1) {
            float bilinear_params[8] = {0, 0, 0, 0, 0, 0, 0, 0};
            ProceduralSurfaceBuffer primary = SurfaceBufferUtils::CreateWithAnchors(EQUATION_PLANE, anchor_indices, params, MOD_NONE);
            ProceduralSurfaceBuffer secondary = SurfaceBufferUtils::CreateWithAnchors(EQUATION_BILINEAR, anchor_indices, bilinear_params, MOD_NONE);
            std::vector<ProceduralSurfaceBuffer*> buffers = {&primary, &secondary};
            surface = SurfaceBufferUtils::CreateCombination(EQUATION_PLANE, buffers, COMBO_BLEND, MOD_NONE);
            surface.combination_param = 128;
        } else {
            surface = SurfaceBufferUtils::CreateWithAnchors(EQUATION_PLANE, anchor_indices, params, MOD_NONE);
        }

        simplified_collection.AddSurface(surface);
    }

    std::set<uint16_t> used_anchors;
    for (const auto& surface : simplified_collection.surface_pool) {
        for (int i = 0; i < 4; ++i) {
            used_anchors.insert(surface.anchor_point_indices[i]);
        }
    }

    std::vector<AnchorPointBuffer> new_anchor_pool;
    std::map<uint16_t, uint16_t> old_to_new_index;
    for (uint16_t i = 0; i < input_collection.anchor_pool.size(); ++i) {
        if (used_anchors.find(i) != used_anchors.end()) {
            old_to_new_index[i] = static_cast<uint16_t>(new_anchor_pool.size());
            new_anchor_pool.push_back(input_collection.anchor_pool[i]);
        }
    }

    simplified_collection.anchor_pool = new_anchor_pool;

    for (auto& surface : simplified_collection.surface_pool) {
        for (int i = 0; i < 4; ++i) {
            surface.anchor_point_indices[i] = old_to_new_index[surface.anchor_point_indices[i]];
        }
    }

    simplified_collection.UpdateHeader();

    if (verbose) {
        std::cout << "Simplified PSF: " << simplified_collection.anchor_pool.size() << " anchors, "
                  << simplified_collection.surface_pool.size() << " surfaces\n";
    }

    return simplified_collection;
}

bool ConvertPSFToOBJManually(const std::string& psf_file, const std::string& obj_file, bool verbose) {
    std::cout << "Converting PSF to OBJ (manual method)...\n";
    std::cout << "Input:  " << psf_file << "\n";
    std::cout << "Output: " << obj_file << "\n";

    ProceduralObjectCollection collection;
    if (!collection.LoadFromFile(psf_file)) {
        std::cout << "❌ Error: Failed to load PSF file: " << psf_file << "\n";
        return false;
    }

    DebugPSF(collection, verbose);

    std::vector<float> vertices, normals;
    std::vector<uint32_t> indices;

    std::map<std::tuple<float, float, float>, uint32_t> vertex_map;

    for (size_t i = 0; i < collection.surface_pool.size(); ++i) {
        const auto& surface = collection.surface_pool[i];
        std::vector<uint32_t> surface_indices;

        for (int j = 0; j < 4; ++j) {
            uint16_t anchor_idx = surface.anchor_point_indices[j];
            if (anchor_idx >= collection.anchor_pool.size()) {
                std::cout << "⚠️ Warning: Invalid anchor index " << anchor_idx << " in surface " << i << "\n";
                continue;
            }

            Vec3 pos = AnchorPointEvaluator::DecodePosition(collection.anchor_pool[anchor_idx].pos_xyz);
            std::tuple<float, float, float> vertex_key = {pos.x, pos.y, pos.z};

            uint32_t vertex_index;
            auto it = vertex_map.find(vertex_key);
            if (it == vertex_map.end()) {
                vertex_index = static_cast<uint32_t>(vertices.size() / 3);
                vertex_map[vertex_key] = vertex_index;
                vertices.push_back(pos.x);
                vertices.push_back(pos.y);
                vertices.push_back(pos.z);

                const auto& anchor = collection.anchor_pool[anchor_idx];
                Vec3 normal = {
                    (anchor.normal[0] / 255.0f) * 2.0f - 1.0f,
                    (anchor.normal[1] / 255.0f) * 2.0f - 1.0f,
                    (anchor.normal[2] / 255.0f) * 2.0f - 1.0f
                };
                NormalizeVector(normal);
                normals.push_back(normal.x);
                normals.push_back(normal.y);
                normals.push_back(normal.z);
            } else {
                vertex_index = it->second;
            }

            surface_indices.push_back(vertex_index);
        }

        if (surface_indices.size() >= 4) {
            indices.push_back(surface_indices[0]);
            indices.push_back(surface_indices[1]);
            indices.push_back(surface_indices[2]);
            indices.push_back(surface_indices[3]);
        }
    }

    if (vertices.empty() || indices.empty()) {
        std::cout << "❌ Error: No geometry generated from PSF file\n";
        return false;
    }

    if (verbose) {
        std::cout << "Generated geometry: " << vertices.size()/3 << " vertices, "
                  << indices.size()/4 << " faces\n";
    }

    if (!ExportToOBJ(obj_file, vertices, indices, normals)) {
        std::cout << "❌ Error: Failed to write OBJ file: " << obj_file << "\n";
        return false;
    }

    std::ifstream check_file(obj_file, std::ios::ate);
    if (check_file.is_open()) {
        auto file_size = check_file.tellg();
        std::cout << "   Size: " << file_size << " bytes\n";
        check_file.close();
    }

    std::cout << "✅ Successfully converted PSF to OBJ (manual method)!\n";
    return true;
}

int main(int argc, char* argv[]) {
    Args args;
    if (!args.Parse(argc, argv)) {
        return 1;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    if (args.test_mode) {
        std::cout << "=== TEST MODE ===\n";
        std::cout << "Creating test PSF object: " << args.output_file << "\n";
        
        ProceduralObjectCollection test_collection = CreateTestObject();
        if (!test_collection.SaveToFile(args.output_file)) {
            std::cout << "❌ Failed to save test PSF file\n";
            return 1;
        }
        
        if (args.verbose) {
            VerifyPSF(args.output_file, true);
        }
        
        std::cout << "✅ Test object created successfully\n";
    } else if (args.convert_mode) {
        std::cout << "=== CONVERT MODE ===\n";
        if (!VerifyPSF(args.input_file, args.verbose)) {
            return 1;
        }
        
        if (!ConvertPSFToOBJManually(args.input_file, args.output_file, args.verbose)) {
            std::cout << "❌ Failed to convert PSF to OBJ\n";
            return 1;
        }
    } else if (args.simplify_mode) {
        std::cout << "=== SIMPLIFY MODE ===\n";
        if (!VerifyPSF(args.input_file, args.verbose)) {
            return 1;
        }
        
        ProceduralObjectCollection input_collection;
        if (!input_collection.LoadFromFile(args.input_file)) {
            std::cout << "❌ Failed to load input PSF file\n";
            return 1;
        }
        
        ProceduralObjectCollection simplified_collection = SimplifyPSF(input_collection, args.verbose);
        
        if (!simplified_collection.SaveToFile(args.output_file)) {
            std::cout << "❌ Failed to save simplified PSF file\n";
            return 1;
        }
        
        if (args.verbose) {
            VerifyPSF(args.output_file, true);
        }
        
        std::cout << "✅ PSF simplified successfully\n";
    } else {
        // Assume FBX input
        std::string ext = args.input_file.substr(args.input_file.find_last_of('.') + 1);
        if (ext != "fbx" && ext != "FBX") {
            std::cout << "❌ Input file must be .fbx for direct conversion\n";
            return 1;
        }
        
        if (!MeshAwareFBXConverter::ConvertFBXToPSFEnhanced(args.input_file, args.output_file, args.verbose)) {
            std::cout << "❌ Failed to convert FBX to PSF\n";
            return 1;
        }
        
        if (args.verbose) {
            VerifyPSF(args.output_file, true);
        }
        
        std::cout << "✅ FBX to PSF conversion successful\n";
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "\nExecution time: " << duration.count() << " ms\n";
    
    return 0;
}