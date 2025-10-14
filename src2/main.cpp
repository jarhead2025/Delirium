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
#include <utility>

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

// Mathematical complexity levels enum
enum MathematicalComplexityLevel {
    COMPLEXITY_BASIC = 0,
    COMPLEXITY_BILINEAR = 1,
    COMPLEXITY_QUADRATIC = 2,
    COMPLEXITY_BEZIER = 3,
    COMPLEXITY_METABALL = 4,
    COMPLEXITY_ADVANCED = 5
};

struct Args {
    bool verbose = false;
    std::string output_file;
    std::string input_file;
    bool test_mode = false;
    bool convert_mode = false;
    bool simplify_mode = false;
    bool ultra_simplify_mode = false;
    bool analyze_mode = false;
    bool ParseEnhanced(int argc, char* argv[]);
};




struct Vec3Hash {
    size_t operator()(const std::tuple<float, float, float>& v) const {
        const float epsilon = 1e-6f;
        auto [x, y, z] = v;
        return std::hash<int>()(static_cast<int>(x / epsilon)) ^
               std::hash<int>()(static_cast<int>(y / epsilon)) ^
               std::hash<int>()(static_cast<int>(z / epsilon));
    }
};

struct Vec3Equal {
    bool operator()(const std::tuple<float, float, float>& a, const std::tuple<float, float, float>& b) const {
        const float epsilon = 1e-6f;
        return std::abs(std::get<0>(a) - std::get<0>(b)) < epsilon &&
               std::abs(std::get<1>(a) - std::get<1>(b)) < epsilon &&
               std::abs(std::get<2>(a) - std::get<2>(b)) < epsilon;
    }
};



bool IsValidInputFile(const std::string& filename, bool& is_fbx, bool& is_psf) {
    is_fbx = false;
    is_psf = false;
    
    if (filename.empty()) return false;
    
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) return false;
    
    std::string ext = filename.substr(dot_pos + 1);
    
    // Convert to lowercase for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "fbx") {
        is_fbx = true;
        return true;
    } else if (ext == "psf") {
        is_psf = true;
        return true;
    }
    
    return false;
}




// Function declarations
static ProceduralObjectCollection AggressiveSimplifyPSF(const ProceduralObjectCollection& input_collection, bool verbose, float angleThresholdDegrees = 15.0f, float planeTolerance = 0.1f, int maxIterations = 5);
static ProceduralObjectCollection PerformAggressiveSimplificationPass(const ProceduralObjectCollection& input_collection, bool verbose, float angleThresholdDegrees, float planeTolerance, int iteration);
static ProceduralObjectCollection PerformMegaAggressivePass(const ProceduralObjectCollection& input_collection, bool verbose);
static void CreateOptimizedSurfaceFromGroup(const std::vector<size_t>& group, const ProceduralObjectCollection& input_collection, ProceduralObjectCollection& output_collection, bool verbose);
static void CreateMegaSurfaceFromSurfaces(const std::vector<size_t>& surfaces, const ProceduralObjectCollection& input_collection, ProceduralObjectCollection& output_collection, bool verbose);
static void SelectOptimalAnchors(const std::vector<Vec3>& positions, const std::vector<uint16_t>& anchor_indices, uint16_t selected[4]);
static void SelectCornerAnchors(const std::vector<Vec3>& positions, const std::vector<uint16_t>& anchor_indices, uint16_t selected[4]);
static void CalculateBestFitPlane(const std::vector<Vec3>& positions, float params[8]);
static void CompactAnchorPool(ProceduralObjectCollection& collection, bool verbose);
static void AnalyzeSimplificationPotential(const ProceduralObjectCollection& collection, bool verbose);

// Mathematical helper function declarations
static Vec3 CalculateCentroid(const std::vector<Vec3>& positions);
static float CalculateBoundingRadius(const std::vector<Vec3>& positions, const Vec3& center);
static void CalculateBilinearFit(const std::vector<Vec3>& positions, float params[8]);
static void CalculateQuadraticFit(const std::vector<Vec3>& positions, float params[8]);
static void CalculateBezierControlPoints(const std::vector<Vec3>& positions, float params[8]);

// Evolution function declarations
static ProceduralSurfaceBuffer CreateEvolutionSurface(const std::vector<size_t>& group, const ProceduralObjectCollection& input_collection, MathematicalComplexityLevel complexity_level, int iteration, bool verbose);
static ProceduralObjectCollection PerformMathematicalEvolutionPass(const ProceduralObjectCollection& input_collection, MathematicalComplexityLevel complexity_level, float angle_threshold, float plane_tolerance, int iteration, bool verbose);
static ProceduralObjectCollection UltraAggressiveWithMathEvolution(const ProceduralObjectCollection& input_collection, bool verbose);

// Additional function declarations
bool ExportToOBJ(const std::string& filename, const std::vector<float>& vertices, const std::vector<uint32_t>& indices, const std::vector<float>& normals);
ProceduralObjectCollection CreateTestObject();
bool VerifyPSF(const std::string& psf_file, bool verbose);
bool ConvertPSFToOBJ(const std::string& psf_file, const std::string& obj_file, bool verbose, int resolution = 4);
bool ConvertPSFToOBJManually(const std::string& psf_file, const std::string& obj_file, bool verbose);
void DebugPSF(const ProceduralObjectCollection& collection, bool verbose);
ProceduralObjectCollection SimplifyPSF(const ProceduralObjectCollection& input_collection, bool verbose, float angleThresholdDegrees = 5.0f, float planeTolerance = 0.01f);
ProceduralObjectCollection UltraSimplifyPSF(const ProceduralObjectCollection& input_collection, bool verbose);









// Utility function implementations
static float CalculateDistanceBetweenPoints(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrt(dx*dx + dy*dy + dz*dz);
}

static std::pair<Vec3, Vec3> FindBoundingBox(const std::vector<Vec3>& positions) {
    if (positions.empty()) {
        return std::make_pair(Vec3(0,0,0), Vec3(0,0,0));
    }
    
    Vec3 min_pos = positions[0];
    Vec3 max_pos = positions[0];
    
    for (const Vec3& pos : positions) {
        min_pos.x = std::min(min_pos.x, pos.x);
        min_pos.y = std::min(min_pos.y, pos.y);
        min_pos.z = std::min(min_pos.z, pos.z);
        max_pos.x = std::max(max_pos.x, pos.x);
        max_pos.y = std::max(max_pos.y, pos.y);
        max_pos.z = std::max(max_pos.z, pos.z);
    }
    
    return std::make_pair(min_pos, max_pos);
}

static float DotProduct(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static bool AreNormalsSimilar(const Vec3& n1, const Vec3& n2, float angleThresholdDegrees) {
    float dot = DotProduct(n1, n2);
    dot = std::max(-1.0f, std::min(1.0f, dot)); // Clamp to [-1, 1]
    float angle = acos(dot) * 180.0f / static_cast<float>(M_PI);
    return angle < angleThresholdDegrees;
}

static bool IsPointOnPlane(const Vec3& point, const float params[8], float tolerance) {
    float a = params[0], b = params[1], c = params[2], d = params[3];
    float distance = fabs(a * point.x + b * point.y + c * point.z + d);
    return distance < tolerance;
}






// Mathematical helper functions (single definitions)
static Vec3 CalculateCentroid(const std::vector<Vec3>& positions) {
    if (positions.empty()) return Vec3(0, 0, 0);
    
    Vec3 sum(0, 0, 0);
    for (const Vec3& pos : positions) {
        sum.x += pos.x;
        sum.y += pos.y;
        sum.z += pos.z;
    }
    
    return Vec3(sum.x / positions.size(), 
                sum.y / positions.size(), 
                sum.z / positions.size());
}

static float CalculateBoundingRadius(const std::vector<Vec3>& positions, const Vec3& center) {
    float max_dist = 0.0f;
    for (const Vec3& pos : positions) {
        float dist = CalculateDistanceBetweenPoints(pos, center);
        max_dist = std::max(max_dist, dist);
    }
    return max_dist;
}

static void CalculateBilinearFit(const std::vector<Vec3>& positions, float params[8]) {
    Vec3 center = CalculateCentroid(positions);
    std::pair<Vec3, Vec3> bbox = FindBoundingBox(positions);
    
    Vec3 size = Vec3(bbox.second.x - bbox.first.x, 
                     bbox.second.y - bbox.first.y, 
                     bbox.second.z - bbox.first.z);
    
    // Simple bilinear parameters
    params[0] = center.z;           // Base height
    params[1] = size.x * 0.05f;     // X gradient
    params[2] = size.y * 0.05f;     // Y gradient
    params[3] = size.z * 0.02f;     // XY interaction
    params[4] = center.x;           // X offset
    params[5] = center.y;           // Y offset
    params[6] = 0.0f;
    params[7] = 0.0f;
}

static void CalculateQuadraticFit(const std::vector<Vec3>& positions, float params[8]) {
    Vec3 center = CalculateCentroid(positions);
    std::pair<Vec3, Vec3> bbox = FindBoundingBox(positions);
    
    Vec3 size = Vec3(bbox.second.x - bbox.first.x, 
                     bbox.second.y - bbox.first.y, 
                     bbox.second.z - bbox.first.z);
    
    // Simple quadratic parameters
    params[0] = center.x;
    params[1] = center.y;
    params[2] = center.z;
    params[3] = size.x * size.x * 0.01f;    // X² coefficient
    params[4] = size.y * size.y * 0.01f;    // Y² coefficient
    params[5] = size.x * size.y * 0.005f;   // XY coefficient
    params[6] = 0.0f;
    params[7] = 0.0f;
}

static void CalculateBezierControlPoints(const std::vector<Vec3>& positions, float params[8]) {
    Vec3 center = CalculateCentroid(positions);
    std::pair<Vec3, Vec3> bbox = FindBoundingBox(positions);
    
    Vec3 size = Vec3(bbox.second.x - bbox.first.x, 
                     bbox.second.y - bbox.first.y, 
                     bbox.second.z - bbox.first.z);
    
    // Simple Bezier control points
    params[0] = center.x;
    params[1] = center.y; 
    params[2] = center.z;
    params[3] = size.x * 0.25f;   // Control strength X
    params[4] = size.y * 0.25f;   // Control strength Y
    params[5] = size.z * 0.25f;   // Control strength Z
    params[6] = 0.5f;
    params[7] = 0.5f;
}




// Args structure for command line parsing
bool Args::ParseEnhanced(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Procedural Surface Engine - Enhanced Converter\n";
        std::cout << "Usage:\n";
        std::cout << "  FBX Import:   " << argv[0] << " input.fbx [-o output.psf] [-v]\n";
        std::cout << "  Test mode:    " << argv[0] << " --test [-v] [-o output.psf]\n";
        std::cout << "  Convert PSF:  " << argv[0] << " --convert input.psf [-o output.obj] [-v]\n";
        std::cout << "  Simplify PSF: " << argv[0] << " --simplify input.psf [-o output.psf] [-v]\n";
        std::cout << "  Ultra Simplify: " << argv[0] << " --ultra-simplify input.psf [-o output.psf] [-v]\n";
        std::cout << "  Analyze PSF:  " << argv[0] << " --analyze input.psf [-v]\n";
        std::cout << "\nSupported formats:\n";
        std::cout << "  Input:  .fbx (Autodesk FBX), .psf (Procedural Surface Format)\n";
        std::cout << "  Output: .psf (Procedural Surface Format), .obj (Wavefront OBJ)\n";
        std::cout << "\nOptions:\n";
        std::cout << "  -v            Verbose output with detailed statistics\n";
        std::cout << "  -o <file>     Specify output file (auto-detected if not provided)\n";
        std::cout << "\nExamples:\n";
        std::cout << "  " << argv[0] << " model.fbx -v                    # Convert FBX to PSF\n";
        std::cout << "  " << argv[0] << " --convert model.psf -v          # Convert PSF to OBJ\n";
        std::cout << "  " << argv[0] << " --simplify model.psf -v         # Simplify PSF\n";
        
#ifdef NO_FBX_SDK
        std::cout << "\n⚠️  FBX SDK not available - only PSF operations supported\n";
        std::cout << "To enable FBX support: recompile without -DNO_FBX_SDK flag\n";
#else
        std::cout << "\n✅ FBX SDK available - full conversion pipeline supported\n";
#endif
        
        return false;
    }
    
    // Parse arguments (same as original Parse method)
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
        } else if (arg == "--ultra-simplify") {
            ultra_simplify_mode = true;
        } else if (arg == "--analyze") {
            analyze_mode = true;
        } else if (input_file.empty()) {
            input_file = arg;
        }
    }
    
    // Auto-detect output file based on input and mode
    if (output_file.empty() && !test_mode) {
        bool is_fbx, is_psf;
        if (IsValidInputFile(input_file, is_fbx, is_psf)) {
            size_t dot_pos = input_file.find_last_of('.');
            std::string base_name = (dot_pos != std::string::npos) ? 
                input_file.substr(0, dot_pos) : input_file;
            
            if (convert_mode) {
                output_file = base_name + ".obj";
            } else if (simplify_mode || ultra_simplify_mode) {
                output_file = base_name + "_simplified.psf";
            } else if (is_fbx) {
                output_file = base_name + ".psf";
            } else {
                output_file = base_name + "_processed.psf";
            }
        }
    } else if (output_file.empty() && test_mode) {
        output_file = "test_object.psf";
    }
    
    return !input_file.empty();
}


// Test object creation function
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
    
    // Calculate plane parameters from the test positions
    Vec3 edge1 = Vec3(positions[1].x - positions[0].x, positions[1].y - positions[0].y, positions[1].z - positions[0].z);
    Vec3 edge2 = Vec3(positions[2].x - positions[0].x, positions[2].y - positions[0].y, positions[2].z - positions[0].z);
    
    Vec3 normal;
    normal.x = edge1.y * edge2.z - edge1.z * edge2.y;
    normal.y = edge1.z * edge2.x - edge1.x * edge2.z;
    normal.z = edge1.x * edge2.y - edge1.y * edge2.x;
    
    float length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length > 1e-6f) {
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
    }
    
    float d = -(normal.x * positions[0].x + normal.y * positions[0].y + normal.z * positions[0].z);
    
    params[0] = normal.x;
    params[1] = normal.y;
    params[2] = normal.z;
    params[3] = d;
    
    ProceduralSurfaceBuffer surface = SurfaceBufferUtils::CreateWithAnchors(
        EQUATION_PLANE, anchor_indices, params, MOD_NONE);
    
    collection.AddSurface(surface);
    collection.UpdateHeader();
    return collection;
}

// PSF file verification function
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

// Debug PSF function
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
                  << surface.anchor_point_indices[3] << "]\n";
    }
}



// OBJ export function
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

// Manual PSF to OBJ conversion
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

// Basic simplification function
ProceduralObjectCollection SimplifyPSF(const ProceduralObjectCollection& input_collection, bool verbose, float angleThresholdDegrees, float planeTolerance) {
    ProceduralObjectCollection simplified_collection = input_collection;
    simplified_collection.surface_pool.clear();
    
    // Build edge-to-surface mapping for adjacency detection
    std::map<std::pair<uint16_t, uint16_t>, std::vector<size_t>> edge_to_surfaces;
    for (size_t i = 0; i < input_collection.surface_pool.size(); ++i) {
        const auto& surface = input_collection.surface_pool[i];
        for (int j = 0; j < 4; ++j) {
            int k = (j + 1) % 4;
            auto edge = std::minmax(surface.anchor_point_indices[j], surface.anchor_point_indices[k]);
            edge_to_surfaces[edge].push_back(i);
        }
    }

    // Group coplanar adjacent surfaces
    std::vector<bool> used(input_collection.surface_pool.size(), false);
    std::vector<std::vector<size_t>> surface_groups;

    for (size_t i = 0; i < input_collection.surface_pool.size(); ++i) {
        if (used[i]) continue;
        
        std::vector<size_t> group = {i};
        used[i] = true;
        
        const float* params = SurfaceBufferUtils::ExtractParameters(&input_collection.surface_pool[i]);
        Vec3 normal = Vec3(params[0], params[1], params[2]);

        // Flood-fill to find connected coplanar surfaces
        std::vector<size_t> to_check = {i};
        while (!to_check.empty()) {
            size_t curr = to_check.back();
            to_check.pop_back();
            
            // Check all edges of current surface
            for (int j = 0; j < 4; ++j) {
                int k = (j + 1) % 4;
                auto edge = std::minmax(input_collection.surface_pool[curr].anchor_point_indices[j],
                                       input_collection.surface_pool[curr].anchor_point_indices[k]);
                
                // Check all surfaces sharing this edge
                auto edge_it = edge_to_surfaces.find(edge);
                if (edge_it != edge_to_surfaces.end()) {
                    for (size_t neighbor : edge_it->second) {
                        if (!used[neighbor]) {
                            const float* neighbor_params = SurfaceBufferUtils::ExtractParameters(&input_collection.surface_pool[neighbor]);
                            Vec3 neighbor_normal = Vec3(neighbor_params[0], neighbor_params[1], neighbor_params[2]);
                            
                            // Check if normals are similar (within angle threshold)
                            if (AreNormalsSimilar(normal, neighbor_normal, angleThresholdDegrees)) {
                                // Check if surfaces are coplanar
                                bool coplanar = true;
                                for (int a = 0; a < 4 && coplanar; ++a) {
                                    uint16_t anchor_idx = input_collection.surface_pool[neighbor].anchor_point_indices[a];
                                    if (anchor_idx < input_collection.anchor_pool.size()) {
                                        Vec3 pos = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_idx].pos_xyz);
                                        if (!IsPointOnPlane(pos, params, planeTolerance)) {
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
        }
        
        if (!group.empty()) {
            surface_groups.push_back(group);
        }
    }

    if (verbose) {
        std::cout << "Found " << surface_groups.size() << " coplanar surface groups\n";
    }

    // Create simplified surfaces from groups
    for (const auto& group : surface_groups) {
        // Collect all unique anchors from the group
        std::set<uint16_t> group_anchors;
        for (size_t idx : group) {
            const auto& surface = input_collection.surface_pool[idx];
            for (int i = 0; i < 4; ++i) {
                group_anchors.insert(surface.anchor_point_indices[i]);
            }
        }

        // Get positions of all anchors in this group
        std::vector<Vec3> group_positions;
        for (uint16_t anchor_idx : group_anchors) {
            if (anchor_idx < input_collection.anchor_pool.size()) {
                group_positions.push_back(AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_idx].pos_xyz));
            }
        }

        // Find bounding box of the group
        std::pair<Vec3, Vec3> bbox = FindBoundingBox(group_positions);
        Vec3 min_pos = bbox.first;
        Vec3 max_pos = bbox.second;

        // Select 4 representative anchors from the group
        std::vector<std::pair<float, uint16_t>> anchor_distances;
        for (uint16_t anchor_idx : group_anchors) {
            if (anchor_idx < input_collection.anchor_pool.size()) {
                Vec3 pos = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_idx].pos_xyz);
                float dist = CalculateDistanceBetweenPoints(pos, min_pos);
                anchor_distances.push_back({dist, anchor_idx});
            }
        }
        std::sort(anchor_distances.begin(), anchor_distances.end());

        // Select 4 anchors: closest to min, farthest from min, and two intermediate
        uint16_t anchor_indices[4] = {0, 0, 0, 0};
        if (anchor_distances.size() >= 4) {
            anchor_indices[0] = anchor_distances[0].second;
            anchor_indices[1] = anchor_distances[1].second;
            anchor_indices[2] = anchor_distances[anchor_distances.size() - 2].second;
            anchor_indices[3] = anchor_distances[anchor_distances.size() - 1].second;
        } else {
            // Not enough anchors, just use what we have
            size_t i = 0;
            for (uint16_t idx : group_anchors) {
                anchor_indices[i++] = idx;
                if (i >= 4) break;
            }
            // Fill remaining slots with duplicates
            while (i < 4) {
                anchor_indices[i] = anchor_indices[0];
                i++;
            }
        }

        // Calculate plane parameters for the simplified surface
        float params[8] = {0};
        if (anchor_indices[0] < input_collection.anchor_pool.size() &&
            anchor_indices[1] < input_collection.anchor_pool.size() &&
            anchor_indices[2] < input_collection.anchor_pool.size()) {
            Vec3 v0 = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_indices[0]].pos_xyz);
            Vec3 v1 = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_indices[1]].pos_xyz);
            Vec3 v2 = AnchorPointEvaluator::DecodePosition(input_collection.anchor_pool[anchor_indices[2]].pos_xyz);
            
            // Calculate plane from three points
            Vec3 edge1 = Vec3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
            Vec3 edge2 = Vec3(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
            
            Vec3 normal;
            normal.x = edge1.y * edge2.z - edge1.z * edge2.y;
            normal.y = edge1.z * edge2.x - edge1.x * edge2.z;
            normal.z = edge1.x * edge2.y - edge1.y * edge2.x;
            
            float length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (length > 1e-6f) {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }
            
            float d = -(normal.x * v0.x + normal.y * v0.y + normal.z * v0.z);
            
            params[0] = normal.x;
            params[1] = normal.y;
            params[2] = normal.z;
            params[3] = d;
        }

        // Create the simplified surface
        ProceduralSurfaceBuffer surface = SurfaceBufferUtils::CreateWithAnchors(EQUATION_PLANE, anchor_indices, params, MOD_NONE);
        simplified_collection.AddSurface(surface);
    }

    // Clean up unused anchors
    std::set<uint16_t> used_anchors;
    for (const auto& surface : simplified_collection.surface_pool) {
        for (int i = 0; i < 4; ++i) {
            used_anchors.insert(surface.anchor_point_indices[i]);
        }
    }

    // Create new anchor pool with only used anchors
    std::vector<AnchorPointBuffer> new_anchor_pool;
    std::map<uint16_t, uint16_t> old_to_new_index;
    for (uint16_t i = 0; i < input_collection.anchor_pool.size(); ++i) {
        if (used_anchors.find(i) != used_anchors.end()) {
            old_to_new_index[i] = static_cast<uint16_t>(new_anchor_pool.size());
            new_anchor_pool.push_back(input_collection.anchor_pool[i]);
        }
    }

    simplified_collection.anchor_pool = new_anchor_pool;

    // Update surface anchor indices to point to new anchor pool
    for (auto& surface : simplified_collection.surface_pool) {
        for (int i = 0; i < 4; ++i) {
            auto it = old_to_new_index.find(surface.anchor_point_indices[i]);
            if (it != old_to_new_index.end()) {
                surface.anchor_point_indices[i] = it->second;
            } else {
                surface.anchor_point_indices[i] = 0;
            }
        }
    }

    simplified_collection.UpdateHeader();

    if (verbose) {
        std::cout << "Simplified PSF: " << simplified_collection.anchor_pool.size() << " anchors, "
                  << simplified_collection.surface_pool.size() << " surfaces\n";
        std::cout << "Reduction: " << input_collection.surface_pool.size() - simplified_collection.surface_pool.size() 
                  << " surfaces combined\n";
    }

    return simplified_collection;
}




// Analysis function
void AnalyzeSimplificationPotential(const ProceduralObjectCollection& collection, bool verbose) {
    if (!verbose) return;
    
    std::cout << "\n=== SIMPLIFICATION ANALYSIS ===\n";
    std::cout << "Input surfaces: " << collection.surface_pool.size() << "\n";
    std::cout << "Input anchors: " << collection.anchor_pool.size() << "\n";
    
    // Analyze surface types
    std::map<SurfaceEquationType, int> surface_type_count;
    for (const auto& surface : collection.surface_pool) {
        SurfaceEquationType type = SurfaceBufferUtils::ExtractBaseType(&surface);
        surface_type_count[type]++;
    }
    
    std::cout << "Surface types:\n";
    for (const auto& pair : surface_type_count) {
        std::cout << "  Type " << static_cast<int>(pair.first) << ": " << pair.second << " surfaces\n";
    }
    
    // Analyze connectivity
    std::map<std::pair<uint16_t, uint16_t>, int> edge_usage;
    for (const auto& surface : collection.surface_pool) {
        for (int j = 0; j < 4; ++j) {
            int k = (j + 1) % 4;
            auto edge = std::minmax(surface.anchor_point_indices[j], surface.anchor_point_indices[k]);
            edge_usage[edge]++;
        }
    }
    
    int shared_edges = 0;
    for (const auto& pair : edge_usage) {
        if (pair.second > 1) shared_edges++;
    }
    
    std::cout << "Connectivity: " << shared_edges << " shared edges (potential for merging)\n";
    
    // Estimate simplification potential
    float potential_reduction = std::min(90.0f, shared_edges * 100.0f / collection.surface_pool.size());
    std::cout << "Estimated reduction potential: " << potential_reduction << "%\n";
}

// Ultra-aggressive simplification function (placeholder - uses mathematical evolution)
ProceduralObjectCollection UltraSimplifyPSF(const ProceduralObjectCollection& input_collection, bool verbose) {
    if (verbose) {
        std::cout << "\n=== ULTRA-AGGRESSIVE SIMPLIFICATION ===\n";
        std::cout << "Input: " << input_collection.surface_pool.size() << " surfaces\n";
    }
    
    // For now, do multiple passes of basic simplification with increasingly aggressive parameters
    ProceduralObjectCollection current_collection = input_collection;
    
    float angle_threshold = 5.0f;
    float plane_tolerance = 0.01f;
    
    for (int iteration = 0; iteration < 3; ++iteration) {
        if (verbose) {
            std::cout << "Ultra-aggressive iteration " << (iteration + 1) << "\n";
            std::cout << "  Angle threshold: " << angle_threshold << "°\n";
            std::cout << "  Plane tolerance: " << plane_tolerance << "\n";
        }
        
        ProceduralObjectCollection simplified = SimplifyPSF(current_collection, verbose, angle_threshold, plane_tolerance);
        
        size_t surfaces_before = current_collection.surface_pool.size();
        size_t surfaces_after = simplified.surface_pool.size();
        
        if (verbose) {
            std::cout << "  Result: " << surfaces_before << " → " << surfaces_after << " surfaces\n";
        }
        
        // If no improvement, stop
        if (surfaces_after >= surfaces_before || surfaces_after == 0) {
            if (verbose) {
                std::cout << "  No further improvement possible\n";
            }
            break;
        }
        
        current_collection = simplified;
        
        // Make parameters more aggressive for next iteration
        angle_threshold *= 2.0f;  // Allow wider angle differences
        plane_tolerance *= 2.0f;  // Allow more distance from plane
    }
    
    if (verbose) {
        std::cout << "=== ULTRA-AGGRESSIVE COMPLETE ===\n";
        std::cout << "Final: " << current_collection.surface_pool.size() << " surfaces\n";
        if (input_collection.surface_pool.size() > 0) {
            size_t total_reduction = input_collection.surface_pool.size() - current_collection.surface_pool.size();
            float reduction_percentage = (100.0f * total_reduction) / input_collection.surface_pool.size();
            std::cout << "Total reduction: " << total_reduction << " surfaces (" << reduction_percentage << "%)\n";
        }
    }
    
    return current_collection;
}

// Anchor selection helper functions
void SelectOptimalAnchors(const std::vector<Vec3>& positions, const std::vector<uint16_t>& anchor_indices, uint16_t selected[4]) {
    if (positions.empty()) {
        selected[0] = selected[1] = selected[2] = selected[3] = 0;
        return;
    }
    
    if (positions.size() <= 4) {
        // Use all available positions
        for (size_t i = 0; i < positions.size(); ++i) {
            selected[i] = anchor_indices[i];
        }
        // Fill remaining with duplicates
        for (size_t i = positions.size(); i < 4; ++i) {
            selected[i] = anchor_indices[0];
        }
        return;
    }
    
    // Find bounding box
    std::pair<Vec3, Vec3> bbox = FindBoundingBox(positions);
    Vec3 min_pos = bbox.first;
    Vec3 max_pos = bbox.second;
    
    // Find anchors closest to corners of bounding box
    Vec3 corners[4] = {
        Vec3(min_pos.x, min_pos.y, min_pos.z),  // Bottom-left-front
        Vec3(max_pos.x, min_pos.y, min_pos.z),  // Bottom-right-front
        Vec3(max_pos.x, max_pos.y, max_pos.z),  // Top-right-back
        Vec3(min_pos.x, max_pos.y, max_pos.z)   // Top-left-back
    };
    
    for (int corner = 0; corner < 4; ++corner) {
        float min_dist = std::numeric_limits<float>::max();
        int best_anchor = 0;
        
        for (size_t i = 0; i < positions.size(); ++i) {
            float dist = CalculateDistanceBetweenPoints(positions[i], corners[corner]);
            if (dist < min_dist) {
                min_dist = dist;
                best_anchor = static_cast<int>(i);
            }
        }
        
        selected[corner] = anchor_indices[best_anchor];
    }
}

void SelectCornerAnchors(const std::vector<Vec3>& positions, const std::vector<uint16_t>& anchor_indices, uint16_t selected[4]) {
    if (positions.empty()) {
        selected[0] = selected[1] = selected[2] = selected[3] = 0;
        return;
    }
    
    // Use PCA-like approach to find optimal corner distribution
    Vec3 center = Vec3(0.0f, 0.0f, 0.0f);
    for (const Vec3& pos : positions) {
        center.x += pos.x;
        center.y += pos.y;
        center.z += pos.z;
    }
    center.x /= positions.size();
    center.y /= positions.size();
    center.z /= positions.size();
    
    // Find extremes in each direction
    std::vector<std::pair<float, int>> x_sorted, y_sorted, z_sorted;
    
    for (size_t i = 0; i < positions.size(); ++i) {
        x_sorted.push_back({positions[i].x, static_cast<int>(i)});
        y_sorted.push_back({positions[i].y, static_cast<int>(i)});
        z_sorted.push_back({positions[i].z, static_cast<int>(i)});
    }
    
    std::sort(x_sorted.begin(), x_sorted.end());
    std::sort(y_sorted.begin(), y_sorted.end());
    std::sort(z_sorted.begin(), z_sorted.end());
    
    // Select anchors from extremes
    std::set<int> selected_indices;
    selected_indices.insert(x_sorted.front().second);  // Min X
    selected_indices.insert(x_sorted.back().second);   // Max X
    selected_indices.insert(y_sorted.front().second);  // Min Y
    selected_indices.insert(y_sorted.back().second);   // Max Y
    
    // If we don't have 4 unique anchors, add more
    if (selected_indices.size() < 4) {
        selected_indices.insert(z_sorted.front().second);  // Min Z
        selected_indices.insert(z_sorted.back().second);   // Max Z
    }
    
    // Convert to array
    auto it = selected_indices.begin();
    for (int i = 0; i < 4; ++i) {
        if (it != selected_indices.end()) {
            selected[i] = anchor_indices[*it];
            ++it;
        } else {
            selected[i] = anchor_indices[*selected_indices.begin()];
        }
    }
}

// Plane calculation functions
static void CalculatePlaneParameters(const Vec3& v0, const Vec3& v1, const Vec3& v2, float params[8]) {
    // Calculate normal using cross product of (v1 - v0) and (v2 - v0)
    Vec3 edge1 = Vec3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
    Vec3 edge2 = Vec3(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
    
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
        normal = Vec3(0.0f, 0.0f, 1.0f); // Fallback normal
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

void CalculateBestFitPlane(const std::vector<Vec3>& positions, float params[8]) {
    if (positions.size() < 3) {
        // Default to XY plane
        params[0] = 0.0f; params[1] = 0.0f; params[2] = 1.0f; params[3] = 0.0f;
        params[4] = params[5] = params[6] = params[7] = 0.0f;
        return;
    }
    
    if (positions.size() == 3) {
        // Exact plane from 3 points
        CalculatePlaneParameters(positions[0], positions[1], positions[2], params);
        return;
    }
    
    // For simplicity, use first three points to define plane
    CalculatePlaneParameters(positions[0], positions[1], positions[2], params);
}

void CompactAnchorPool(ProceduralObjectCollection& collection, bool verbose) {
    // Find all used anchors
    std::set<uint16_t> used_anchors;
    for (const auto& surface : collection.surface_pool) {
        for (int i = 0; i < 4; ++i) {
            used_anchors.insert(surface.anchor_point_indices[i]);
        }
    }
    
    // Create mapping from old to new indices
    std::vector<AnchorPointBuffer> new_anchor_pool;
    std::map<uint16_t, uint16_t> old_to_new_index;
    
    for (uint16_t old_index : used_anchors) {
        if (old_index < collection.anchor_pool.size()) {
            old_to_new_index[old_index] = static_cast<uint16_t>(new_anchor_pool.size());
            new_anchor_pool.push_back(collection.anchor_pool[old_index]);
        }
    }
    
    // Update surface anchor indices
    for (auto& surface : collection.surface_pool) {
        for (int i = 0; i < 4; ++i) {
            auto it = old_to_new_index.find(surface.anchor_point_indices[i]);
            if (it != old_to_new_index.end()) {
                surface.anchor_point_indices[i] = it->second;
            } else {
                surface.anchor_point_indices[i] = 0;
            }
        }
    }
    
    // Replace anchor pool
    collection.anchor_pool = new_anchor_pool;
    collection.UpdateHeader();
    
    if (verbose) {
        std::cout << "  Compacted anchor pool: " << used_anchors.size() << " anchors kept\n";
    }
}


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
            std::cout << "Error: " << importer->GetStatus().GetErrorString() << "\n";
            manager->Destroy();
            return false;
        }
        
        FbxScene* scene = FbxScene::Create(manager, "ImportedScene");
        if (!importer->Import(scene)) {
            std::cout << "❌ Failed to import FBX scene\n";
            std::cout << "Error: " << importer->GetStatus().GetErrorString() << "\n";
            importer->Destroy();
            manager->Destroy();
            return false;
        }
        
        importer->Destroy();
        
        // Triangulate the scene
        FbxGeometryConverter geometryConverter(manager);
        if (!geometryConverter.Triangulate(scene, true)) {
            std::cout << "⚠️ Warning: Failed to triangulate some meshes\n";
        }
        
        std::vector<EnhancedMeshData> meshes;
        FbxNode* rootNode = scene->GetRootNode();
        if (rootNode) {
            std::cout << "Extracting enhanced meshes from FBX...\n";
            ExtractEnhancedMeshFromNode(rootNode, meshes, verbose);
        }
        
        if (meshes.empty()) {
            std::cout << "❌ No meshes found in FBX file\n";
            manager->Destroy();
            return false;
        }
        
        std::cout << "Found " << meshes.size() << " mesh(es)\n";
        
        // Convert first mesh (or merge all meshes)
        ProceduralObjectCollection collection;
        if (meshes.size() == 1) {
            collection = SmartMeshConverter::ConvertMeshToProcedural(meshes[0], verbose);
        } else {
            // Merge all meshes into one
            EnhancedMeshData merged_mesh;
            merged_mesh.name = "MergedMesh";
            
            size_t total_vertices = 0;
            for (const auto& mesh : meshes) {
                total_vertices += mesh.vertices.size();
            }
            merged_mesh.vertices.reserve(total_vertices);
            merged_mesh.normals.reserve(total_vertices);
            
            size_t vertex_offset = 0;
            for (const auto& mesh : meshes) {
                // Add vertices and normals
                merged_mesh.vertices.insert(merged_mesh.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
                merged_mesh.normals.insert(merged_mesh.normals.end(), mesh.normals.begin(), mesh.normals.end());
                
                // Add faces with adjusted indices
                for (const auto& face : mesh.faces) {
                    std::vector<int> adjusted_face;
                    for (int vertex_idx : face) {
                        adjusted_face.push_back(vertex_idx + static_cast<int>(vertex_offset));
                    }
                    merged_mesh.faces.push_back(adjusted_face);
                }
                
                vertex_offset += mesh.vertices.size();
            }
            
            if (verbose) {
                std::cout << "Merged " << meshes.size() << " meshes into one:\n";
                std::cout << "  Total vertices: " << merged_mesh.vertices.size() << "\n";
                std::cout << "  Total faces: " << merged_mesh.faces.size() << "\n";
            }
            
            collection = SmartMeshConverter::ConvertMeshToProcedural(merged_mesh, verbose);
        }
        
        if (verbose) {
            std::cout << "\n=== CONVERSION DEBUG ===\n";
            std::cout << "Created " << collection.anchor_pool.size() << " anchors\n";
            std::cout << "Created " << collection.surface_pool.size() << " surfaces\n";
            
            for (size_t i = 0; i < std::min(collection.anchor_pool.size(), size_t(3)); ++i) {
                Vec3 pos = AnchorPointEvaluator::DecodePosition(collection.anchor_pool[i].pos_xyz);
                std::cout << "  Anchor " << i << ": (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
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
    }
#else
    bool ConvertFBXToPSFEnhanced(const std::string& fbx_file, const std::string& psf_file, bool verbose) {
        std::cout << "❌ FBX SDK not available. Cannot convert FBX files.\n";
        std::cout << "To enable FBX support:\n";
        std::cout << "1. Install Autodesk FBX SDK\n";
        std::cout << "2. Remove -DNO_FBX_SDK from compile flags\n";
        std::cout << "3. Link with FBX SDK libraries\n";
        return false;
    }
#endif
}


int main(int argc, char* argv[]) {
    Args args;
    if (!args.ParseEnhanced(argc, argv)) {
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
    } else if (args.ultra_simplify_mode) {
        std::cout << "=== ULTRA-AGGRESSIVE SIMPLIFY MODE ===\n";
        
        ProceduralObjectCollection input_collection;
        if (!input_collection.LoadFromFile(args.input_file)) {
            std::cout << "❌ Failed to load input PSF file\n";
            return 1;
        }
        
        // Analyze first
        AnalyzeSimplificationPotential(input_collection, args.verbose);
        
        // Ultra-aggressive simplification
        ProceduralObjectCollection ultra_simplified = UltraSimplifyPSF(input_collection, args.verbose);
        
        if (!ultra_simplified.SaveToFile(args.output_file)) {
            std::cout << "❌ Failed to save ultra-simplified PSF file\n";
            return 1;
        }
        
        std::cout << "✅ Ultra-aggressive simplification complete!\n";
        
    } else if (args.analyze_mode) {
        std::cout << "=== ANALYSIS MODE ===\n";
        
        ProceduralObjectCollection collection;
        if (!collection.LoadFromFile(args.input_file)) {
            std::cout << "❌ Failed to load PSF file\n";
            return 1;
        }
        
        AnalyzeSimplificationPotential(collection, true);
    }  else {
        // Assume FBX input
        std::string ext = args.input_file.substr(args.input_file.find_last_of('.') + 1);
        if (ext != "fbx" && ext != "FBX") {
            std::cout << "❌ Input file must be .fbx for direct conversion\n";
            std::cout << "Supported modes:\n";
            std::cout << "  --test        Create test PSF\n";
            std::cout << "  --convert     PSF to OBJ\n";
            std::cout << "  --simplify    Simplify PSF\n";
            std::cout << "  --ultra-simplify Ultra-aggressive simplification\n";
            std::cout << "  --analyze     Analyze PSF\n";
            std::cout << "  [file.fbx]    Convert FBX to PSF\n";
            return 1;
        }
        
        std::cout << "=== FBX TO PSF CONVERSION ===\n";
        
        if (!MeshAwareFBXConverter::ConvertFBXToPSFEnhanced(args.input_file, args.output_file, args.verbose)) {
            std::cout << "❌ Failed to convert FBX to PSF\n";
            std::cout << "\nTroubleshooting:\n";
            std::cout << "1. Check if FBX file exists and is readable\n";
            std::cout << "2. Ensure FBX file is not corrupted\n";
            std::cout << "3. Try with a simpler FBX file first\n";

            std::cout << "4. FBX SDK is not available - recompile with FBX SDK support\n";

            return 1;
        }
        
        if (args.verbose) {
            VerifyPSF(args.output_file, true);
        }
        
        std::cout << "✅ FBX to PSF conversion successful\n";
        std::cout << "Output file: " << args.output_file << "\n";
        
        // Optional: Show conversion statistics
        if (args.verbose) {
            std::ifstream psf_check(args.output_file, std::ios::ate);
            if (psf_check.is_open()) {
                auto file_size = psf_check.tellg();
                std::cout << "PSF file size: " << file_size << " bytes\n";
                psf_check.close();
                
                // Quick analysis
                ProceduralObjectCollection result_collection;
                if (result_collection.LoadFromFile(args.output_file)) {
                    float compression_estimate = 0.0f;
                    if (result_collection.surface_pool.size() > 0) {
                        // Rough estimate: each surface represents ~2 triangles worth of data
                        size_t estimated_triangles = result_collection.surface_pool.size() * 2;
                        size_t estimated_obj_size = estimated_triangles * 100; // ~100 bytes per triangle in OBJ
                        compression_estimate = static_cast<float>(estimated_obj_size) / file_size;
                    }
                    
                    std::cout << "Conversion summary:\n";
                    std::cout << "  Anchors: " << result_collection.anchor_pool.size() << "\n";
                    std::cout << "  Surfaces: " << result_collection.surface_pool.size() << "\n";
                    if (compression_estimate > 1.0f) {
                        std::cout << "  Estimated compression: " << compression_estimate << "x\n";
                    }
                }
            }
        }
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "\nExecution time: " << duration.count() << " ms\n";
    
    return 0;
}






