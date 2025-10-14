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

#ifndef NO_FBX_SDK
#include <fbxsdk.h>
#endif
#include <functional>
#include <iomanip>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Your dependencies
#include "psf_realtime_renderer.h"
#include "psf_opengl_viewer.h"
#include "object_descriptor_buffers.h"
#include "mathematical_color_evaluator.h"
#include "surface_buffers.h"
#include "math_types.h"
#include "surface_buffer_evaluator.h"
#include "anchorpoint_evaluator.h"
#include "bone_evaluator.h"
#include "psf_realtime_renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"



struct EnhancedMeshData {
    std::string name;
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<std::vector<int>> faces;
    std::vector<Vec3> colors;
    void AddTriangle(int v1, int v2, int v3) {
        faces.push_back({v1, v2, v3});
    }
    
    void AddQuad(int v1, int v2, int v3, int v4) {
        faces.push_back({v1, v2, v3, v4});
    }
};


struct RealTextureInfo {
    std::string filename;
    std::string uvSetName;
    FbxFileTexture* fbxTexture;
    bool isValid;
    
    // Actual image data
    unsigned char* imageData;
    int width;
    int height;
    int channels;
    
    RealTextureInfo() : fbxTexture(nullptr), isValid(false), imageData(nullptr), width(0), height(0), channels(0) {}
    
    ~RealTextureInfo() {
        if (imageData) {
            stbi_image_free(imageData);
            imageData = nullptr;
        }
    }
    
    // Move constructor for std::vector compatibility
    RealTextureInfo(RealTextureInfo&& other) noexcept
        : filename(std::move(other.filename))
        , uvSetName(std::move(other.uvSetName))
        , fbxTexture(other.fbxTexture)
        , isValid(other.isValid)
        , imageData(other.imageData)
        , width(other.width)
        , height(other.height)
        , channels(other.channels)
    {
        other.imageData = nullptr;
        other.fbxTexture = nullptr;
        other.isValid = false;
    }
    
    // Move assignment operator
    RealTextureInfo& operator=(RealTextureInfo&& other) noexcept {
        if (this != &other) {
            // Clean up existing data
            if (imageData) {
                stbi_image_free(imageData);
            }
            
            // Move data
            filename = std::move(other.filename);
            uvSetName = std::move(other.uvSetName);
            fbxTexture = other.fbxTexture;
            isValid = other.isValid;
            imageData = other.imageData;
            width = other.width;
            height = other.height;
            channels = other.channels;
            
            // Reset other
            other.imageData = nullptr;
            other.fbxTexture = nullptr;
            other.isValid = false;
        }
        return *this;
    }
    
    // Delete copy constructor and assignment to prevent issues
    RealTextureInfo(const RealTextureInfo&) = delete;
    RealTextureInfo& operator=(const RealTextureInfo&) = delete;
    
bool LoadImage(const std::string& fbxPath) {
    if (filename.empty()) {
        return false;
    }
    
    std::vector<std::string> pathsToTry;
    
    // Extract just the filename without path
    std::string baseFilename = filename;
    size_t lastSlash = baseFilename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseFilename = baseFilename.substr(lastSlash + 1);
    }
    
    // Strategy 1: Try the exact path as stored in FBX
    pathsToTry.push_back(filename);
    
    // Strategy 2: Try relative to FBX file directory
    if (!fbxPath.empty()) {
        size_t fbxLastSlash = fbxPath.find_last_of("/\\");
        if (fbxLastSlash != std::string::npos) {
            std::string fbxDir = fbxPath.substr(0, fbxLastSlash + 1);
            pathsToTry.push_back(fbxDir + baseFilename);
            pathsToTry.push_back(fbxDir + filename); // In case filename has subdirs
        }
    }
    
    // Strategy 3: Try just the base filename in current directory
    pathsToTry.push_back(baseFilename);
    
    // Strategy 4: Try common texture subdirectories relative to FBX
    if (!fbxPath.empty()) {
        size_t fbxLastSlash = fbxPath.find_last_of("/\\");
        if (fbxLastSlash != std::string::npos) {
            std::string fbxDir = fbxPath.substr(0, fbxLastSlash + 1);
            std::vector<std::string> commonDirs = {
                "textures/", "Textures/", "TEXTURES/",
                "images/", "Images/", "IMAGES/",
                "materials/", "Materials/", "MATERIALS/",
                "assets/", "Assets/", "ASSETS/",
                "../textures/", "../Textures/",
                "../images/", "../Images/"
            };
            
            for (const std::string& dir : commonDirs) {
                pathsToTry.push_back(fbxDir + dir + baseFilename);
            }
        }
    }
    
    // Strategy 5: Try case variations of the filename
    std::string lowerFilename = baseFilename;
    std::string upperFilename = baseFilename;
    std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
    std::transform(upperFilename.begin(), upperFilename.end(), upperFilename.begin(), ::toupper);
    
    if (lowerFilename != baseFilename) {
        pathsToTry.push_back(lowerFilename);
        if (!fbxPath.empty()) {
            size_t fbxLastSlash = fbxPath.find_last_of("/\\");
            if (fbxLastSlash != std::string::npos) {
                std::string fbxDir = fbxPath.substr(0, fbxLastSlash + 1);
                pathsToTry.push_back(fbxDir + lowerFilename);
            }
        }
    }
    
    if (upperFilename != baseFilename) {
        pathsToTry.push_back(upperFilename);
        if (!fbxPath.empty()) {
            size_t fbxLastSlash = fbxPath.find_last_of("/\\");
            if (fbxLastSlash != std::string::npos) {
                std::string fbxDir = fbxPath.substr(0, fbxLastSlash + 1);
                pathsToTry.push_back(fbxDir + upperFilename);
            }
        }
    }
    
    // Try to load from each path
    for (size_t i = 0; i < pathsToTry.size(); ++i) {
        const std::string& path = pathsToTry[i];
        
        // Skip duplicates
        if (i > 0 && path == pathsToTry[i-1]) continue;
        
        std::cout << "  Trying path " << (i+1) << "/" << pathsToTry.size() << ": " << path << std::endl;
        
        // Check if file exists first (optional, but helpful for debugging)
        std::ifstream testFile(path);
        if (!testFile.good()) {
            std::cout << "    File not found or not readable" << std::endl;
            continue;
        }
        testFile.close();
        
        imageData = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (imageData) {
            std::cout << "    ✅ SUCCESS! Loaded texture: " << path << std::endl;
            std::cout << "       Size: " << width << "x" << height << ", Channels: " << channels << std::endl;
            isValid = true;
            return true;
        } else {
            std::cout << "    ❌ stbi_load failed: " << stbi_failure_reason() << std::endl;
        }
    }
    
    std::cout << "  ❌ All texture loading attempts failed for: " << filename << std::endl;
    isValid = false;
    return false;
}

    
 Vec3 SampleColor(float u, float v) const {
        if (verbose_debug) {
            std::cout << "SampleColor called with UV(" << u << ", " << v << ")\n";
            std::cout << "  imageData: " << (imageData ? "valid" : "null") << "\n";
            std::cout << "  dimensions: " << width << "x" << height << " channels: " << channels << "\n";
        }
        
        if (!imageData || width <= 0 || height <= 0) {
            if (verbose_debug) {
                std::cout << "  Returning gray fallback (no image data)\n";
            }
            return Vec3(0.8f, 0.6f, 0.4f); // Change to warmer fallback instead of gray
        }
        
        // Wrap UV coordinates for tiling
        u = u - floor(u);
        v = v - floor(v);
        
        // Convert UV to pixel coordinates
        int x = static_cast<int>(u * width);
        int y = static_cast<int>((1.0f - v) * height);
        
        // Clamp to image bounds
        x = std::max(0, std::min(width - 1, x));
        y = std::max(0, std::min(height - 1, y));
        
        // Get pixel data
        int pixelIndex = (y * width + x) * channels;
        
        if (pixelIndex >= 0 && pixelIndex + 2 < width * height * channels && channels >= 3) {
            float r = imageData[pixelIndex + 0] / 255.0f;
            float g = imageData[pixelIndex + 1] / 255.0f;
            float b = imageData[pixelIndex + 2] / 255.0f;
            
            if (verbose_debug && x < 5 && y < 5) {
                std::cout << "  Pixel(" << x << "," << y << ") RGB: " << r << "," << g << "," << b << "\n";
            }
            
            return Vec3(r, g, b);
        }
        
        if (verbose_debug) {
            std::cout << "  Returning gray fallback (invalid pixel access)\n";
        }
        return Vec3(0.7f, 0.5f, 0.3f); // Warmer fallback
    }
    
    static bool verbose_debug; 
};
bool RealTextureInfo::verbose_debug = false;

// Enhanced material color info with real texture loading
struct RealMaterialColorInfo {
    Vec3 diffuse;
    Vec3 ambient;
    Vec3 specular;
    Vec3 emissive;
    float transparency;
    std::vector<RealTextureInfo> textures;
    
    RealMaterialColorInfo() : 
        diffuse(0.7f, 0.7f, 0.7f), 
        ambient(0.2f, 0.2f, 0.2f),
        specular(1.0f, 1.0f, 1.0f),
        emissive(0.0f, 0.0f, 0.0f),
        transparency(0.0f) {}
};

// REMOVE the duplicate SampleColor function that was causing the error
// The SampleColor method is now properly inside the RealTextureInfo struct

// REMOVE this duplicate struct definition if it exists in your code:
/*
Vec3 SampleColor(float u, float v) const {
    // This was the problematic code - it should be inside RealTextureInfo struct
}
*/

// Keep only the procedural texture sampling function (rename to avoid conflicts)
Vec3 SampleProceduralTextureColor(const std::string& filename, float u, float v) {
    // Normalize UV coordinates
    u = u - floor(u); // Keep fractional part for tiling
    v = v - floor(v);
    
    // Check filename for hints about texture type
    std::string fname = filename;
    std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
    
    Vec3 color(0.5f, 0.5f, 0.5f);
    
    if (fname.find("wood") != std::string::npos || fname.find("bark") != std::string::npos) {
        // Wood-like pattern
        float grain = sin(u * 20.0f) * 0.1f + sin(v * 40.0f) * 0.05f;
        color = Vec3(0.6f + grain, 0.4f + grain * 0.5f, 0.2f + grain * 0.3f);
    }
    else if (fname.find("metal") != std::string::npos || fname.find("steel") != std::string::npos) {
        // Metallic pattern
        float noise = sin(u * 50.0f + v * 30.0f) * 0.1f;
        color = Vec3(0.7f + noise, 0.7f + noise, 0.8f + noise);
    }
    else if (fname.find("stone") != std::string::npos || fname.find("rock") != std::string::npos) {
        // Stone-like pattern
        float noise1 = sin(u * 15.0f) * sin(v * 18.0f) * 0.2f;
        float noise2 = sin(u * 35.0f + v * 25.0f) * 0.1f;
        color = Vec3(0.6f + noise1, 0.55f + noise1, 0.5f + noise1 + noise2);
    }
    else if (fname.find("fabric") != std::string::npos || fname.find("cloth") != std::string::npos) {
        // Fabric pattern
        float weave = (sin(u * 40.0f) * sin(v * 40.0f)) * 0.15f;
        color = Vec3(0.4f + weave, 0.3f + weave, 0.5f + weave);
    }
    else {
        // Generic texture pattern - checkerboard with gradient
        int checkU = static_cast<int>(u * 8.0f);
        int checkV = static_cast<int>(v * 8.0f);
        float checker = ((checkU + checkV) % 2) * 0.3f + 0.4f;
        
        color = Vec3(
            checker * (0.3f + u * 0.4f),
            checker * (0.3f + v * 0.4f),
            checker * (0.4f + (1.0f - u * v) * 0.3f)
        );
    }
    
    // Clamp to valid range
    color.x = std::max(0.0f, std::min(1.0f, color.x));
    color.y = std::max(0.0f, std::min(1.0f, color.y));
    color.z = std::max(0.0f, std::min(1.0f, color.z));
    
    return color;
}








void SafeEvaluateColorFromBuffer(const ProceduralSurfaceBuffer* surface, float u, float v, float result[3]) {
    // Clamp UV coordinates to safe range
    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
    
    // Evaluate each channel separately with bounds checking
    for (int channel = 0; channel < 3; ++channel) {
        ColorEquationType equation;
        uint8_t param;
        
        if (channel == 0) {
            equation = static_cast<ColorEquationType>(surface->color_eqn_r);
            param = surface->color_param_r;
        } else if (channel == 1) {
            equation = static_cast<ColorEquationType>(surface->color_eqn_g);
            param = surface->color_param_g;
        } else {
            equation = static_cast<ColorEquationType>(surface->color_eqn_b);
            param = surface->color_param_b;
        }
        
        float color_value = 0.5f; // Safe default
        
        try {
            // Use the mathematical evaluator but with safety checks
            color_value = MathematicalColorEvaluator::EvaluateColorChannel(equation, param, u, v);
            
            // Critical bounds checking
            if (!std::isfinite(color_value) || color_value < 0.0f || color_value > 1.0f) {
                // Fallback to parameter-based color
                color_value = static_cast<float>(param) / 255.0f;
            }
        } catch (...) {
            // If mathematical evaluation fails, use parameter directly
            color_value = static_cast<float>(param) / 255.0f;
        }
        
        // Final safety clamp
        result[channel] = std::max(0.0f, std::min(1.0f, color_value));
    }
}

class SmartMeshConverter {
public:
    static ProceduralObjectCollection ConvertMeshToProcedural(const EnhancedMeshData& meshData, bool verbose);
    
private:
    static Vec3 WorldToPSF(const Vec3& world_pos, const Vec3& min_pos, const Vec3& max_pos) {
        Vec3 size = Vec3(max_pos.x - min_pos.x, max_pos.y - min_pos.y, max_pos.z - min_pos.z);
        
        // CRITICAL FIX: Use the maximum dimension to maintain aspect ratio
        float max_dimension = std::max({size.x, size.y, size.z});
        
        if (max_dimension < 1e-6f) {
            return Vec3(0, 0, 0);
        }
        
        // Calculate center point
        Vec3 center = Vec3(
            (min_pos.x + max_pos.x) * 0.5f,
            (min_pos.y + max_pos.y) * 0.5f,
            (min_pos.z + max_pos.z) * 0.5f
        );
        
        // FIXED: Normalize based on max dimension to preserve aspect ratio
        Vec3 normalized = Vec3(
            (world_pos.x - center.x) / max_dimension,
            (world_pos.y - center.y) / max_dimension,
            (world_pos.z - center.z) / max_dimension
        );
        
        // Scale to PSF coordinate range [-1.8, 1.8] (safe range within [-2, 2])
        normalized.x *= 1.8f;
        normalized.y *= 1.8f;
        normalized.z *= 1.8f;
        
        // Clamp to safe range
        normalized.x = std::max(-1.8f, std::min(1.8f, normalized.x));
        normalized.y = std::max(-1.8f, std::min(1.8f, normalized.y));
        normalized.z = std::max(-1.8f, std::min(1.8f, normalized.z));
        
        return normalized;
    }
};

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
    bool view_mode = false;
    bool gl_view_mode = false;
    bool color_aware_mode = false;
    bool color_preserve_mode = false;      // Strict color preservation
    bool debug_colors = false;             // Detailed color debugging
    bool no_simplify_colors = false;       // Skip color-based grouping
    bool force_original_colors = false;
    float color_tolerance = 0.10f;
    float angle_threshold = 5.0f;
    float plane_tolerance = 0.01f;
    
    bool ParseEnhanced(int argc, char* argv[]);
};

// FIXED GEOMETRY FUNCTIONS
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

static bool AreNormalsSimilar(const Vec3& n1, const Vec3& n2, float angleThresholdDegrees) {
    float dot = n1.x * n2.x + n1.y * n2.y + n1.z * n2.z;
    dot = std::max(-1.0f, std::min(1.0f, dot));
    float angle = acos(dot) * 180.0f / static_cast<float>(M_PI);
    return angle < angleThresholdDegrees;
}

static bool IsPointOnPlane(const Vec3& point, const float params[8], float tolerance) {
    float a = params[0], b = params[1], c = params[2], d = params[3];
    float distance = fabs(a * point.x + b * point.y + c * point.z + d);
    return distance < tolerance;
}

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

// FIXED PLANE CALCULATION
static void CalculateBestFitPlane(const std::vector<Vec3>& positions, float params[8]) {
    if (positions.size() < 3) {
        params[0] = 0.0f; params[1] = 0.0f; params[2] = 1.0f; params[3] = 0.0f;
        params[4] = params[5] = params[6] = params[7] = 0.0f;
        return;
    }
    
    Vec3 centroid = CalculateCentroid(positions);
    
    // Use first three valid points to calculate plane
    Vec3 edge1 = Vec3(positions[1].x - positions[0].x, 
                     positions[1].y - positions[0].y, 
                     positions[1].z - positions[0].z);
    Vec3 edge2 = Vec3(positions[2].x - positions[0].x, 
                     positions[2].y - positions[0].y, 
                     positions[2].z - positions[0].z);
    
    // Cross product for normal
    Vec3 normal;
    normal.x = edge1.y * edge2.z - edge1.z * edge2.y;
    normal.y = edge1.z * edge2.x - edge1.x * edge2.z;
    normal.z = edge1.x * edge2.y - edge1.y * edge2.x;
    
    // Normalize
    float length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length > 1e-6f) {
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
    } else {
        normal = Vec3(0.0f, 0.0f, 1.0f);
    }
    
    // Calculate D parameter using centroid
    float d = -(normal.x * centroid.x + normal.y * centroid.y + normal.z * centroid.z);
    
    params[0] = normal.x;
    params[1] = normal.y;
    params[2] = normal.z;
    params[3] = d;
    params[4] = params[5] = params[6] = params[7] = 0.0f;
}

// FIXED ANCHOR SELECTION
static void SelectOptimalAnchors(const std::vector<Vec3>& positions, 
                               const std::vector<uint16_t>& anchor_indices, 
                               uint16_t selected[4], bool verbose = false) {
    if (positions.empty() || anchor_indices.empty()) {
        selected[0] = selected[1] = selected[2] = selected[3] = 0;
        return;
    }
    
    if (positions.size() <= 4) {
        for (size_t i = 0; i < positions.size(); ++i) {
            selected[i] = anchor_indices[i];
        }
        for (size_t i = positions.size(); i < 4; ++i) {
            selected[i] = anchor_indices[0];
        }
        return;
    }
    
    // Find corner points that best represent the surface
    std::pair<Vec3, Vec3> bbox = FindBoundingBox(positions);
    Vec3 center = Vec3((bbox.first.x + bbox.second.x) * 0.5f,
                       (bbox.first.y + bbox.second.y) * 0.5f,
                       (bbox.first.z + bbox.second.z) * 0.5f);
    
    // Find points closest to bbox corners
    std::vector<Vec3> target_corners = {
        Vec3(bbox.first.x, bbox.first.y, bbox.first.z),
        Vec3(bbox.second.x, bbox.first.y, bbox.first.z),
        Vec3(bbox.second.x, bbox.second.y, bbox.second.z),
        Vec3(bbox.first.x, bbox.second.y, bbox.second.z)
    };
    
    for (int corner = 0; corner < 4; ++corner) {
        float min_dist = std::numeric_limits<float>::max();
        int best_idx = 0;
        
        for (size_t i = 0; i < positions.size(); ++i) {
            float dist = CalculateDistanceBetweenPoints(positions[i], target_corners[corner]);
            if (dist < min_dist) {
                min_dist = dist;
                best_idx = static_cast<int>(i);
            }
        }
        
        selected[corner] = anchor_indices[best_idx];
    }
    
    if (verbose) {
        std::cout << "Selected optimal anchors: [" << selected[0] << ", " 
                  << selected[1] << ", " << selected[2] << ", " << selected[3] << "]\n";
    }
}

// FIXED COLOR-AWARE SURFACE CREATION
static void CreateOptimizedSurfaceFromGroup(const std::vector<size_t>& group, 
                                           const ProceduralObjectCollection& input_collection, 
                                           ProceduralObjectCollection& output_collection, 
                                           bool verbose) {
    
    if (verbose) {
        std::cout << "Creating color-preserving optimized surface from group of " << group.size() << " surfaces\n";
    }
    
    // Collect unique anchor points from this group
    std::set<uint16_t> group_anchors;
    for (size_t idx : group) {
        if (idx < input_collection.surface_pool.size()) {
            const auto& surface = input_collection.surface_pool[idx];
            for (int i = 0; i < 4; ++i) {
                if (surface.anchor_point_indices[i] < input_collection.anchor_pool.size()) {
                    group_anchors.insert(surface.anchor_point_indices[i]);
                }
            }
        }
    }
    
    if (group_anchors.size() < 4) {
        if (verbose) {
            std::cout << "⚠️ Warning: Group has fewer than 4 anchors, using available anchors\n";
        }
    }
    
    // Convert to vectors for processing
    std::vector<uint16_t> anchor_indices(group_anchors.begin(), group_anchors.end());
    std::vector<Vec3> positions;
    for (uint16_t anchor_idx : anchor_indices) {
        Vec3 pos = AnchorPointEvaluator::DecodePosition(
            input_collection.anchor_pool[anchor_idx].pos_xyz
        );
        positions.push_back(pos);
    }
    
    // Select optimal anchors
    uint16_t selected_anchors[4] = {0};
    SelectOptimalAnchors(positions, anchor_indices, selected_anchors, verbose);
    
    // Calculate plane parameters from all positions
    float params[8] = {0};
    CalculateBestFitPlane(positions, params);
    
    // CRITICAL: Analyze and preserve color information mathematically
    std::map<ColorEquationType, int> r_freq, g_freq, b_freq;
    std::vector<uint8_t> r_params, g_params, b_params;
    std::vector<Vec3> sample_colors;
    
    // Sample colors from all surfaces in the group using mathematical evaluation
    for (size_t idx : group) {
        if (idx >= input_collection.surface_pool.size()) continue;
        
        const auto& surface = input_collection.surface_pool[idx];
        
        // Count color equation frequencies
        r_freq[static_cast<ColorEquationType>(surface.color_eqn_r)]++;
        g_freq[static_cast<ColorEquationType>(surface.color_eqn_g)]++;
        b_freq[static_cast<ColorEquationType>(surface.color_eqn_b)]++;
        
        r_params.push_back(surface.color_param_r);
        g_params.push_back(surface.color_param_g);
        b_params.push_back(surface.color_param_b);
        
        // Sample color at surface center using mathematical evaluator
        float sample_rgb[3];
        MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, 0.5f, 0.5f, sample_rgb);
        sample_colors.push_back(Vec3(sample_rgb[0], sample_rgb[1], sample_rgb[2]));
        
        if (verbose && idx == group[0]) {
            std::cout << "  Original color equations: R=" << static_cast<int>(surface.color_eqn_r) 
                      << " G=" << static_cast<int>(surface.color_eqn_g) 
                      << " B=" << static_cast<int>(surface.color_eqn_b) << "\n";
            std::cout << "  Sample color: (" << sample_rgb[0] << ", " << sample_rgb[1] << ", " << sample_rgb[2] << ")\n";
        }
    }
    
    // Create the optimized surface
    ProceduralSurfaceBuffer surface = SurfaceBufferUtils::CreateWithAnchors(
        EQUATION_PLANE, selected_anchors, params, MOD_COLOR);
    
    // MATHEMATICAL COLOR PRESERVATION: Find dominant color equations
    ColorEquationType dominant_r = std::max_element(r_freq.begin(), r_freq.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; })->first;
    ColorEquationType dominant_g = std::max_element(g_freq.begin(), g_freq.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; })->first;
    ColorEquationType dominant_b = std::max_element(b_freq.begin(), b_freq.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; })->first;
    
    // Calculate average color from mathematical samples
    Vec3 avg_color(0.0f, 0.0f, 0.0f);
    if (!sample_colors.empty()) {
        for (const Vec3& color : sample_colors) {
            avg_color.x += color.x;
            avg_color.y += color.y;
            avg_color.z += color.z;
        }
        avg_color.x /= sample_colors.size();
        avg_color.y /= sample_colors.size();
        avg_color.z /= sample_colors.size();
    }
    
    // Set optimized mathematical color equations
    surface.color_eqn_r = static_cast<uint8_t>(dominant_r);
    surface.color_eqn_g = static_cast<uint8_t>(dominant_g);
    surface.color_eqn_b = static_cast<uint8_t>(dominant_b);
    
    // CRITICAL FIX: Ensure visible colors by enhancing brightness
    if (dominant_r == COLOR_CONSTANT) {
        float enhanced_r = std::max(0.3f, std::min(1.0f, avg_color.x * 1.2f));
        surface.color_param_r = ColorParameterEncoder::EncodeNormalized(enhanced_r);
    } else {
        // Use average of original parameters, but ensure minimum brightness
        uint32_t sum_r = 0;
        for (uint8_t param : r_params) sum_r += param;
        uint8_t avg_param = static_cast<uint8_t>(sum_r / r_params.size());
        surface.color_param_r = std::max(static_cast<uint8_t>(77), avg_param); // Minimum 30% brightness
    }
    
    if (dominant_g == COLOR_CONSTANT) {
        float enhanced_g = std::max(0.3f, std::min(1.0f, avg_color.y * 1.2f));
        surface.color_param_g = ColorParameterEncoder::EncodeNormalized(enhanced_g);
    } else {
        uint32_t sum_g = 0;
        for (uint8_t param : g_params) sum_g += param;
        uint8_t avg_param = static_cast<uint8_t>(sum_g / g_params.size());
        surface.color_param_g = std::max(static_cast<uint8_t>(77), avg_param);
    }
    
    if (dominant_b == COLOR_CONSTANT) {
        float enhanced_b = std::max(0.3f, std::min(1.0f, avg_color.z * 1.2f));
        surface.color_param_b = ColorParameterEncoder::EncodeNormalized(enhanced_b);
    } else {
        uint32_t sum_b = 0;
        for (uint8_t param : b_params) sum_b += param;
        uint8_t avg_param = static_cast<uint8_t>(sum_b / b_params.size());
        surface.color_param_b = std::max(static_cast<uint8_t>(77), avg_param);
    }
    
    // ADDITIONAL DEBUG: Test the created surface immediately
    if (verbose) {
        float verification_color[3];
        MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, 0.5f, 0.5f, verification_color);
        std::cout << "  VERIFICATION: Created surface evaluates to color (" 
                  << verification_color[0] << ", " << verification_color[1] << ", " << verification_color[2] << ")\n";
        
        if (verification_color[0] < 0.1f && verification_color[1] < 0.1f && verification_color[2] < 0.1f) {
            std::cout << "  ⚠️  WARNING: Surface color is too dark! Applying emergency color boost.\n";
            // Emergency color boost
            surface.color_eqn_r = surface.color_eqn_g = surface.color_eqn_b = COLOR_CONSTANT;
            surface.color_param_r = 180; // 70% brightness
            surface.color_param_g = 160; // 63% brightness  
            surface.color_param_b = 140; // 55% brightness
        }
    }
    
    // Preserve material properties
    if (!group.empty() && group[0] < input_collection.surface_pool.size()) {
        const auto& first_surface = input_collection.surface_pool[group[0]];
        surface.roughness = first_surface.roughness;
        surface.metallic = first_surface.metallic;
        surface.emissive_strength = first_surface.emissive_strength;
        surface.specular_tint = first_surface.specular_tint;
    }
    
    output_collection.AddSurface(surface);
    
    if (verbose) {
        std::cout << "  ✅ Created optimized surface with preserved mathematical colors\n";
        std::cout << "  Final color equations: R=" << static_cast<int>(surface.color_eqn_r) 
                  << " G=" << static_cast<int>(surface.color_eqn_g) 
                  << " B=" << static_cast<int>(surface.color_eqn_b) << "\n";
        std::cout << "  Final color params: R=" << static_cast<int>(surface.color_param_r)
                  << " G=" << static_cast<int>(surface.color_param_g) 
                  << " B=" << static_cast<int>(surface.color_param_b) << "\n";
        std::cout << "  Average sampled color: (" << avg_color.x << ", " << avg_color.y << ", " << avg_color.z << ")\n";
    }
}

// FIXED SIMPLIFICATION
// SIMPLE FIX: Replace your entire SimplifyPSF function with this single version
// This handles both cases (with and without color tolerance) in one function

ProceduralObjectCollection SimplifyPSF(const ProceduralObjectCollection& input_collection, 
                                      bool verbose, 
                                      float angleThresholdDegrees = 5.0f, 
                                      float planeTolerance = 0.01f,
                                      float colorTolerance = 0.2f) {  // Added default parameter
    ProceduralObjectCollection simplified_collection = input_collection;
    simplified_collection.surface_pool.clear();
    
    if (verbose) {
        std::cout << "=== ENHANCED COLOR-AWARE SURFACE SIMPLIFICATION ===\n";
        std::cout << "Input surfaces: " << input_collection.surface_pool.size() << "\n";
        std::cout << "Angle threshold: " << angleThresholdDegrees << "°\n";
        std::cout << "Plane tolerance: " << planeTolerance << "\n";
        std::cout << "Color tolerance: " << colorTolerance << "\n";
    }
    
    // Build edge-to-surface mapping for connectivity analysis
    std::map<std::pair<uint16_t, uint16_t>, std::vector<size_t>> edge_to_surfaces;
    for (size_t i = 0; i < input_collection.surface_pool.size(); ++i) {
        const auto& surface = input_collection.surface_pool[i];
        for (int j = 0; j < 4; ++j) {
            int k = (j + 1) % 4;
            auto edge = std::minmax(surface.anchor_point_indices[j], surface.anchor_point_indices[k]);
            edge_to_surfaces[edge].push_back(i);
        }
    }

    // Enhanced color compatibility function with perceptual weighting
    auto AreColorsCompatible = [colorTolerance](const ProceduralSurfaceBuffer& surf1, const ProceduralSurfaceBuffer& surf2) -> bool {
        // Sample colors from both surfaces at multiple points for better accuracy
        float total_distance = 0.0f;
        int sample_count = 0;
        
        // Sample at multiple UV coordinates
        std::vector<std::pair<float, float>> sample_points = {
            {0.25f, 0.25f}, {0.75f, 0.25f}, {0.25f, 0.75f}, {0.75f, 0.75f}, {0.5f, 0.5f}
        };
        
        for (const auto& uv : sample_points) {
            float color1[3], color2[3];
            MathematicalColorEvaluator::EvaluateColorFromBuffer(&surf1, uv.first, uv.second, color1);
            MathematicalColorEvaluator::EvaluateColorFromBuffer(&surf2, uv.first, uv.second, color2);
            
            // Calculate perceptual color distance (weighted for human vision)
            float dr = color1[0] - color2[0];
            float dg = color1[1] - color2[1];
            float db = color1[2] - color2[2];
            
            // Perceptual weighting: green is more prominent, then red, then blue
            float perceptual_distance = sqrt(0.3f * dr * dr + 0.59f * dg * dg + 0.11f * db * db);
            total_distance += perceptual_distance;
            sample_count++;
        }
        
        float avg_distance = total_distance / sample_count;
        return avg_distance < colorTolerance;
    };

    // Find groups of compatible surfaces
    std::vector<bool> used(input_collection.surface_pool.size(), false);
    std::vector<std::vector<size_t>> surface_groups;

    for (size_t i = 0; i < input_collection.surface_pool.size(); ++i) {
        if (used[i]) continue;
        
        std::vector<size_t> group = {i};
        used[i] = true;
        
        // Get reference normal from first surface
        const float* params = SurfaceBufferUtils::ExtractParameters(&input_collection.surface_pool[i]);
        Vec3 reference_normal = Vec3(params[0], params[1], params[2]);
        float ref_length = sqrt(reference_normal.x * reference_normal.x + 
                               reference_normal.y * reference_normal.y + 
                               reference_normal.z * reference_normal.z);
        if (ref_length > 1e-6f) {
            reference_normal.x /= ref_length;
            reference_normal.y /= ref_length;
            reference_normal.z /= ref_length;
        }
        
        // Enhanced grouping logic with strict color compatibility
        std::vector<size_t> to_check = {i};
        while (!to_check.empty()) {
            size_t curr = to_check.back();
            to_check.pop_back();
            
            // Check all edges of current surface
            for (int j = 0; j < 4; ++j) {
                int k = (j + 1) % 4;
                auto edge = std::minmax(input_collection.surface_pool[curr].anchor_point_indices[j],
                                       input_collection.surface_pool[curr].anchor_point_indices[k]);
                
                auto edge_it = edge_to_surfaces.find(edge);
                if (edge_it != edge_to_surfaces.end()) {
                    for (size_t neighbor : edge_it->second) {
                        if (!used[neighbor] && neighbor < input_collection.surface_pool.size()) {
                            // Check geometric compatibility
                            const float* neighbor_params = SurfaceBufferUtils::ExtractParameters(&input_collection.surface_pool[neighbor]);
                            Vec3 neighbor_normal = Vec3(neighbor_params[0], neighbor_params[1], neighbor_params[2]);
                            float n_length = sqrt(neighbor_normal.x * neighbor_normal.x + 
                                                 neighbor_normal.y * neighbor_normal.y + 
                                                 neighbor_normal.z * neighbor_normal.z);
                            if (n_length > 1e-6f) {
                                neighbor_normal.x /= n_length;
                                neighbor_normal.y /= n_length;
                                neighbor_normal.z /= n_length;
                            }
                            
                            bool geometric_compatible = AreNormalsSimilar(reference_normal, neighbor_normal, angleThresholdDegrees);
                            
                            if (geometric_compatible) {
                                // Check coplanarity
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
                                    // CRITICAL: Enhanced color compatibility check using the parameter
                                    bool color_compatible = AreColorsCompatible(input_collection.surface_pool[i], 
                                                                               input_collection.surface_pool[neighbor]);
                                    
                                    if (color_compatible) {
                                        group.push_back(neighbor);
                                        used[neighbor] = true;
                                        to_check.push_back(neighbor);
                                        
                                        if (verbose && group.size() <= 5) {
                                            std::cout << "Grouped surfaces " << i << " and " << neighbor << " (geometry + color compatible)\n";
                                        }
                                    } else if (verbose) {
                                        // Sample and display the colors that failed compatibility
                                        float color_i[3], color_neighbor[3];
                                        MathematicalColorEvaluator::EvaluateColorFromBuffer(&input_collection.surface_pool[i], 0.5f, 0.5f, color_i);
                                        MathematicalColorEvaluator::EvaluateColorFromBuffer(&input_collection.surface_pool[neighbor], 0.5f, 0.5f, color_neighbor);
                                        
                                        float color_distance = sqrt((color_i[0] - color_neighbor[0]) * (color_i[0] - color_neighbor[0]) +
                                                                   (color_i[1] - color_neighbor[1]) * (color_i[1] - color_neighbor[1]) +
                                                                   (color_i[2] - color_neighbor[2]) * (color_i[2] - color_neighbor[2]));
                                        
                                        std::cout << "Surfaces " << i << " and " << neighbor << " geometrically compatible but colors differ: "
                                                  << "(" << color_i[0] << "," << color_i[1] << "," << color_i[2] << ") vs "
                                                  << "(" << color_neighbor[0] << "," << color_neighbor[1] << "," << color_neighbor[2] << ") "
                                                  << "distance=" << color_distance << " > tolerance=" << colorTolerance << "\n";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        surface_groups.push_back(group);
    }

    if (verbose) {
        std::cout << "Found " << surface_groups.size() << " color-compatible surface groups\n";
        
        // Show group statistics
        size_t single_surface_groups = 0;
        size_t largest_group_size = 0;
        for (const auto& group : surface_groups) {
            if (group.size() == 1) single_surface_groups++;
            largest_group_size = std::max(largest_group_size, group.size());
        }
        std::cout << "  Single-surface groups: " << single_surface_groups << "\n";
        std::cout << "  Largest group size: " << largest_group_size << "\n";
    }

    // Create optimized surfaces for each group
    for (const auto& group : surface_groups) {
        CreateOptimizedSurfaceFromGroup(group, input_collection, simplified_collection, verbose);
    }

    // Compact anchor pool (same as before)
    std::set<uint16_t> used_anchors;
    for (const auto& surface : simplified_collection.surface_pool) {
        for (int i = 0; i < 4; ++i) {
            used_anchors.insert(surface.anchor_point_indices[i]);
        }
    }

    std::vector<AnchorPointBuffer> new_anchor_pool;
    std::map<uint16_t, uint16_t> old_to_new_index;
    for (uint16_t old_index : used_anchors) {
        if (old_index < input_collection.anchor_pool.size()) {
            old_to_new_index[old_index] = static_cast<uint16_t>(new_anchor_pool.size());
            new_anchor_pool.push_back(input_collection.anchor_pool[old_index]);
        }
    }

    // Update surface anchor indices
    for (auto& surface : simplified_collection.surface_pool) {
        for (int i = 0; i < 4; ++i) {
            auto it = old_to_new_index.find(surface.anchor_point_indices[i]);
            if (it != old_to_new_index.end()) {
                surface.anchor_point_indices[i] = it->second;
            }
        }
    }

    simplified_collection.anchor_pool = new_anchor_pool;
    simplified_collection.UpdateHeader();

    if (verbose) {
        std::cout << "Enhanced color-aware simplification complete:\n";
        std::cout << "  Input: " << input_collection.surface_pool.size() << " surfaces\n";
        std::cout << "  Output: " << simplified_collection.surface_pool.size() << " surfaces\n";
        if (input_collection.surface_pool.size() > 0) {
            float reduction = (input_collection.surface_pool.size() - simplified_collection.surface_pool.size()) * 100.0f / input_collection.surface_pool.size();
            std::cout << "  Reduction: " << reduction << "%\n";
            
            if (reduction < 10.0f) {
                std::cout << "  ⚠️  Low reduction achieved. Consider:\n";
                std::cout << "     - Increasing --color-tolerance (currently " << colorTolerance << ")\n";
                std::cout << "     - Increasing --angle-threshold (currently " << angleThresholdDegrees << "°)\n";
                std::cout << "     - Using --ultra-simplify for more aggressive reduction\n";
            } else {
                std::cout << "  ✅ Good reduction achieved while preserving colors\n";
            }
        }
    }

    return simplified_collection;
}
// ULTRA SIMPLIFY WITH PROPER ITERATION
ProceduralObjectCollection UltraSimplifyPSF(const ProceduralObjectCollection& input_collection, bool verbose) {
    if (verbose) {
        std::cout << "\n=== ULTRA SIMPLIFICATION ===\n";
        std::cout << "Input: " << input_collection.surface_pool.size() << " surfaces\n";
    }
    
    ProceduralObjectCollection current_collection = input_collection;
    
    // Progressive simplification with relaxed thresholds
    std::vector<std::pair<float, float>> thresholds = {
        {5.0f, 0.01f},   // Conservative first pass
        {15.0f, 0.05f},  // More aggressive second pass
        {30.0f, 0.1f},   // Very aggressive third pass
        {60.0f, 0.2f}    // Ultra aggressive final pass
    };
    
    for (size_t iteration = 0; iteration < thresholds.size(); ++iteration) {
        float angle_threshold = thresholds[iteration].first;
        float plane_tolerance = thresholds[iteration].second;
        
        if (verbose) {
            std::cout << "Ultra pass " << (iteration + 1) << ": angle=" << angle_threshold 
                      << "°, tolerance=" << plane_tolerance << "\n";
        }
        
        ProceduralObjectCollection simplified = SimplifyPSF(current_collection, verbose, angle_threshold, plane_tolerance);
        
        size_t surfaces_before = current_collection.surface_pool.size();
        size_t surfaces_after = simplified.surface_pool.size();
        
        if (verbose) {
            std::cout << "  Result: " << surfaces_before << " → " << surfaces_after << " surfaces\n";
        }
        
        // Stop if no improvement or reaching minimum
        if (surfaces_after >= surfaces_before || surfaces_after <= 1) {
            if (verbose) {
                std::cout << "  No further improvement possible\n";
            }
            break;
        }
        
        current_collection = simplified;
    }
    
    if (verbose) {
        std::cout << "=== ULTRA SIMPLIFICATION COMPLETE ===\n";
        std::cout << "Final: " << current_collection.surface_pool.size() << " surfaces\n";
        if (input_collection.surface_pool.size() > 0) {
            size_t total_reduction = input_collection.surface_pool.size() - current_collection.surface_pool.size();
            float reduction_percentage = (100.0f * total_reduction) / input_collection.surface_pool.size();
            std::cout << "Total reduction: " << total_reduction << " surfaces (" << reduction_percentage << "%)\n";
        }
    }
    
    return current_collection;
}
void VerifyColorPreservation(const std::string& psf_file, bool verbose) {
    if (!verbose) return;
    
    std::cout << "\n=== COLOR PRESERVATION VERIFICATION ===\n";
    
    ProceduralObjectCollection collection;
    if (!collection.LoadFromFile(psf_file)) {
        std::cout << "❌ Failed to load PSF file for verification\n";
        return;
    }
    
    std::cout << "Surfaces: " << collection.surface_pool.size() << "\n";
    
    // Sample colors from first few surfaces
    std::map<ColorEquationType, int> color_equation_usage;
    Vec3 min_color(1.0f, 1.0f, 1.0f), max_color(0.0f, 0.0f, 0.0f);
    Vec3 avg_color(0.0f, 0.0f, 0.0f);
    int color_samples = 0;
    
    for (size_t i = 0; i < std::min<size_t>(collection.surface_pool.size(), 10); ++i) {
        const auto& surface = collection.surface_pool[i];
        
        color_equation_usage[static_cast<ColorEquationType>(surface.color_eqn_r)]++;
        color_equation_usage[static_cast<ColorEquationType>(surface.color_eqn_g)]++;
        color_equation_usage[static_cast<ColorEquationType>(surface.color_eqn_b)]++;
        
        // Sample color at center
        float sample_color[3];
        MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, 0.5f, 0.5f, sample_color);
        
        min_color.x = std::min(min_color.x, sample_color[0]);
        min_color.y = std::min(min_color.y, sample_color[1]);
        min_color.z = std::min(min_color.z, sample_color[2]);
        
        max_color.x = std::max(max_color.x, sample_color[0]);
        max_color.y = std::max(max_color.y, sample_color[1]);
        max_color.z = std::max(max_color.z, sample_color[2]);
        
        avg_color.x += sample_color[0];
        avg_color.y += sample_color[1];
        avg_color.z += sample_color[2];
        color_samples++;
        
        if (i < 5) {
            std::cout << "Surface " << i << " color: (" << sample_color[0] 
                      << ", " << sample_color[1] << ", " << sample_color[2] << ")\n";
        }
    }
    
    if (color_samples > 0) {
        avg_color.x /= color_samples;
        avg_color.y /= color_samples;
        avg_color.z /= color_samples;
        
        std::cout << "Color Range:\n";
        std::cout << "  Min: (" << min_color.x << ", " << min_color.y << ", " << min_color.z << ")\n";
        std::cout << "  Max: (" << max_color.x << ", " << max_color.y << ", " << max_color.z << ")\n";
        std::cout << "  Avg: (" << avg_color.x << ", " << avg_color.y << ", " << avg_color.z << ")\n";
        
        float total_variation = (max_color.x - min_color.x) + 
                               (max_color.y - min_color.y) + 
                               (max_color.z - min_color.z);
        std::cout << "  Total Variation: " << total_variation << "\n";
        
        if (total_variation < 0.1f) {
            std::cout << "  ⚠️  WARNING: Low color variation detected! Colors may be too uniform.\n";
        } else {
            std::cout << "  ✅ Good color variation preserved.\n";
        }
    }
    
    std::cout << "Color Equation Usage:\n";
    for (const auto& pair : color_equation_usage) {
        std::cout << "  " << MathematicalColorEvaluator::GetColorEquationName(pair.first) 
                  << ": " << pair.second << " channels\n";
    }
    
    std::cout << "======================================\n";
}
// Function declarations
bool IsValidInputFile(const std::string& filename, bool& is_fbx, bool& is_psf);
ProceduralObjectCollection CreateTestObject();
bool VerifyPSF(const std::string& psf_file, bool verbose = true);
bool ConvertPSFToOBJ(const std::string& psf_file, const std::string& obj_file, bool verbose, int sample_count);
bool ViewPSFDirectly(const std::string& psf_file, bool verbose);
void ExtractEnhancedMeshFromNode(FbxNode* node, std::vector<EnhancedMeshData>& meshes, 
                               std::vector<FbxNode*>& mesh_nodes, bool verbose);
void ExtractSkeletonFromScene(FbxScene* scene, std::vector<BoneBuffer>& bone_pool, bool verbose);
void LinkVertexSkinning(const std::vector<FbxNode*>& mesh_nodes, 
                       const std::vector<FbxNode*>& skeleton_nodes,
                       std::vector<AnchorPointBuffer>& anchor_pool,
                       const std::map<FbxNode*, uint32_t>& node_to_bone_index,
                       bool verbose);

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
        std::cout << "  View PSF:     " << argv[0] << " --view input.psf [-v]\n";
        std::cout << "  OpenGL View:  " << argv[0] << " --view-gl input.psf [-v]\n";
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
        } else if (arg == "--ultra-simplify") {
            ultra_simplify_mode = true;
        } else if (arg == "--analyze") {
            analyze_mode = true;
        } else if (arg == "--view") {
            view_mode = true;
        } else if (arg == "--view-gl" || arg == "--gl-view") {
            view_mode = true;
            gl_view_mode = true;
        } else if (arg == "--color-simplify") {
            simplify_mode = true;
            color_aware_mode = true;
        } else if (arg == "--color-preserve") {
            color_preserve_mode = true;
            color_aware_mode = true;
        } else if (arg == "--debug-colors") {
            debug_colors = true;
            verbose = true; // Automatically enable verbose for color debugging
        } else if (arg == "--no-simplify-colors") {
            no_simplify_colors = true;
        } else if (arg == "--force-original-colors") {
            force_original_colors = true;
        }else if (arg == "--color-tolerance" && i + 1 < argc) {
            color_tolerance = std::stof(argv[++i]);
        } else if (arg == "--angle-threshold" && i + 1 < argc) {
            angle_threshold = std::stof(argv[++i]);
        } else if (arg == "--plane-tolerance" && i + 1 < argc) {
            plane_tolerance = std::stof(argv[++i]);
        } else if (input_file.empty()) {
            input_file = arg;
        }
    }
    
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

bool IsValidInputFile(const std::string& filename, bool& is_fbx, bool& is_psf) {
    is_fbx = false;
    is_psf = false;
    
    if (filename.empty()) return false;
    
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) return false;
    
    std::string ext = filename.substr(dot_pos + 1);
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

ProceduralObjectCollection CreateTestObject() {
    ProceduralObjectCollection collection;
    collection.object_name = "test_patch";
    collection.creator_info = "Test Generator";
    
    // Create 4 anchor points for a simple quad
    std::vector<Vec3> positions = {
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.1f},
        {-1.0f,  1.0f, 0.1f}
    };
    
    for (size_t i = 0; i < positions.size(); ++i) {
        AnchorPointBuffer anchor;
        memset(&anchor, 0, sizeof(anchor));
        
        AnchorPointEvaluator::EncodePosition(positions[i], anchor.pos_xyz);
        
        // Set normal pointing up
        anchor.normal[0] = static_cast<uint8_t>(127);
        anchor.normal[1] = static_cast<uint8_t>(127);
        anchor.normal[2] = static_cast<uint8_t>(255);
        
        // Set different colors per corner for testing
        switch (i) {
            case 0: anchor.color[0] = 255; anchor.color[1] = 100; anchor.color[2] = 100; break; // Red corner
            case 1: anchor.color[0] = 100; anchor.color[1] = 255; anchor.color[2] = 100; break; // Green corner
            case 2: anchor.color[0] = 100; anchor.color[1] = 100; anchor.color[2] = 255; break; // Blue corner
            case 3: anchor.color[0] = 255; anchor.color[1] = 255; anchor.color[2] = 100; break; // Yellow corner
        }
        anchor.roughness = 128;
        anchor.metallic = 0;
        
        collection.AddAnchor(anchor);
    }
    
    // Create a surface with VISIBLE mathematical colors
    uint16_t anchor_indices[4] = {0, 1, 2, 3};
    float params[8] = {0, 0, 1, 0, 0, 0, 0, 0}; // Simple plane equation z = 0
    
    ProceduralSurfaceBuffer surface = SurfaceBufferUtils::CreateWithAnchors(
        EQUATION_PLANE, anchor_indices, params, MOD_COLOR);
    
    // CRITICAL: Set bright, visible mathematical colors
    surface.color_eqn_r = COLOR_LINEAR_U;     // Red varies with U
    surface.color_eqn_g = COLOR_LINEAR_V;     // Green varies with V  
    surface.color_eqn_b = COLOR_CONSTANT;     // Blue is constant
    
    surface.color_param_r = 200; // 78% brightness for red channel
    surface.color_param_g = 180; // 70% brightness for green channel
    surface.color_param_b = 160; // 63% brightness for blue channel
    
    collection.AddSurface(surface);
    collection.UpdateHeader();
    
    std::cout << "Test object created with mathematical colors:\n";
    std::cout << "  R: LINEAR_U with param 200 (78%)\n";
    std::cout << "  G: LINEAR_V with param 180 (70%)\n";
    std::cout << "  B: CONSTANT with param 160 (63%)\n";
    
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
    ProceduralObjectCollection collection;
    if (!collection.LoadFromFile(psf_file)) {
        std::cout << "❌ Failed to load PSF file: " << psf_file << "\n";
        return false;
    }

    std::vector<float> vertices, normals, colors;
    std::vector<uint32_t> indices;

    if (collection.surface_pool.empty()) {
        std::cout << "Warning: No surfaces in collection.\n";
        return false;
    }

    if (verbose) {
        std::cout << "=== ENHANCED COLOR EXPORT DEBUG ===\n";
        for (size_t i = 0; i < std::min<size_t>(collection.surface_pool.size(), 3); ++i) {
            const auto& surface = collection.surface_pool[i];
            std::cout << "Surface " << i << " color equations: R=" << static_cast<int>(surface.color_eqn_r)
                      << " G=" << static_cast<int>(surface.color_eqn_g) 
                      << " B=" << static_cast<int>(surface.color_eqn_b) << "\n";
            std::cout << "Surface " << i << " color params: R=" << static_cast<int>(surface.color_param_r)
                      << " G=" << static_cast<int>(surface.color_param_g) 
                      << " B=" << static_cast<int>(surface.color_param_b) << "\n";
            
            // Test mathematical color evaluation at multiple points
            for (float u = 0.0f; u <= 1.0f; u += 0.5f) {
                for (float v = 0.0f; v <= 1.0f; v += 0.5f) {
                    float test_color[3];
                    MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, u, v, test_color);
                    std::cout << "  UV(" << u << "," << v << "): (" << test_color[0] 
                              << ", " << test_color[1] << ", " << test_color[2] << ")\n";
                }
            }
        }
    }

    // Convert anchor points to vertices with ENHANCED MATHEMATICAL COLOR EVALUATION
    for (size_t i = 0; i < collection.anchor_pool.size(); ++i) {
        const auto& anchor = collection.anchor_pool[i];
        Vec3 pos = AnchorPointEvaluator::DecodePosition(anchor.pos_xyz);
        
        vertices.push_back(pos.x);
        vertices.push_back(pos.y);
        vertices.push_back(pos.z);
        
        // Decode normal
        Vec3 normal = AnchorPointEvaluator::DecodeNormal(anchor.normal);
        normals.push_back(normal.x);
        normals.push_back(normal.y);
        normals.push_back(normal.z);
        
        // ENHANCED: Multiple color sampling strategies
        Vec3 anchor_color(0.7f, 0.7f, 0.7f); // Default fallback
        bool found_surface_color = false;
        float best_color_weight = 0.0f;
        
        // Strategy 1: Find the surface this anchor belongs to and evaluate its mathematical color
        for (size_t surf_idx = 0; surf_idx < collection.surface_pool.size(); ++surf_idx) {
            const auto& surface = collection.surface_pool[surf_idx];
            
            // Check if this anchor is used by this surface
            for (int j = 0; j < 4; ++j) {
                if (surface.anchor_point_indices[j] == i) {
                    // Calculate UV coordinates for this anchor within the surface
                    float u = (j == 1 || j == 2) ? 1.0f : 0.0f;
                    float v = (j == 2 || j == 3) ? 1.0f : 0.0f;
                    
                    float math_color[3];
                    MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, u, v, math_color);
                    
                    // Weight this color based on surface area/importance
                    float color_weight = 1.0f;
                    
                    if (color_weight > best_color_weight) {
                        anchor_color = Vec3(math_color[0], math_color[1], math_color[2]);
                        best_color_weight = color_weight;
                        found_surface_color = true;
                    }
                    
                    if (verbose && i < 5) {
                        std::cout << "Anchor " << i << " surface " << surf_idx << " UV(" << u << "," << v 
                                  << ") color: (" << math_color[0] << ", " << math_color[1] << ", " << math_color[2] << ")\n";
                    }
                    break;
                }
            }
        }
        
        // Strategy 2: If no surface color found, use anchor's stored color as fallback
        if (!found_surface_color) {
            anchor_color = Vec3(
                static_cast<float>(anchor.color[0]) / 255.0f,
                static_cast<float>(anchor.color[1]) / 255.0f,
                static_cast<float>(anchor.color[2]) / 255.0f
            );
            
            if (verbose && i < 5) {
                std::cout << "Anchor " << i << " using stored color: (" 
                          << anchor_color.x << ", " << anchor_color.y << ", " << anchor_color.z << ")\n";
            }
        }
        
        // Ensure minimum visibility
        if (anchor_color.x + anchor_color.y + anchor_color.z < 0.3f) {
            float boost = 0.6f / (anchor_color.x + anchor_color.y + anchor_color.z + 0.01f);
            boost = std::min(boost, 2.0f);
            anchor_color.x = std::min(1.0f, anchor_color.x * boost);
            anchor_color.y = std::min(1.0f, anchor_color.y * boost);
            anchor_color.z = std::min(1.0f, anchor_color.z * boost);
        }
        
        colors.push_back(anchor_color.x);
        colors.push_back(anchor_color.y);
        colors.push_back(anchor_color.z);
    }

    // Convert surfaces to faces (same as before)
    for (const auto& surface : collection.surface_pool) {
        // Validate anchor indices
        bool valid = true;
        for (int i = 0; i < 4; ++i) {
            if (surface.anchor_point_indices[i] >= collection.anchor_pool.size()) {
                valid = false;
                break;
            }
        }
        
        if (!valid) continue;
        
        // Create triangles from quad
        indices.push_back(surface.anchor_point_indices[0]);
        indices.push_back(surface.anchor_point_indices[1]);
        indices.push_back(surface.anchor_point_indices[2]);
        
        indices.push_back(surface.anchor_point_indices[0]);
        indices.push_back(surface.anchor_point_indices[2]);
        indices.push_back(surface.anchor_point_indices[3]);
    }

    // Write enhanced OBJ file
    std::ofstream file(obj_file);
    if (!file.is_open()) {
        std::cout << "❌ Failed to open OBJ file: " << obj_file << "\n";
        return false;
    }

    file << "# Procedural Surface Export with Enhanced Mathematical Colors\n";
    file << "# Generated with color-preserving PSF engine\n";
    
    // Write vertices with enhanced mathematical colors
    for (size_t i = 0; i < vertices.size(); i += 3) {
        file << "v " << vertices[i] << " " << vertices[i+1] << " " << vertices[i+2]
             << " " << colors[i] << " " << colors[i+1] << " " << colors[i+2] << "\n";
    }

    // Write normals
    for (size_t i = 0; i < normals.size(); i += 3) {
        file << "vn " << normals[i] << " " << normals[i+1] << " " << normals[i+2] << "\n";
    }

    // Write faces
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t v1 = indices[i] + 1;
        uint32_t v2 = indices[i+1] + 1;
        uint32_t v3 = indices[i+2] + 1;
        file << "f " << v1 << "//" << v1 << " " << v2 << "//" << v2 << " " << v3 << "//" << v3 << "\n";
    }

    file.close();
    std::cout << "✅ Exported OBJ with enhanced mathematical colors: " << obj_file << "\n";
    
    if (verbose) {
        // Calculate and report color statistics
        Vec3 min_color(1.0f, 1.0f, 1.0f), max_color(0.0f, 0.0f, 0.0f), avg_color(0.0f, 0.0f, 0.0f);
        for (size_t i = 0; i < colors.size(); i += 3) {
            min_color.x = std::min(min_color.x, colors[i]);
            min_color.y = std::min(min_color.y, colors[i+1]);
            min_color.z = std::min(min_color.z, colors[i+2]);
            
            max_color.x = std::max(max_color.x, colors[i]);
            max_color.y = std::max(max_color.y, colors[i+1]);
            max_color.z = std::max(max_color.z, colors[i+2]);
            
            avg_color.x += colors[i];
            avg_color.y += colors[i+1];
            avg_color.z += colors[i+2];
        }
        
        if (!colors.empty()) {
            int vertex_count = colors.size() / 3;
            avg_color.x /= vertex_count;
            avg_color.y /= vertex_count;
            avg_color.z /= vertex_count;
            
            std::cout << "Exported color statistics:\n";
            std::cout << "  Vertices: " << vertex_count << "\n";
            std::cout << "  Color range: (" << min_color.x << ", " << min_color.y << ", " << min_color.z 
                      << ") to (" << max_color.x << ", " << max_color.y << ", " << max_color.z << ")\n";
            std::cout << "  Average color: (" << avg_color.x << ", " << avg_color.y << ", " << avg_color.z << ")\n";
        }
    }
    
    return true;
}

bool ViewPSFDirectly(const std::string& psf_file, bool verbose) {
    std::cout << "=== PSF VIEWER WITH MATHEMATICAL COLOR DEBUGGING ===\n";
    std::cout << "Loading PSF file: " << psf_file << "\n";
    
    ProceduralObjectCollection collection;
    if (!collection.LoadFromFile(psf_file)) {
        std::cout << "❌ Failed to load PSF file: " << psf_file << "\n";
        return false;
    }
    
    if (verbose) {
        std::cout << "✅ PSF loaded successfully\n";
        std::cout << "Surfaces: " << collection.surface_pool.size() << "\n";
        std::cout << "Anchors: " << collection.anchor_pool.size() << "\n";
        std::cout << "Bones: " << collection.bone_pool.size() << "\n";
        
        // DEBUG: Print color information for first few surfaces
        std::cout << "\n=== COLOR DEBUG ANALYSIS ===\n";
        for (size_t i = 0; i < std::min<size_t>(collection.surface_pool.size(), 5); ++i) {
            const auto& surface = collection.surface_pool[i];
            std::cout << "Surface " << i << ":\n";
            std::cout << "  Color equations: R=" << MathematicalColorEvaluator::GetColorEquationName(static_cast<ColorEquationType>(surface.color_eqn_r))
                      << " G=" << MathematicalColorEvaluator::GetColorEquationName(static_cast<ColorEquationType>(surface.color_eqn_g))
                      << " B=" << MathematicalColorEvaluator::GetColorEquationName(static_cast<ColorEquationType>(surface.color_eqn_b)) << "\n";
            std::cout << "  Color params: R=" << static_cast<int>(surface.color_param_r)
                      << " G=" << static_cast<int>(surface.color_param_g) 
                      << " B=" << static_cast<int>(surface.color_param_b) << "\n";
            
            // Test mathematical color evaluation at different UV points
            float test_colors[4][3];
            MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, 0.0f, 0.0f, test_colors[0]);
            MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, 1.0f, 0.0f, test_colors[1]);
            MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, 0.5f, 0.5f, test_colors[2]);
            MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, 1.0f, 1.0f, test_colors[3]);
            
            std::cout << "  Mathematical colors:\n";
            std::cout << "    UV(0,0): (" << test_colors[0][0] << ", " << test_colors[0][1] << ", " << test_colors[0][2] << ")\n";
            std::cout << "    UV(1,0): (" << test_colors[1][0] << ", " << test_colors[1][1] << ", " << test_colors[1][2] << ")\n";
            std::cout << "    UV(0.5,0.5): (" << test_colors[2][0] << ", " << test_colors[2][1] << ", " << test_colors[2][2] << ")\n";
            std::cout << "    UV(1,1): (" << test_colors[3][0] << ", " << test_colors[3][1] << ", " << test_colors[3][2] << ")\n";
        }
    }
    
    // Use PSF realtime renderer for preview with enhanced color debugging
    PSFRealtimeConfig config;
    config.max_points_per_surface = 2000; // Increased for better color representation
    config.enable_global_illumination = true;
    config.debug_timing = verbose;
    config.min_point_spacing = 0.01f; // Finer spacing for better color detail
    config.max_point_spacing = 0.5f;
    config.ambient_intensity = 0.2f; // Increase ambient to see colors better
    
    auto renderer = std::make_unique<PSFRealtimeRenderer>(config);
    
    // Calculate optimal camera position
    Vec3 centroid(0.0f, 0.0f, 0.0f);
    size_t valid_anchors = 0;
    for (const auto& anchor : collection.anchor_pool) {
        Vec3 pos = AnchorPointEvaluator::DecodePosition(anchor.pos_xyz);
        centroid.x += pos.x;
        centroid.y += pos.y;
        centroid.z += pos.z;
        valid_anchors++;
    }
    if (valid_anchors > 0) {
        centroid.x /= valid_anchors;
        centroid.y /= valid_anchors;
        centroid.z /= valid_anchors;
    }
    
    // Set up camera at optimal distance
    float camera_distance = 3.0f;
    float camera_pos[3] = {centroid.x, centroid.y, centroid.z + camera_distance};
    float camera_target[3] = {centroid.x, centroid.y, centroid.z};
    PSFRealtimeCamera camera = PSFRealtimeRenderer::PSFRealtimeCreateCamera(
        camera_pos, camera_target, 60.0f, 16.0f/9.0f
    );
    
    // Set up enhanced lighting for color visibility
    float light_pos1[3] = {centroid.x + 2.0f, centroid.y + 3.0f, centroid.z + 2.0f};
    float light_pos2[3] = {centroid.x - 2.0f, centroid.y + 2.0f, centroid.z + 3.0f};
    float light_color1[3] = {1.0f, 1.0f, 1.0f}; // Pure white light
    float light_color2[3] = {0.8f, 0.8f, 0.8f}; // Softer white light
    std::vector<PSFRealtimeLight> lights = {
        PSFRealtimeRenderer::PSFRealtimeCreatePointLight(light_pos1, light_color1, 3.0f, 20.0f),
        PSFRealtimeRenderer::PSFRealtimeCreatePointLight(light_pos2, light_color2, 2.0f, 15.0f)
    };
    
    // Render frame
    std::vector<PSFRealtimePoint> points;
    if (!renderer->PSFRealtimeRenderFrame(collection, lights, camera, points)) {
        std::cout << "❌ Failed to render PSF frame\n";
        return false;
    }
    
    // Display detailed render stats
    const auto& stats = renderer->PSFRealtimeGetLastFrameStats();
    std::cout << "\n=== RENDER STATS ===\n";
    std::cout << "Total Surfaces: " << stats.total_surfaces << "\n";
    std::cout << "Points Generated: " << stats.total_points_generated << "\n";
    std::cout << "Visible Points: " << stats.visible_points << "\n";
    std::cout << "Culled Points: " << stats.culled_points << "\n";
    std::cout << "Frame Time: " << stats.total_frame_time_ms << " ms\n";
    
    // Analyze point colors
    if (verbose && !points.empty()) {
        std::cout << "\n=== POINT COLOR ANALYSIS ===\n";
        Vec3 avg_color(0, 0, 0);
        Vec3 min_color(1, 1, 1);
        Vec3 max_color(0, 0, 0);
        int visible_count = 0;
        
        for (const auto& point : points) {
            if (!(point.flags & 0x1)) { // Not culled
                avg_color.x += point.color[0];
                avg_color.y += point.color[1];
                avg_color.z += point.color[2];
                
                min_color.x = std::min(min_color.x, point.color[0]);
                min_color.y = std::min(min_color.y, point.color[1]);
                min_color.z = std::min(min_color.z, point.color[2]);
                
                max_color.x = std::max(max_color.x, point.color[0]);
                max_color.y = std::max(max_color.y, point.color[1]);
                max_color.z = std::max(max_color.z, point.color[2]);
                
                visible_count++;
            }
        }
        
        if (visible_count > 0) {
            avg_color.x /= visible_count;
            avg_color.y /= visible_count;
            avg_color.z /= visible_count;
            
            std::cout << "Average color: (" << avg_color.x << ", " << avg_color.y << ", " << avg_color.z << ")\n";
            std::cout << "Min color: (" << min_color.x << ", " << min_color.y << ", " << min_color.z << ")\n";
            std::cout << "Max color: (" << max_color.x << ", " << max_color.y << ", " << max_color.z << ")\n";
            
            // Sample a few points for detailed analysis
            std::cout << "Sample point colors:\n";
            for (int i = 0; i < std::min(10, static_cast<int>(points.size())); ++i) {
                if (!(points[i].flags & 0x1)) {
                    std::cout << "  Point " << i << ": color(" << points[i].color[0] 
                              << ", " << points[i].color[1] << ", " << points[i].color[2] 
                              << ") albedo(" << points[i].albedo[0] 
                              << ", " << points[i].albedo[1] << ", " << points[i].albedo[2] 
                              << ") surface_id=" << points[i].surface_id << "\n";
                }
            }
        }
    }
    
    std::cout << "\n✅ PSF viewing complete\n";
    std::cout << "If colors look wrong, check the mathematical color equations in the surface buffers.\n";
    return true;
}

void AnalyzeSimplificationPotential(const ProceduralObjectCollection& collection, bool verbose) {
    if (!verbose) return;
    
    std::cout << "\n=== SIMPLIFICATION ANALYSIS ===\n";
    std::cout << "Input surfaces: " << collection.surface_pool.size() << "\n";
    std::cout << "Input anchors: " << collection.anchor_pool.size() << "\n";
    
    std::map<SurfaceEquationType, int> surface_type_count;
    for (const auto& surface : collection.surface_pool) {
        SurfaceEquationType type = SurfaceBufferUtils::ExtractBaseType(&surface);
        surface_type_count[type]++;
    }
    
    std::cout << "Surface types:\n";
    for (const auto& pair : surface_type_count) {
        std::cout << "  Type " << static_cast<int>(pair.first) << ": " << pair.second << " surfaces\n";
    }
    
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
    
    float potential_reduction = std::min(90.0f, shared_edges * 100.0f / collection.surface_pool.size());
    std::cout << "Estimated reduction potential: " << potential_reduction << "%\n";
}




void DebugColorEquations(const ProceduralSurfaceBuffer& surface, size_t surface_idx, bool verbose) {
    if (!verbose) return;
    
    std::cout << "\n=== SURFACE " << surface_idx << " COLOR DEBUG ===\n";
    std::cout << "Color equations: R=" << static_cast<int>(surface.color_eqn_r) 
              << " G=" << static_cast<int>(surface.color_eqn_g) 
              << " B=" << static_cast<int>(surface.color_eqn_b) << "\n";
    std::cout << "Color params: R=" << static_cast<int>(surface.color_param_r)
              << " G=" << static_cast<int>(surface.color_param_g) 
              << " B=" << static_cast<int>(surface.color_param_b) << "\n";
    
    // Test mathematical evaluation at multiple UV points
    std::cout << "Mathematical color evaluation test:\n";
    float test_uvs[][2] = {{0.0f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f}, {0.25f, 0.75f}};
    
    for (int i = 0; i < 4; ++i) {
        float u = test_uvs[i][0];
        float v = test_uvs[i][1];
        float color[3];
        
        MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, u, v, color);
        
        std::cout << "  UV(" << u << "," << v << "): RGB(" << color[0] 
                  << ", " << color[1] << ", " << color[2] << ")";
        
        // Check for suspicious values
        if (color[0] < 0.0f || color[0] > 1.0f || 
            color[1] < 0.0f || color[1] > 1.0f || 
            color[2] < 0.0f || color[2] > 1.0f) {
            std::cout << " ⚠️ OUT OF RANGE!";
        }
        if (color[0] > 0.9f && color[1] < 0.1f && color[2] < 0.1f) {
            std::cout << " ⚠️ PURE RED!";
        }
        if (color[0] < 0.1f && color[1] > 0.9f && color[2] < 0.1f) {
            std::cout << " ⚠️ PURE GREEN!";
        }
        if (color[0] < 0.1f && color[1] < 0.1f && color[2] > 0.9f) {
            std::cout << " ⚠️ PURE BLUE!";
        }
        std::cout << "\n";
    }
    std::cout << "===============================\n";
}


ProceduralObjectCollection SmartMeshConverter::ConvertMeshToProcedural(const EnhancedMeshData& meshData, bool verbose) {
    ProceduralObjectCollection collection;
    
    if (meshData.vertices.empty() || meshData.faces.empty()) {
        if (verbose) {
            std::cout << "WARNING: Empty mesh data: " << meshData.name << std::endl;
        }
        return collection;
    }
    
    collection.Reserve(0, meshData.vertices.size(), meshData.faces.size(), 1);

    // Calculate bounding box for coordinate transformation
    std::pair<Vec3, Vec3> bbox = FindBoundingBox(meshData.vertices);
    Vec3 min_pos = bbox.first;
    Vec3 max_pos = bbox.second;
    
    if (verbose) {
        std::cout << "\n=== FIXED MATHEMATICAL CONVERSION: " << meshData.name << " ===\n";
        std::cout << "Vertices: " << meshData.vertices.size() << "\n";
        std::cout << "Faces: " << meshData.faces.size() << "\n";
        std::cout << "Colors available: " << meshData.colors.size() << "\n";
    }

    // Add anchors with original colors preserved
    for (size_t i = 0; i < meshData.vertices.size(); ++i) {
        const Vec3& vertex = meshData.vertices[i];
        
        AnchorPointBuffer anchor;
        memset(&anchor, 0, sizeof(anchor));

        // Transform to PSF coordinate space
        Vec3 psf_pos = WorldToPSF(vertex, min_pos, max_pos);
        AnchorPointEvaluator::EncodePosition(psf_pos, anchor.pos_xyz);

        // Add normal
        if (i < meshData.normals.size()) {
            Vec3 normal = meshData.normals[i];
            float length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (length > 1e-6f) {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }
            anchor.normal[0] = static_cast<uint8_t>((normal.x * 0.5f + 0.5f) * 255.0f);
            anchor.normal[1] = static_cast<uint8_t>((normal.y * 0.5f + 0.5f) * 255.0f);
            anchor.normal[2] = static_cast<uint8_t>((normal.z * 0.5f + 0.5f) * 255.0f);
        } else {
            anchor.normal[0] = anchor.normal[1] = anchor.normal[2] = 128;
        }

        // Store original colors
        if (i < meshData.colors.size()) {
            Vec3 color = meshData.colors[i];
            color.x = std::max(0.0f, std::min(1.0f, color.x));
            color.y = std::max(0.0f, std::min(1.0f, color.y));
            color.z = std::max(0.0f, std::min(1.0f, color.z));
            
            anchor.color[0] = static_cast<uint8_t>(color.x * 255.0f);
            anchor.color[1] = static_cast<uint8_t>(color.y * 255.0f);
            anchor.color[2] = static_cast<uint8_t>(color.z * 255.0f);
        } else {
            anchor.color[0] = 180;
            anchor.color[1] = 140;
            anchor.color[2] = 100;
        }

        collection.AddAnchor(anchor);
    }

    // Process faces with WORKING mathematical color fitting
    size_t faces_processed = 0;
    
    for (const auto& face : meshData.faces) {
        if (face.size() < 3) continue;
        
        // Validate face indices
        bool valid_face = true;
        for (int idx : face) {
            if (idx < 0 || idx >= static_cast<int>(collection.anchor_pool.size())) {
                valid_face = false;
                break;
            }
        }
        
        if (!valid_face) continue;
        
        // Create anchor indices array
        uint16_t anchor_indices[4] = {0, 0, 0, 0};
        for (size_t j = 0; j < std::min<size_t>(face.size(), 4); ++j) {
            anchor_indices[j] = static_cast<uint16_t>(face[j]);
        }
        if (face.size() == 3) {
            anchor_indices[3] = anchor_indices[2];
        }
        
        // Calculate plane parameters
        float params[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        std::vector<Vec3> face_positions;
        for (int j = 0; j < 4; ++j) {
            if (anchor_indices[j] < collection.anchor_pool.size()) {
                face_positions.push_back(AnchorPointEvaluator::DecodePosition(
                    collection.anchor_pool[anchor_indices[j]].pos_xyz));
            }
        }
        if (face_positions.size() >= 3) {
            CalculateBestFitPlane(face_positions, params);
        }

        // Extract corner colors
        Vec3 corner_colors[4];
        for (int j = 0; j < 4; ++j) {
            int vertex_idx = face[std::min(j, static_cast<int>(face.size())-1)];
            if (vertex_idx >= 0 && vertex_idx < static_cast<int>(meshData.colors.size())) {
                corner_colors[j] = meshData.colors[vertex_idx];
            } else {
                corner_colors[j] = Vec3(0.7f, 0.5f, 0.3f);
            }
        }

        if (verbose && faces_processed < 3) {
            std::cout << "\n=== FACE " << faces_processed << " EQUATION FITTING ===\n";
            for (int j = 0; j < 4; ++j) {
                std::cout << "Corner " << j << ": (" << corner_colors[j].x << "," 
                          << corner_colors[j].y << "," << corner_colors[j].z << ")\n";
            }
        }

        // FIXED: Find best mathematical equations for each channel
        ColorEquationType best_eqn_r = COLOR_CONSTANT;
        ColorEquationType best_eqn_g = COLOR_CONSTANT;
        ColorEquationType best_eqn_b = COLOR_CONSTANT;
        
        uint8_t best_param_r = 128;
        uint8_t best_param_g = 128;
        uint8_t best_param_b = 128;

        // UV coordinates for the 4 corners
        float corner_uvs[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

        // For each color channel, find the best equation and parameter
        for (int channel = 0; channel < 3; ++channel) {
            float target_values[4];
            
            // Extract target values for this channel
            for (int corner = 0; corner < 4; ++corner) {
                if (channel == 0) target_values[corner] = corner_colors[corner].x;
                else if (channel == 1) target_values[corner] = corner_colors[corner].y;
                else target_values[corner] = corner_colors[corner].z;
            }

            ColorEquationType best_equation = COLOR_CONSTANT;
            uint8_t best_parameter = 128;
            float best_error = FLT_MAX;

            // Test ONLY equations that work properly
            std::vector<ColorEquationType> equations = {
                COLOR_CONSTANT, COLOR_LINEAR_U, COLOR_LINEAR_V, COLOR_BILINEAR
            };

            for (ColorEquationType equation : equations) {
                
                if (equation == COLOR_CONSTANT) {
                    // For constant: find average
                    float avg_target = (target_values[0] + target_values[1] + target_values[2] + target_values[3]) / 4.0f;
                    uint8_t param = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, avg_target * 255.0f)));
                    
                    float total_error = 0.0f;
                    for (int corner = 0; corner < 4; ++corner) {
                        float predicted = static_cast<float>(param) / 255.0f;
                        float error = (predicted - target_values[corner]) * (predicted - target_values[corner]);
                        total_error += error;
                    }
                    
                    if (total_error < best_error) {
                        best_error = total_error;
                        best_equation = equation;
                        best_parameter = param;
                    }
                }
                
                else if (equation == COLOR_LINEAR_U) {
                    // For linear U: color = param * u
                    // We have: target[0] = param * 0, target[1] = param * 1
                    // So param = target[1], and we check how well it fits
                    if (target_values[1] > 0.01f) { // Avoid division by zero
                        float param_float = target_values[1]; // param * 1 = target[1]
                        uint8_t param = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, param_float * 255.0f)));
                        
                        float total_error = 0.0f;
                        for (int corner = 0; corner < 4; ++corner) {
                            float u = corner_uvs[corner][0];
                            float predicted = (static_cast<float>(param) / 255.0f) * u;
                            float error = (predicted - target_values[corner]) * (predicted - target_values[corner]);
                            total_error += error;
                        }
                        
                        if (total_error < best_error) {
                            best_error = total_error;
                            best_equation = equation;
                            best_parameter = param;
                        }
                    }
                }
                
                else if (equation == COLOR_LINEAR_V) {
                    // For linear V: color = param * v
                    // target[3] = param * 1 (corner 3 has v=1)
                    if (target_values[3] > 0.01f) { // Avoid division by zero
                        float param_float = target_values[3];
                        uint8_t param = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, param_float * 255.0f)));
                        
                        float total_error = 0.0f;
                        for (int corner = 0; corner < 4; ++corner) {
                            float v = corner_uvs[corner][1];
                            float predicted = (static_cast<float>(param) / 255.0f) * v;
                            float error = (predicted - target_values[corner]) * (predicted - target_values[corner]);
                            total_error += error;
                        }
                        
                        if (total_error < best_error) {
                            best_error = total_error;
                            best_equation = equation;
                            best_parameter = param;
                        }
                    }
                }
                
                else if (equation == COLOR_BILINEAR) {
                    // For bilinear: color = param * u * v
                    // target[2] = param * 1 * 1 = param (corner 2 has u=1, v=1)
                    if (target_values[2] > 0.01f) { // Avoid division by zero
                        float param_float = target_values[2];
                        uint8_t param = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, param_float * 255.0f)));
                        
                        float total_error = 0.0f;
                        for (int corner = 0; corner < 4; ++corner) {
                            float u = corner_uvs[corner][0];
                            float v = corner_uvs[corner][1];
                            float predicted = (static_cast<float>(param) / 255.0f) * u * v;
                            float error = (predicted - target_values[corner]) * (predicted - target_values[corner]);
                            total_error += error;
                        }
                        
                        if (total_error < best_error) {
                            best_error = total_error;
                            best_equation = equation;
                            best_parameter = param;
                        }
                    }
                }
            }

            // Store results
            if (channel == 0) {
                best_eqn_r = best_equation;
                best_param_r = best_parameter;
            } else if (channel == 1) {
                best_eqn_g = best_equation;
                best_param_g = best_parameter;
            } else {
                best_eqn_b = best_equation;
                best_param_b = best_parameter;
            }

            if (verbose && faces_processed < 3) {
                std::cout << "Channel " << (channel == 0 ? "R" : channel == 1 ? "G" : "B") 
                          << ": " << (best_equation == COLOR_CONSTANT ? "CONSTANT" :
                                     best_equation == COLOR_LINEAR_U ? "LINEAR_U" :
                                     best_equation == COLOR_LINEAR_V ? "LINEAR_V" : "BILINEAR")
                          << ", param=" << static_cast<int>(best_parameter) 
                          << ", error=" << best_error << "\n";
            }
        }

        // OUTLIER VALIDATION AND CORRECTION
        bool has_outliers = false;
        float max_acceptable_error = 0.3f; // Threshold for acceptable color error
        float max_single_channel_error = 0.4f; // Per-channel error threshold

        // Test the fitted equations against the target colors
        float total_face_error = 0.0f;
        float max_channel_error = 0.0f;

        for (int corner = 0; corner < 4; ++corner) {
            float u = corner_uvs[corner][0];
            float v = corner_uvs[corner][1];
            
            // Test each channel individually
            float predicted_r = 0.0f, predicted_g = 0.0f, predicted_b = 0.0f;
            
            if (best_eqn_r == COLOR_CONSTANT) {
                predicted_r = static_cast<float>(best_param_r) / 255.0f;
            } else if (best_eqn_r == COLOR_LINEAR_U) {
                predicted_r = (static_cast<float>(best_param_r) / 255.0f) * u;
            } else if (best_eqn_r == COLOR_LINEAR_V) {
                predicted_r = (static_cast<float>(best_param_r) / 255.0f) * v;
            } else if (best_eqn_r == COLOR_BILINEAR) {
                predicted_r = (static_cast<float>(best_param_r) / 255.0f) * u * v;
            }
            
            if (best_eqn_g == COLOR_CONSTANT) {
                predicted_g = static_cast<float>(best_param_g) / 255.0f;
            } else if (best_eqn_g == COLOR_LINEAR_U) {
                predicted_g = (static_cast<float>(best_param_g) / 255.0f) * u;
            } else if (best_eqn_g == COLOR_LINEAR_V) {
                predicted_g = (static_cast<float>(best_param_g) / 255.0f) * v;
            } else if (best_eqn_g == COLOR_BILINEAR) {
                predicted_g = (static_cast<float>(best_param_g) / 255.0f) * u * v;
            }
            
            if (best_eqn_b == COLOR_CONSTANT) {
                predicted_b = static_cast<float>(best_param_b) / 255.0f;
            } else if (best_eqn_b == COLOR_LINEAR_U) {
                predicted_b = (static_cast<float>(best_param_b) / 255.0f) * u;
            } else if (best_eqn_b == COLOR_LINEAR_V) {
                predicted_b = (static_cast<float>(best_param_b) / 255.0f) * v;
            } else if (best_eqn_b == COLOR_BILINEAR) {
                predicted_b = (static_cast<float>(best_param_b) / 255.0f) * u * v;
            }
            
            // Calculate errors
            float error_r = std::abs(predicted_r - corner_colors[corner].x);
            float error_g = std::abs(predicted_g - corner_colors[corner].y);
            float error_b = std::abs(predicted_b - corner_colors[corner].z);
            
            float corner_total_error = error_r + error_g + error_b;
            total_face_error += corner_total_error;
            
            max_channel_error = std::max({max_channel_error, error_r, error_g, error_b});
            
            // Check for outliers
            if (corner_total_error > max_acceptable_error || 
                error_r > max_single_channel_error || 
                error_g > max_single_channel_error || 
                error_b > max_single_channel_error) {
                has_outliers = true;
                
                if (verbose && faces_processed < 3) {
                    std::cout << "⚠️  OUTLIER detected at corner " << corner 
                              << ": errors R=" << error_r << " G=" << error_g << " B=" << error_b << "\n";
                }
            }
            
            // Check for obviously wrong values (out of range)
            if (predicted_r < -0.1f || predicted_r > 1.1f ||
                predicted_g < -0.1f || predicted_g > 1.1f ||
                predicted_b < -0.1f || predicted_b > 1.1f) {
                has_outliers = true;
                
                if (verbose && faces_processed < 3) {
                    std::cout << "⚠️  OUT OF RANGE values at corner " << corner 
                              << ": RGB(" << predicted_r << "," << predicted_g << "," << predicted_b << ")\n";
                }
            }
        }

        // APPLY FALLBACK FOR OUTLIERS
        if (has_outliers) {
            if (verbose && faces_processed < 3) {
                std::cout << "🔧 APPLYING OUTLIER CORRECTION - using safe COLOR_CONSTANT fallback\n";
            }
            
            // Calculate safe average colors from the corner colors
            float avg_r = (corner_colors[0].x + corner_colors[1].x + corner_colors[2].x + corner_colors[3].x) / 4.0f;
            float avg_g = (corner_colors[0].y + corner_colors[1].y + corner_colors[2].y + corner_colors[3].y) / 4.0f;
            float avg_b = (corner_colors[0].z + corner_colors[1].z + corner_colors[2].z + corner_colors[3].z) / 4.0f;
            
            // Clamp to safe ranges
            avg_r = std::max(0.0f, std::min(1.0f, avg_r));
            avg_g = std::max(0.0f, std::min(1.0f, avg_g));
            avg_b = std::max(0.0f, std::min(1.0f, avg_b));
            
            // Use COLOR_CONSTANT with the safe average colors
            best_eqn_r = COLOR_CONSTANT;
            best_eqn_g = COLOR_CONSTANT;
            best_eqn_b = COLOR_CONSTANT;
            
            best_param_r = static_cast<uint8_t>(avg_r * 255.0f);
            best_param_g = static_cast<uint8_t>(avg_g * 255.0f);
            best_param_b = static_cast<uint8_t>(avg_b * 255.0f);
            
            if (verbose && faces_processed < 3) {
                std::cout << "  Fallback colors: R=" << static_cast<int>(best_param_r)
                          << " G=" << static_cast<int>(best_param_g) 
                          << " B=" << static_cast<int>(best_param_b) << "\n";
            }
        } else {
            if (verbose && faces_processed < 3) {
                std::cout << "✅ Equations passed validation (max error: " << max_channel_error << ")\n";
            }
        }

        // ADDITIONAL SAFETY CHECKS
        // Ensure parameters are in reasonable ranges
        if (best_param_r < 10) best_param_r = 10;   // Minimum 4% to avoid pure black
        if (best_param_g < 10) best_param_g = 10;
        if (best_param_b < 10) best_param_b = 10;

        if (best_param_r > 250) best_param_r = 250; // Maximum 98% to avoid overflow
        if (best_param_g > 250) best_param_g = 250;
        if (best_param_b > 250) best_param_b = 250;

        // Detect and fix potential overflow cases for non-constant equations
        if (best_eqn_r != COLOR_CONSTANT && best_param_r > 200) {
            // High parameter with linear equations can cause overflow at corners
            best_eqn_r = COLOR_CONSTANT;
            best_param_r = static_cast<uint8_t>((corner_colors[0].x + corner_colors[1].x + corner_colors[2].x + corner_colors[3].x) / 4.0f * 255.0f);
            
            if (verbose && faces_processed < 3) {
                std::cout << "🔧 Fixed potential R overflow: switched to CONSTANT\n";
            }
        }

        if (best_eqn_g != COLOR_CONSTANT && best_param_g > 200) {
            best_eqn_g = COLOR_CONSTANT;
            best_param_g = static_cast<uint8_t>((corner_colors[0].y + corner_colors[1].y + corner_colors[2].y + corner_colors[3].y) / 4.0f * 255.0f);
            
            if (verbose && faces_processed < 3) {
                std::cout << "🔧 Fixed potential G overflow: switched to CONSTANT\n";
            }
        }

        if (best_eqn_b != COLOR_CONSTANT && best_param_b > 200) {
            best_eqn_b = COLOR_CONSTANT;
            best_param_b = static_cast<uint8_t>((corner_colors[0].z + corner_colors[1].z + corner_colors[2].z + corner_colors[3].z) / 4.0f * 255.0f);
            
            if (verbose && faces_processed < 3) {
                std::cout << "🔧 Fixed potential B overflow: switched to CONSTANT\n";
            }
        }

        // Create surface with the validated equations
        ProceduralSurfaceBuffer surface = SurfaceBufferUtils::CreateWithAnchors(
            EQUATION_PLANE, anchor_indices, params, MOD_COLOR);

        surface.color_eqn_r = static_cast<uint8_t>(best_eqn_r);
        surface.color_eqn_g = static_cast<uint8_t>(best_eqn_g);
        surface.color_eqn_b = static_cast<uint8_t>(best_eqn_b);
        
        surface.color_param_r = best_param_r;
        surface.color_param_g = best_param_g;
        surface.color_param_b = best_param_b;

        if (verbose && faces_processed < 3) {
            std::cout << "\nVERIFICATION:\n";
            for (int corner = 0; corner < 4; ++corner) {
                float u = corner_uvs[corner][0];
                float v = corner_uvs[corner][1];
                
                float result[3];
                MathematicalColorEvaluator::EvaluateColorFromBuffer(&surface, u, v, result);
                
                std::cout << "Corner " << corner << " UV(" << u << "," << v << "):\n";
                std::cout << "  Target: (" << corner_colors[corner].x << "," << corner_colors[corner].y << "," << corner_colors[corner].z << ")\n";
                std::cout << "  Result: (" << result[0] << "," << result[1] << "," << result[2] << ")\n";
                
                float total_error = std::abs(result[0] - corner_colors[corner].x) + 
                                   std::abs(result[1] - corner_colors[corner].y) + 
                                   std::abs(result[2] - corner_colors[corner].z);
                std::cout << "  Error: " << total_error << (total_error < 0.3f ? " ✅" : " ⚠️") << "\n";
            }
        }

        collection.AddSurface(surface);
        faces_processed++;
    }

    collection.UpdateHeader();
    
    if (verbose) {
        std::cout << "\n=== CONVERSION COMPLETE ===\n";
        std::cout << "Faces processed: " << faces_processed << "\n";
        std::cout << "Surfaces created: " << collection.surface_pool.size() << "\n";
        std::cout << "==============================\n\n";
    }
    
    return collection;
}



Vec3 SampleTextureColor(const RealTextureInfo& texInfo, float u, float v) {
    // Use the SampleColor method from RealTextureInfo
    return texInfo.SampleColor(u, v);
}


RealMaterialColorInfo ExtractRealMaterialInfo(FbxNode* node, const std::string& fbxPath, bool verbose) {
    RealMaterialColorInfo matInfo;
    
    if (verbose) {
        std::cout << "=== ENHANCED MATERIAL ANALYSIS ===\n";
        std::cout << "FBX Path: " << fbxPath << "\n";
    }
    
    for (int matIndex = 0; matIndex < node->GetMaterialCount(); ++matIndex) {
        FbxSurfaceMaterial* material = node->GetMaterial(matIndex);
        if (!material) continue;
        
        if (verbose) {
            std::cout << "Material " << matIndex << ": " << material->GetName() << "\n";
        }
        
        // Extract material properties
        if (material->GetClassId().Is(FbxSurfacePhong::ClassId)) {
            FbxSurfacePhong* phong = static_cast<FbxSurfacePhong*>(material);
            FbxDouble3 diffuse = phong->Diffuse.Get();
            matInfo.diffuse.x = static_cast<float>(diffuse[0]);
            matInfo.diffuse.y = static_cast<float>(diffuse[1]);
            matInfo.diffuse.z = static_cast<float>(diffuse[2]);
            
            if (verbose) {
                std::cout << "  Phong material diffuse: (" << matInfo.diffuse.x 
                          << ", " << matInfo.diffuse.y << ", " << matInfo.diffuse.z << ")\n";
            }
        } else if (material->GetClassId().Is(FbxSurfaceLambert::ClassId)) {
            FbxSurfaceLambert* lambert = static_cast<FbxSurfaceLambert*>(material);
            FbxDouble3 diffuse = lambert->Diffuse.Get();
            matInfo.diffuse.x = static_cast<float>(diffuse[0]);
            matInfo.diffuse.y = static_cast<float>(diffuse[1]);
            matInfo.diffuse.z = static_cast<float>(diffuse[2]);
            
            if (verbose) {
                std::cout << "  Lambert material diffuse: (" << matInfo.diffuse.x 
                          << ", " << matInfo.diffuse.y << ", " << matInfo.diffuse.z << ")\n";
            }
        }
        
        // Enhanced texture extraction with comprehensive property checking
        auto extractTextures = [&](const char* propName) {
            if (verbose) {
                std::cout << "  Checking property: " << propName << "\n";
            }
            
            FbxProperty prop = material->FindProperty(propName);
            if (prop.IsValid()) {
                int textureCount = prop.GetSrcObjectCount<FbxTexture>();
                if (verbose) {
                    std::cout << "    Found " << textureCount << " textures for " << propName << "\n";
                }
                
                for (int texIndex = 0; texIndex < textureCount; ++texIndex) {
                    FbxTexture* texture = prop.GetSrcObject<FbxTexture>(texIndex);
                    if (FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture)) {
                        RealTextureInfo texInfo;
                        texInfo.filename = fileTexture->GetFileName();
                        texInfo.fbxTexture = fileTexture;
                        
                        if (verbose) {
                            std::cout << "      Texture " << texIndex << ": " << texInfo.filename << "\n";
                        }
                        
                        // Get UV set name
                        FbxString uvSetName = fileTexture->UVSet.Get();
                        if (uvSetName.GetLen() > 0) {
                            texInfo.uvSetName = uvSetName.Buffer();
                            if (verbose) {
                                std::cout << "        UV Set: " << texInfo.uvSetName << "\n";
                            }
                        } else {
                            if (verbose) {
                                std::cout << "        UV Set: <default>\n";
                            }
                        }
                        
                        // Actually load the texture image with enhanced path resolution
                        if (verbose) {
                            std::cout << "        Attempting to load texture...\n";
                        }
                        
                        if (texInfo.LoadImage(fbxPath)) {
                            texInfo.isValid = true;
                            matInfo.textures.push_back(std::move(texInfo));
                            
                            if (verbose) {
                                std::cout << "        ✅ Successfully loaded texture!\n";
                            }
                        } else {
                            if (verbose) {
                                std::cout << "        ❌ Failed to load texture\n";
                            }
                            // Still add it to the list but mark as invalid
                            texInfo.isValid = false;
                            matInfo.textures.push_back(std::move(texInfo));
                        }
                    } else {
                        if (verbose) {
                            std::cout << "      Texture " << texIndex << ": Not a file texture (skipping)\n";
                        }
                    }
                }
            } else {
                if (verbose) {
                    std::cout << "    Property " << propName << " not found or invalid\n";
                }
            }
        };
        
        // Check all common texture properties
        extractTextures(FbxSurfaceMaterial::sDiffuse);
        extractTextures(FbxSurfaceMaterial::sNormalMap);
        extractTextures(FbxSurfaceMaterial::sSpecular);
        extractTextures("DiffuseColor");
        extractTextures("BaseColor");
        extractTextures("base_color_texture"); // Sometimes used
        extractTextures("diffuse_texture");    // Alternative name
        
        // Use first material found
        break;
    }
    
    if (verbose) {
        std::cout << "=== MATERIAL ANALYSIS SUMMARY ===\n";
        std::cout << "Total textures found: " << matInfo.textures.size() << "\n";
        std::cout << "Valid textures: " << std::count_if(matInfo.textures.begin(), matInfo.textures.end(), 
                                                         [](const RealTextureInfo& t) { return t.isValid; }) << "\n";
        std::cout << "Final material diffuse: (" << matInfo.diffuse.x 
                  << ", " << matInfo.diffuse.y << ", " << matInfo.diffuse.z << ")\n";
        std::cout << "=====================================\n";
    }
    
    return matInfo;
}







#ifndef NO_FBX_SDK

void ExtractEnhancedMeshFromNode(FbxNode* node, std::vector<EnhancedMeshData>& meshes, 
                               std::vector<FbxNode*>& mesh_nodes, bool verbose, 
                               const std::string& fbxPath = "") {
    if (!node) return;
    
    FbxMesh* mesh = node->GetMesh();
    if (mesh) {
        EnhancedMeshData meshData;
        meshData.name = node->GetName();
        if (meshData.name.empty()) {
            meshData.name = "Mesh_" + std::to_string(meshes.size());
        }
        
        int vertexCount = mesh->GetControlPointsCount();
        int polygonCount = mesh->GetPolygonCount();
        
        if (verbose) {
            std::cout << "\n=== ENHANCED COLOR EXTRACTION DEBUG ===\n";
            std::cout << "Processing mesh: " << meshData.name << " (" 
                      << vertexCount << " vertices, " << polygonCount << " polygons)\n";
        }
        
        meshData.vertices.reserve(vertexCount);
        meshData.normals.reserve(vertexCount);
        meshData.colors.reserve(vertexCount);
        
        FbxVector4* controlPoints = mesh->GetControlPoints();
        FbxGeometryElementNormal* normalElement = mesh->GetElementNormal();
        
        // Enhanced material analysis with real texture loading
        RealMaterialColorInfo materialInfo = ExtractRealMaterialInfo(node, fbxPath, verbose);
        
        // UV elements
        std::vector<FbxGeometryElementUV*> uvElements;
        std::map<std::string, FbxGeometryElementUV*> uvSetMap;
        
        for (int i = 0; i < mesh->GetElementUVCount(); ++i) {
            FbxGeometryElementUV* uvElement = mesh->GetElementUV(i);
            if (uvElement) {
                uvElements.push_back(uvElement);
                std::string uvName = uvElement->GetName();
                if (uvName.empty()) uvName = "default";
                uvSetMap[uvName] = uvElement;
                
                if (verbose) {
                    std::cout << "  UV Set " << i << ": " << uvName << " (count: " << uvElement->GetDirectArray().GetCount() << ")\n";
                }
            }
        }
        
        // ===== EXTRACT VERTICES =====
        for (int i = 0; i < vertexCount; ++i) {
            FbxVector4 vert = controlPoints[i];
            meshData.vertices.emplace_back(
                static_cast<float>(vert[0]),
                static_cast<float>(vert[1]),
                static_cast<float>(vert[2])
            );
        }
        
        // ===== EXTRACT NORMALS =====
        if (normalElement) {
            for (int i = 0; i < vertexCount; ++i) {
                FbxVector4 normal;
                if (normalElement->GetMappingMode() == FbxGeometryElement::eByControlPoint) {
                    if (normalElement->GetReferenceMode() == FbxGeometryElement::eDirect) {
                        if (i < normalElement->GetDirectArray().GetCount()) {
                            normal = normalElement->GetDirectArray().GetAt(i);
                        } else {
                            normal = FbxVector4(0.0, 0.0, 1.0, 0.0);
                        }
                    } else if (normalElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
                        if (i < normalElement->GetIndexArray().GetCount()) {
                            int id = normalElement->GetIndexArray().GetAt(i);
                            if (id >= 0 && id < normalElement->GetDirectArray().GetCount()) {
                                normal = normalElement->GetDirectArray().GetAt(id);
                            } else {
                                normal = FbxVector4(0.0, 0.0, 1.0, 0.0);
                            }
                        } else {
                            normal = FbxVector4(0.0, 0.0, 1.0, 0.0);
                        }
                    }
                } else {
                    normal = FbxVector4(0.0, 0.0, 0.0, 0.0);
                    int normalCount = 0;
                    
                    for (int poly = 0; poly < polygonCount; ++poly) {
                        int polySize = mesh->GetPolygonSize(poly);
                        for (int vert = 0; vert < polySize; ++vert) {
                            if (mesh->GetPolygonVertex(poly, vert) == i) {
                                int vertexId = mesh->GetPolygonVertexIndex(poly) + vert;
                                
                                if (normalElement->GetReferenceMode() == FbxGeometryElement::eDirect) {
                                    if (vertexId < normalElement->GetDirectArray().GetCount()) {
                                        normal += normalElement->GetDirectArray().GetAt(vertexId);
                                        normalCount++;
                                    }
                                } else if (normalElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
                                    if (vertexId < normalElement->GetIndexArray().GetCount()) {
                                        int normalId = normalElement->GetIndexArray().GetAt(vertexId);
                                        if (normalId >= 0 && normalId < normalElement->GetDirectArray().GetCount()) {
                                            normal += normalElement->GetDirectArray().GetAt(normalId);
                                            normalCount++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    if (normalCount > 0) {
                        normal /= static_cast<double>(normalCount);
                    } else {
                        normal = FbxVector4(0.0, 0.0, 1.0, 0.0);
                    }
                }
                
                meshData.normals.emplace_back(
                    static_cast<float>(normal[0]),
                    static_cast<float>(normal[1]),
                    static_cast<float>(normal[2])
                );
            }
        } else {
            for (int i = 0; i < vertexCount; ++i) {
                meshData.normals.emplace_back(0.0f, 0.0f, 1.0f);
            }
        }
        
        // ===== ENHANCED MULTI-STRATEGY COLOR EXTRACTION =====
        meshData.colors.resize(vertexCount, Vec3(0.8f, 0.6f, 0.4f)); // Warmer default instead of gray
        bool extracted_vertex_colors = false;
        bool extracted_texture_colors = false;
        bool extracted_material_colors = false;
        
        if (verbose) {
            std::cout << "=== MULTI-STRATEGY COLOR EXTRACTION ===\n";
        }
        
        // STRATEGY 1: Extract vertex colors if available
        FbxGeometryElementVertexColor* colorElement = mesh->GetElementVertexColor();
        if (colorElement) {
            if (verbose) {
                std::cout << "✅ Found vertex color element, mapping mode: " << colorElement->GetMappingMode() << "\n";
                std::cout << "   Reference mode: " << colorElement->GetReferenceMode() << "\n";
                std::cout << "   Direct array count: " << colorElement->GetDirectArray().GetCount() << "\n";
            }
            
            for (int i = 0; i < vertexCount; ++i) {
                FbxColor color(0.8, 0.6, 0.4, 1.0); // Warmer default
                bool found_color = false;
                
                if (colorElement->GetMappingMode() == FbxGeometryElement::eByControlPoint) {
                    if (colorElement->GetReferenceMode() == FbxGeometryElement::eDirect) {
                        if (i < colorElement->GetDirectArray().GetCount()) {
                            color = colorElement->GetDirectArray().GetAt(i);
                            found_color = true;
                        }
                    } else if (colorElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
                        if (i < colorElement->GetIndexArray().GetCount()) {
                            int colorId = colorElement->GetIndexArray().GetAt(i);
                            if (colorId >= 0 && colorId < colorElement->GetDirectArray().GetCount()) {
                                color = colorElement->GetDirectArray().GetAt(colorId);
                                found_color = true;
                            }
                        }
                    }
                } else if (colorElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
                    // Handle per-polygon-vertex colors by averaging
                    FbxColor avgColor(0.0, 0.0, 0.0, 0.0);
                    int colorCount = 0;
                    
                    for (int poly = 0; poly < polygonCount; ++poly) {
                        int polySize = mesh->GetPolygonSize(poly);
                        for (int vert = 0; vert < polySize; ++vert) {
                            if (mesh->GetPolygonVertex(poly, vert) == i) {
                                int vertexId = mesh->GetPolygonVertexIndex(poly) + vert;
                                
                                if (colorElement->GetReferenceMode() == FbxGeometryElement::eDirect) {
                                    if (vertexId < colorElement->GetDirectArray().GetCount()) {
                                        FbxColor polyColor = colorElement->GetDirectArray().GetAt(vertexId);
                                        avgColor.mRed += polyColor.mRed;
                                        avgColor.mGreen += polyColor.mGreen;
                                        avgColor.mBlue += polyColor.mBlue;
                                        colorCount++;
                                    }
                                } else if (colorElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
                                    if (vertexId < colorElement->GetIndexArray().GetCount()) {
                                        int colorId = colorElement->GetIndexArray().GetAt(vertexId);
                                        if (colorId >= 0 && colorId < colorElement->GetDirectArray().GetCount()) {
                                            FbxColor polyColor = colorElement->GetDirectArray().GetAt(colorId);
                                            avgColor.mRed += polyColor.mRed;
                                            avgColor.mGreen += polyColor.mGreen;
                                            avgColor.mBlue += polyColor.mBlue;
                                            colorCount++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    if (colorCount > 0) {
                        color.mRed = avgColor.mRed / colorCount;
                        color.mGreen = avgColor.mGreen / colorCount;
                        color.mBlue = avgColor.mBlue / colorCount;
                        found_color = true;
                    }
                }
                
                if (found_color) {
                    extracted_vertex_colors = true;
                }
                
                meshData.colors[i] = Vec3(
                    static_cast<float>(color.mRed),
                    static_cast<float>(color.mGreen),
                    static_cast<float>(color.mBlue)
                );
                
                if (verbose && i < 5) {
                    std::cout << "  Vertex " << i << " color: (" << meshData.colors[i].x 
                              << ", " << meshData.colors[i].y << ", " << meshData.colors[i].z << ") "
                              << (found_color ? "✅" : "❌") << "\n";
                }
            }
        } else {
            if (verbose) {
                std::cout << "❌ No vertex color element found\n";
            }
        }
        
        // STRATEGY 2: Extract texture colors if no vertex colors found
        if (!extracted_vertex_colors && !materialInfo.textures.empty() && !uvElements.empty()) {
            if (verbose) {
                std::cout << "=== TEXTURE COLOR EXTRACTION ===\n";
                std::cout << "Available textures: " << materialInfo.textures.size() << "\n";
                std::cout << "Available UV sets: " << uvElements.size() << "\n";
            }
            
            // Find the best texture and UV set combination
            RealTextureInfo* bestTexture = nullptr;
            FbxGeometryElementUV* bestUVElement = nullptr;
            
            // First try to find matching UV set
            for (auto& texInfo : materialInfo.textures) {
                if (texInfo.isValid) {
                    if (verbose) {
                        std::cout << "  Checking texture: " << texInfo.filename 
                                  << " (UV set: " << texInfo.uvSetName << ")\n";
                    }
                    
                    if (!texInfo.uvSetName.empty() && uvSetMap.find(texInfo.uvSetName) != uvSetMap.end()) {
                        bestTexture = &texInfo;
                        bestUVElement = uvSetMap[texInfo.uvSetName];
                        if (verbose) {
                            std::cout << "    ✅ Found matching UV set: " << texInfo.uvSetName << "\n";
                        }
                        break;
                    }
                }
            }
            
            // If no specific UV set match, use first valid texture and first UV set
            if (!bestTexture && !materialInfo.textures.empty() && !uvElements.empty()) {
                for (auto& texInfo : materialInfo.textures) {
                    if (texInfo.isValid) {
                        bestTexture = &texInfo;
                        bestUVElement = uvElements[0];
                        if (verbose) {
                            std::cout << "    ✅ Using first valid texture with first UV set\n";
                        }
                        break;
                    }
                }
            }
            
            if (bestTexture && bestUVElement) {
                if (verbose) {
                    std::cout << "✅ Using texture: " << bestTexture->filename << "\n";
                    std::cout << "✅ Using UV set: " << bestUVElement->GetName() << "\n";
                    std::cout << "   Texture size: " << bestTexture->width << "x" << bestTexture->height << "\n";
                    std::cout << "   Channels: " << bestTexture->channels << "\n";
                }
                
                std::vector<bool> controlPointProcessed(vertexCount, false);
                
                // Process all polygon vertices to get UV coordinates
                for (int polyIndex = 0; polyIndex < polygonCount; ++polyIndex) {
                    int polySize = mesh->GetPolygonSize(polyIndex);
                    
                    for (int vertIndex = 0; vertIndex < polySize; ++vertIndex) {
                        int controlPointIndex = mesh->GetPolygonVertex(polyIndex, vertIndex);
                        int polygonVertexIndex = mesh->GetPolygonVertexIndex(polyIndex) + vertIndex;
                        
                        if (controlPointIndex >= 0 && controlPointIndex < vertexCount && 
                            !controlPointProcessed[controlPointIndex]) {
                            
                            FbxVector2 uv(0.5, 0.5);
                            bool found_uv = false;
                            
                            // Get UV coordinates
                            if (bestUVElement->GetMappingMode() == FbxGeometryElement::eByControlPoint) {
                                if (bestUVElement->GetReferenceMode() == FbxGeometryElement::eDirect) {
                                    if (controlPointIndex < bestUVElement->GetDirectArray().GetCount()) {
                                        uv = bestUVElement->GetDirectArray().GetAt(controlPointIndex);
                                        found_uv = true;
                                    }
                                } else if (bestUVElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
                                    if (controlPointIndex < bestUVElement->GetIndexArray().GetCount()) {
                                        int uvId = bestUVElement->GetIndexArray().GetAt(controlPointIndex);
                                        if (uvId >= 0 && uvId < bestUVElement->GetDirectArray().GetCount()) {
                                            uv = bestUVElement->GetDirectArray().GetAt(uvId);
                                            found_uv = true;
                                        }
                                    }
                                }
                            } else if (bestUVElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
                                if (bestUVElement->GetReferenceMode() == FbxGeometryElement::eDirect) {
                                    if (polygonVertexIndex < bestUVElement->GetDirectArray().GetCount()) {
                                        uv = bestUVElement->GetDirectArray().GetAt(polygonVertexIndex);
                                        found_uv = true;
                                    }
                                } else if (bestUVElement->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
                                    if (polygonVertexIndex < bestUVElement->GetIndexArray().GetCount()) {
                                        int uvId = bestUVElement->GetIndexArray().GetAt(polygonVertexIndex);
                                        if (uvId >= 0 && uvId < bestUVElement->GetDirectArray().GetCount()) {
                                            uv = bestUVElement->GetDirectArray().GetAt(uvId);
                                            found_uv = true;
                                        }
                                    }
                                }
                            }
                            
                            if (found_uv) {
                                // Sample actual texture color from loaded image
                                Vec3 textureColor = bestTexture->SampleColor(
                                    static_cast<float>(uv[0]), 
                                    static_cast<float>(uv[1])
                                );
                                
                                meshData.colors[controlPointIndex] = textureColor;
                                controlPointProcessed[controlPointIndex] = true;
                                extracted_texture_colors = true;
                                
                                // Debug output for first few colors
                                if (verbose && controlPointIndex < 5) {
                                    std::cout << "  Vertex " << controlPointIndex << " UV(" << uv[0] << "," << uv[1] 
                                              << ") -> Color(" << textureColor.x << "," << textureColor.y << "," << textureColor.z << ")\n";
                                }
                            }
                        }
                    }
                }
                
                if (verbose) {
                    std::cout << "✅ Texture colors extracted successfully\n";
                }
            } else {
                if (verbose) {
                    std::cout << "❌ No valid texture/UV combination found\n";
                    if (materialInfo.textures.empty()) {
                        std::cout << "   Reason: No textures in material\n";
                    } else {
                        std::cout << "   Reason: No valid texture data or UV sets\n";
                        for (size_t i = 0; i < materialInfo.textures.size(); ++i) {
                            std::cout << "     Texture " << i << ": " << materialInfo.textures[i].filename 
                                      << " (valid: " << materialInfo.textures[i].isValid << ")\n";
                        }
                    }
                }
            }
        }
        
        // STRATEGY 3: Use enhanced material colors as fallback
        if (!extracted_vertex_colors && !extracted_texture_colors) {
            if (verbose) {
                std::cout << "=== MATERIAL COLOR FALLBACK ===\n";
                std::cout << "Material diffuse: (" << materialInfo.diffuse.x 
                          << ", " << materialInfo.diffuse.y << ", " << materialInfo.diffuse.z << ")\n";
            }
            
            Vec3 baseColor = materialInfo.diffuse;
            
            // Enhanced material color processing
            if (baseColor.x + baseColor.y + baseColor.z < 0.5f) {
                if (verbose) {
                    std::cout << "Material color too dark, using enhanced fallback\n";
                }
                baseColor = Vec3(0.7f, 0.5f, 0.3f); // Warm brown fallback
            }
            
            // Generate varied colors based on vertex position to avoid uniform appearance
            for (int i = 0; i < vertexCount; ++i) {
                Vec3 pos = meshData.vertices[i];
                
                // Create subtle color variation based on position
                float variation_r = sin(pos.x * 3.14159f + pos.y * 2.71828f) * 0.15f;
                float variation_g = cos(pos.y * 3.14159f + pos.z * 1.41421f) * 0.15f;
                float variation_b = sin(pos.z * 3.14159f + pos.x * 1.73205f) * 0.15f;
                
                meshData.colors[i] = Vec3(
                    std::max(0.1f, std::min(1.0f, baseColor.x + variation_r)),
                    std::max(0.1f, std::min(1.0f, baseColor.y + variation_g)),
                    std::max(0.1f, std::min(1.0f, baseColor.z + variation_b))
                );
            }
            extracted_material_colors = true;
            
            if (verbose) {
                std::cout << "✅ Generated position-based color variation\n";
            }
        }
        
        // ===== EXTRACT FACES =====
        for (int i = 0; i < polygonCount; ++i) {
            int polygonSize = mesh->GetPolygonSize(i);
            std::vector<int> face;
            face.reserve(polygonSize);
            
            for (int j = 0; j < polygonSize; ++j) {
                int vertexIndex = mesh->GetPolygonVertex(i, j);
                if (vertexIndex >= 0 && vertexIndex < vertexCount) {
                    face.push_back(vertexIndex);
                }
            }
            
            if (face.size() == 3) {
                meshData.AddTriangle(face[0], face[1], face[2]);
            } else if (face.size() == 4) {
                meshData.AddQuad(face[0], face[1], face[2], face[3]);
            } else if (face.size() > 4) {
                for (size_t triIndex = 1; triIndex < face.size() - 1; ++triIndex) {
                    meshData.AddTriangle(face[0], face[triIndex], face[triIndex + 1]);
                }
            }
        }
        
        meshes.push_back(meshData);
        mesh_nodes.push_back(node);
        
        if (verbose) {
            std::cout << "=== FINAL COLOR EXTRACTION SUMMARY ===\n";
            std::cout << "Mesh: " << meshData.name << "\n";
            std::cout << "Vertex colors: " << (extracted_vertex_colors ? "✅ YES" : "❌ NO") << "\n";
            std::cout << "Texture colors: " << (extracted_texture_colors ? "✅ YES" : "❌ NO") << "\n";
            std::cout << "Material colors: " << (extracted_material_colors ? "✅ YES" : "❌ NO") << "\n";
            
            // Calculate and display color statistics
            if (!meshData.colors.empty()) {
                Vec3 minColor(1.0f, 1.0f, 1.0f), maxColor(0.0f, 0.0f, 0.0f), avgColor(0.0f, 0.0f, 0.0f);
                for (const auto& color : meshData.colors) {
                    minColor.x = std::min(minColor.x, color.x);
                    minColor.y = std::min(minColor.y, color.y);
                    minColor.z = std::min(minColor.z, color.z);
                    maxColor.x = std::max(maxColor.x, color.x);
                    maxColor.y = std::max(maxColor.y, color.y);
                    maxColor.z = std::max(maxColor.z, color.z);
                    avgColor.x += color.x;
                    avgColor.y += color.y;
                    avgColor.z += color.z;
                }
                avgColor.x /= meshData.colors.size();
                avgColor.y /= meshData.colors.size();
                avgColor.z /= meshData.colors.size();
                
                std::cout << "Color range: (" << minColor.x << "," << minColor.y << "," << minColor.z
                          << ") to (" << maxColor.x << "," << maxColor.y << "," << maxColor.z << ")\n";
                std::cout << "Average color: (" << avgColor.x << "," << avgColor.y << "," << avgColor.z << ")\n";
                
                // Quality assessment
                float colorVariation = (maxColor.x - minColor.x) + (maxColor.y - minColor.y) + (maxColor.z - minColor.z);
                std::cout << "Color variation: " << colorVariation;
                if (colorVariation < 0.1f) {
                    std::cout << " ⚠️  LOW (very uniform colors)";
                } else if (colorVariation > 1.5f) {
                    std::cout << " ✅ HIGH (good color variety)";
                } else {
                    std::cout << " ✅ MODERATE";
                }
                std::cout << "\n";
                
                // Gray detection
                bool appears_gray = (avgColor.x > 0.4f && avgColor.x < 0.6f && 
                                   avgColor.y > 0.4f && avgColor.y < 0.6f && 
                                   avgColor.z > 0.4f && avgColor.z < 0.6f);
                if (appears_gray) {
                    std::cout << "⚠️  WARNING: Colors appear predominantly gray/neutral\n";
                } else {
                    std::cout << "✅ Colors appear varied and interesting\n";
                }
                
                // Show sample colors
                std::cout << "Sample colors:\n";
                int sample_count = std::min(5, static_cast<int>(meshData.colors.size()));
                for (int i = 0; i < sample_count; ++i) {
                    std::cout << "  Color " << i << ": (" << meshData.colors[i].x 
                              << ", " << meshData.colors[i].y << ", " << meshData.colors[i].z << ")\n";
                }
            }
            std::cout << "==========================================\n\n";
        }
    }
    
    // Recursively process child nodes
    for (int i = 0; i < node->GetChildCount(); ++i) {
        ExtractEnhancedMeshFromNode(node->GetChild(i), meshes, mesh_nodes, verbose, fbxPath);
    }
}

void ExtractSkeletonFromScene(FbxScene* scene, std::vector<BoneBuffer>& bone_pool, bool verbose) {
    if (!scene) {
        if (verbose) {
            std::cout << "No scene provided for skeleton extraction\n";
        }
        return;
    }
    
    if (verbose) {
        std::cout << "=== ENHANCED SKELETON EXTRACTION ===\n";
    }
    
    // Find all skeleton nodes in the scene
    std::vector<FbxNode*> skeleton_nodes;
    std::function<void(FbxNode*)> findSkeletonNodes = [&](FbxNode* node) {
        if (node) {
            FbxNodeAttribute* attribute = node->GetNodeAttribute();
            if (attribute && attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
                skeleton_nodes.push_back(node);
                if (verbose) {
                    std::cout << "Found skeleton node: " << node->GetName() << "\n";
                }
            }
            
            // Also check for nodes that might be bones even without skeleton attribute
            if (!attribute || attribute->GetAttributeType() == FbxNodeAttribute::eNull) {
                // Check if this node has bone-like properties (has children in hierarchy)
                bool has_mesh_children = false;
                bool has_bone_children = false;
                
                for (int i = 0; i < node->GetChildCount(); ++i) {
                    FbxNode* child = node->GetChild(i);
                    if (child) {
                        FbxNodeAttribute* childAttr = child->GetNodeAttribute();
                        if (childAttr) {
                            if (childAttr->GetAttributeType() == FbxNodeAttribute::eMesh) {
                                has_mesh_children = true;
                            } else if (childAttr->GetAttributeType() == FbxNodeAttribute::eSkeleton ||
                                      childAttr->GetAttributeType() == FbxNodeAttribute::eNull) {
                                has_bone_children = true;
                            }
                        }
                    }
                }
                
                // If this node has bone-like children and a bone-like name, treat it as a bone
                std::string name = node->GetName();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                bool has_bone_name = (name.find("bone") != std::string::npos ||
                                     name.find("joint") != std::string::npos ||
                                     name.find("limb") != std::string::npos ||
                                     name.find("spine") != std::string::npos ||
                                     name.find("arm") != std::string::npos ||
                                     name.find("leg") != std::string::npos ||
                                     name.find("hand") != std::string::npos ||
                                     name.find("foot") != std::string::npos ||
                                     name.find("head") != std::string::npos ||
                                     name.find("neck") != std::string::npos);
                
                if ((has_bone_children || has_bone_name) && !has_mesh_children) {
                    skeleton_nodes.push_back(node);
                    if (verbose) {
                        std::cout << "Found potential bone node: " << node->GetName() 
                                  << " (name-based: " << has_bone_name << ", children: " << has_bone_children << ")\n";
                    }
                }
            }
            
            // Recursively check children
            for (int i = 0; i < node->GetChildCount(); ++i) {
                findSkeletonNodes(node->GetChild(i));
            }
        }
    };
    
    // Start search from root
    findSkeletonNodes(scene->GetRootNode());
    
    if (skeleton_nodes.empty()) {
        if (verbose) {
            std::cout << "No skeleton nodes found in scene\n";
        }
        return;
    }
    
    if (verbose) {
        std::cout << "Found " << skeleton_nodes.size() << " skeleton/bone nodes total\n";
    }
    
    // Create a mapping from FbxNode to bone index
    std::map<FbxNode*, uint32_t> node_to_index;
    
    // Create bone buffers
    for (size_t i = 0; i < skeleton_nodes.size(); ++i) {
        FbxNode* bone_node = skeleton_nodes[i];
        node_to_index[bone_node] = static_cast<uint32_t>(i);
        
        BoneBuffer bone;
        memset(&bone, 0, sizeof(bone));
        
        // Get bone transform
        FbxAMatrix bone_transform = bone_node->EvaluateGlobalTransform();
        FbxVector4 translation = bone_transform.GetT();
        FbxQuaternion rotation = bone_transform.GetQ();
        
        // Encode position (same format as anchor points)
        Vec3 pos(static_cast<float>(translation[0]),
                 static_cast<float>(translation[1]),
                 static_cast<float>(translation[2]));
        
        // Transform position to PSF coordinate space [-2, +2]
        // For now, simple scaling - you might want to use the same transform as meshes
        pos.x = std::max(-2.0f, std::min(2.0f, pos.x * 0.01f)); // Scale down by 100
        pos.y = std::max(-2.0f, std::min(2.0f, pos.y * 0.01f));
        pos.z = std::max(-2.0f, std::min(2.0f, pos.z * 0.01f));
        
        AnchorPointEvaluator::EncodePosition(pos, bone.pos_xyz);
        
        // Encode rotation (simplified - you might want to implement proper quaternion compression)
        bone.orientation = 0; // TODO: Implement quaternion encoding
        
        // Set parent relationship
        FbxNode* parent = bone_node->GetParent();
        if (parent && node_to_index.find(parent) != node_to_index.end()) {
            bone.parent_bone_index = node_to_index[parent];
        } else {
            bone.parent_bone_index = static_cast<uint32_t>(i); // Self-reference for root bones
        }
        
        // Find first child bone
        bone.child_bone_index = static_cast<uint32_t>(i); // Self-reference if no children
        for (int j = 0; j < bone_node->GetChildCount(); ++j) {
            FbxNode* child = bone_node->GetChild(j);
            if (std::find(skeleton_nodes.begin(), skeleton_nodes.end(), child) != skeleton_nodes.end()) {
                // We'll set this properly in the second pass
                break;
            }
        }
        
        // Set some default physical parameters
        bone.physical_params[0] = 1.0f; // Mass
        bone.physical_params[1] = 0.1f; // Damping
        
        bone_pool.push_back(bone);
        
        if (verbose) {
            std::cout << "  Bone " << i << ": " << bone_node->GetName() 
                      << " pos(" << pos.x << ", " << pos.y << ", " << pos.z << ")"
                      << " parent=" << bone.parent_bone_index << "\n";
        }
    }
    
    // Second pass: Set child bone indices correctly
    for (size_t i = 0; i < skeleton_nodes.size(); ++i) {
        FbxNode* bone_node = skeleton_nodes[i];
        
        // Find first child bone
        for (int j = 0; j < bone_node->GetChildCount(); ++j) {
            FbxNode* child = bone_node->GetChild(j);
            auto child_it = node_to_index.find(child);
            if (child_it != node_to_index.end()) {
                bone_pool[i].child_bone_index = child_it->second;
                break;
            }
        }
    }
    
    if (verbose) {
        std::cout << "Successfully extracted " << bone_pool.size() << " bones\n";
        
        // Show bone hierarchy
        std::function<void(uint32_t, int)> printHierarchy = [&](uint32_t bone_idx, int depth) {
            if (bone_idx >= bone_pool.size()) return;
            
            for (int i = 0; i < depth; ++i) std::cout << "  ";
            std::cout << "- Bone " << bone_idx;
            if (bone_idx < skeleton_nodes.size()) {
                std::cout << " (" << skeleton_nodes[bone_idx]->GetName() << ")";
            }
            std::cout << "\n";
            
            // Find children
            for (uint32_t i = 0; i < bone_pool.size(); ++i) {
                if (bone_pool[i].parent_bone_index == bone_idx && i != bone_idx) {
                    printHierarchy(i, depth + 1);
                }
            }
        };
        
        std::cout << "Bone hierarchy:\n";
        // Find root bones (bones that are their own parent)
        for (uint32_t i = 0; i < bone_pool.size(); ++i) {
            if (bone_pool[i].parent_bone_index == i) {
                printHierarchy(i, 0);
            }
        }
    }
}


void LinkVertexSkinning(const std::vector<FbxNode*>& mesh_nodes, 
                       const std::vector<FbxNode*>& skeleton_nodes,
                       std::vector<AnchorPointBuffer>& anchor_pool,
                       const std::map<FbxNode*, uint32_t>& node_to_bone_index,
                       bool verbose) {
    
    if (verbose) {
        std::cout << "=== ENHANCED VERTEX SKINNING ===\n";
        std::cout << "Mesh nodes: " << mesh_nodes.size() << "\n";
        std::cout << "Skeleton nodes: " << skeleton_nodes.size() << "\n";
        std::cout << "Anchor points: " << anchor_pool.size() << "\n";
    }
    
    if (mesh_nodes.empty() || anchor_pool.empty()) {
        if (verbose) {
            std::cout << "No meshes or anchors to process for skinning\n";
        }
        return;
    }
    
    size_t anchor_offset = 0;
    
    for (size_t mesh_idx = 0; mesh_idx < mesh_nodes.size(); ++mesh_idx) {
        FbxNode* mesh_node = mesh_nodes[mesh_idx];
        FbxMesh* mesh = mesh_node->GetMesh();
        
        if (!mesh) {
            if (verbose) {
                std::cout << "Mesh node " << mesh_idx << " has no mesh data\n";
            }
            continue;
        }
        
        int vertex_count = mesh->GetControlPointsCount();
        
        if (verbose) {
            std::cout << "Processing skinning for mesh " << mesh_idx << ": " << mesh_node->GetName() 
                      << " (" << vertex_count << " vertices)\n";
        }
        
        // Check if this mesh has skin deformers
        int deformer_count = mesh->GetDeformerCount(FbxDeformer::eSkin);
        
        if (deformer_count > 0) {
            if (verbose) {
                std::cout << "  Found " << deformer_count << " skin deformers\n";
            }
            
            for (int def_idx = 0; def_idx < deformer_count; ++def_idx) {
                FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(def_idx, FbxDeformer::eSkin));
                if (!skin) continue;
                
                int cluster_count = skin->GetClusterCount();
                if (verbose) {
                    std::cout << "    Skin deformer " << def_idx << " has " << cluster_count << " clusters\n";
                }
                
                for (int cluster_idx = 0; cluster_idx < cluster_count; ++cluster_idx) {
                    FbxCluster* cluster = skin->GetCluster(cluster_idx);
                    if (!cluster) continue;
                    
                    FbxNode* bone_node = cluster->GetLink();
                    if (!bone_node) continue;
                    
                    auto bone_it = node_to_bone_index.find(bone_node);
                    if (bone_it == node_to_bone_index.end()) {
                        if (verbose) {
                            std::cout << "      Cluster " << cluster_idx << " links to unknown bone: " 
                                      << bone_node->GetName() << "\n";
                        }
                        continue;
                    }
                    
                    uint32_t bone_index = bone_it->second;
                    int* indices = cluster->GetControlPointIndices();
                    double* weights = cluster->GetControlPointWeights();
                    int index_count = cluster->GetControlPointIndicesCount();
                    
                    if (verbose) {
                        std::cout << "      Cluster " << cluster_idx << " (bone: " << bone_node->GetName() 
                                  << ", index: " << bone_index << ") affects " << index_count << " vertices\n";
                    }
                    
                    // Apply skinning weights to anchor points
                    for (int i = 0; i < index_count; ++i) {
                        int vertex_index = indices[i];
                        double weight = weights[i];
                        
                        if (vertex_index >= 0 && vertex_index < vertex_count && weight > 0.01) {
                            size_t anchor_index = anchor_offset + vertex_index;
                            
                            if (anchor_index < anchor_pool.size()) {
                                // Assign bone to this anchor (using strongest influence)
                                if (weight > 0.5) { // Only assign if this bone has significant influence
                                    anchor_pool[anchor_index].bone_index = bone_index;
                                    
                                    if (verbose && i < 5) { // Show first few assignments
                                        std::cout << "        Vertex " << vertex_index 
                                                  << " -> Anchor " << anchor_index 
                                                  << " -> Bone " << bone_index 
                                                  << " (weight: " << weight << ")\n";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else {
            if (verbose) {
                std::cout << "  No skin deformers found, using default bone assignment\n";
            }
            
            // No skinning data - assign all vertices to first bone or default
            uint32_t default_bone = 0;
            if (!node_to_bone_index.empty()) {
                default_bone = node_to_bone_index.begin()->second;
            }
            
            for (int v = 0; v < vertex_count; ++v) {
                size_t anchor_index = anchor_offset + v;
                if (anchor_index < anchor_pool.size()) {
                    anchor_pool[anchor_index].bone_index = default_bone;
                }
            }
            
            if (verbose) {
                std::cout << "    Assigned all " << vertex_count << " vertices to bone " << default_bone << "\n";
            }
        }
        
        anchor_offset += vertex_count;
    }
    
    if (verbose) {
        std::cout << "Vertex skinning complete\n";
        
        // Show statistics
        std::map<uint32_t, int> bone_usage;
        for (const auto& anchor : anchor_pool) {
            bone_usage[anchor.bone_index]++;
        }
        
        std::cout << "Bone usage statistics:\n";
        for (const auto& usage : bone_usage) {
            std::cout << "  Bone " << usage.first << ": " << usage.second << " anchors\n";
        }
    }
}
#endif

int main(int argc, char* argv[]) {
    Args args;
    if (!args.ParseEnhanced(argc, argv)) {
        return 1;
    }

    if (args.test_mode) {
        std::cout << "Running in test mode...\n";
        ProceduralObjectCollection test_collection = CreateTestObject();
        
        if (!args.output_file.empty()) {
            if (test_collection.SaveToFile(args.output_file)) {
                std::cout << "✅ Test object saved to: " << args.output_file << "\n";
            } else {
                std::cout << "❌ Failed to save test object\n";
                return 1;
            }
        }
        return 0;
    }
    
    bool is_fbx, is_psf;
    if (!IsValidInputFile(args.input_file, is_fbx, is_psf)) {
        std::cout << "❌ Invalid input file: " << args.input_file << "\n";
        return 1;
    }

    if (args.convert_mode) {
        if (!is_psf) {
            std::cout << "❌ Convert mode only supports PSF files\n";
            return 1;
        }
        return ConvertPSFToOBJ(args.input_file, args.output_file, args.verbose || args.debug_colors, 4) ? 0 : 1;
    }

    if (args.simplify_mode) {
        if (!is_psf) {
            std::cout << "❌ Simplify mode only supports PSF files\n";
            return 1;
        }

        ProceduralObjectCollection collection;
        if (!collection.LoadFromFile(args.input_file)) {
            std::cout << "❌ Failed to load PSF file: " << args.input_file << "\n";
            return 1;
        }

        ProceduralObjectCollection simplified;
        if (args.color_preserve_mode || args.color_aware_mode) {
            if (args.debug_colors) {
                std::cout << "Using enhanced color-aware simplification\n";
                std::cout << "Color tolerance: " << args.color_tolerance << "\n";
            }
            simplified = SimplifyPSF(collection, args.verbose || args.debug_colors, 
                                    args.angle_threshold, 
                                    args.plane_tolerance, 
                                    args.color_tolerance);
        } else {
            simplified = SimplifyPSF(collection, args.verbose || args.debug_colors, 
                                    args.angle_threshold, 
                                    args.plane_tolerance);
        }

        if (!simplified.SaveToFile(args.output_file)) {
            std::cout << "❌ Failed to save simplified PSF file: " << args.output_file << "\n";
            return 1;
        }

        std::cout << "✅ Successfully simplified PSF";
        if (args.color_preserve_mode) {
            std::cout << " with color preservation";
        }
        std::cout << ": " << args.output_file << "\n";
        
        if (args.debug_colors || args.color_preserve_mode) {
            VerifyColorPreservation(args.output_file, true);
        }
        
        return 0;
    }

    if (args.ultra_simplify_mode) {
        if (!is_psf) {
            std::cout << "❌ Ultra simplify mode only supports PSF files\n";
            return 1;
        }

        ProceduralObjectCollection collection;
        if (!collection.LoadFromFile(args.input_file)) {
            std::cout << "❌ Failed to load PSF file: " << args.input_file << "\n";
            return 1;
        }

        ProceduralObjectCollection ultra_simplified = UltraSimplifyPSF(collection, args.verbose || args.debug_colors);

        if (!ultra_simplified.SaveToFile(args.output_file)) {
            std::cout << "❌ Failed to save ultra simplified PSF file: " << args.output_file << "\n";
            return 1;
        }

        std::cout << "✅ Successfully ultra simplified PSF: " << args.output_file << "\n";
        return 0;
    }

    if (args.analyze_mode) {
        if (!is_psf) {
            std::cout << "❌ Analyze mode only supports PSF files\n";
            return 1;
        }

        ProceduralObjectCollection collection;
        if (!collection.LoadFromFile(args.input_file)) {
            std::cout << "❌ Failed to load PSF file: " << args.input_file << "\n";
            return 1;
        }

        AnalyzeSimplificationPotential(collection, args.verbose || args.debug_colors);
        
        if (args.debug_colors) {
            VerifyColorPreservation(args.input_file, true);
        }
        
        return 0;
    }

    if (args.view_mode) {
        if (!is_psf) {
            std::cout << "❌ View mode only supports PSF files\n";
            return 1;
        }
        
        if (args.gl_view_mode) {
            PSFOpenGLViewer::ViewerConfig config;
            config.show_fps = args.verbose || args.debug_colors;
            config.show_stats = args.verbose || args.debug_colors;
            return PSFViewerIntegration::ViewPSFFile(args.input_file, config) ? 0 : 1;
        } else {
            return ViewPSFDirectly(args.input_file, args.verbose || args.debug_colors) ? 0 : 1;
        }
    }

#ifndef NO_FBX_SDK
    if (is_fbx) {
        if (args.debug_colors) {
            std::cout << "=== FBX TO PSF CONVERSION WITH COLOR PRESERVATION ===\n";
            std::cout << "Color preserve mode: " << (args.color_preserve_mode ? "ON" : "OFF") << "\n";
            std::cout << "Debug colors: ON\n";
            std::cout << "Force original colors: " << (args.force_original_colors ? "ON" : "OFF") << "\n";
        }
        
        FbxManager* manager = FbxManager::Create();
        FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
        manager->SetIOSettings(ios);

        FbxImporter* importer = FbxImporter::Create(manager, "");
        if (!importer->Initialize(args.input_file.c_str(), -1, manager->GetIOSettings())) {
            std::cout << "❌ Failed to initialize FBX importer: " << importer->GetStatus().GetErrorString() << "\n";
            manager->Destroy();
            return 1;
        }

        FbxScene* scene = FbxScene::Create(manager, "scene");
        if (!importer->Import(scene)) {
            std::cout << "❌ Failed to import FBX file: " << importer->GetStatus().GetErrorString() << "\n";
            importer->Destroy();
            manager->Destroy();
            return 1;
        }

        importer->Destroy();

        std::vector<EnhancedMeshData> meshes;
        std::vector<FbxNode*> mesh_nodes;
        
        // FIXED: Pass fbxPath as the fourth parameter
        ExtractEnhancedMeshFromNode(scene->GetRootNode(), meshes, mesh_nodes, 
                                   args.verbose || args.debug_colors, args.input_file);

        std::vector<BoneBuffer> bone_pool;
        ExtractSkeletonFromScene(scene, bone_pool, args.verbose || args.debug_colors);

        ProceduralObjectCollection combined_collection;
        combined_collection.object_name = args.input_file.substr(args.input_file.find_last_of("/\\") + 1);
        if (args.color_preserve_mode) {
            combined_collection.creator_info = "FBX to PSF Converter with Enhanced Color Preservation";
        } else {
            combined_collection.creator_info = "FBX to PSF Converter";
        }

        // Process meshes with enhanced color preservation
        for (const auto& mesh : meshes) {
            if (args.debug_colors) {
                std::cout << "\n=== PROCESSING MESH: " << mesh.name << " ===\n";
                std::cout << "Vertices: " << mesh.vertices.size() << "\n";
                std::cout << "Colors: " << mesh.colors.size() << "\n";
                std::cout << "Faces: " << mesh.faces.size() << "\n";
                
                if (!mesh.colors.empty()) {
                    // Calculate color statistics
                    Vec3 min_color(1.0f, 1.0f, 1.0f), max_color(0.0f, 0.0f, 0.0f);
                    Vec3 avg_color(0.0f, 0.0f, 0.0f);
                    
                    for (const auto& color : mesh.colors) {
                        min_color.x = std::min(min_color.x, color.x);
                        min_color.y = std::min(min_color.y, color.y);
                        min_color.z = std::min(min_color.z, color.z);
                        max_color.x = std::max(max_color.x, color.x);
                        max_color.y = std::max(max_color.y, color.y);
                        max_color.z = std::max(max_color.z, color.z);
                        avg_color.x += color.x;
                        avg_color.y += color.y;
                        avg_color.z += color.z;
                    }
                    
                    avg_color.x /= mesh.colors.size();
                    avg_color.y /= mesh.colors.size();
                    avg_color.z /= mesh.colors.size();
                    
                    std::cout << "Color range: (" << min_color.x << ", " << min_color.y << ", " << min_color.z 
                              << ") to (" << max_color.x << ", " << max_color.y << ", " << max_color.z << ")\n";
                    std::cout << "Average color: (" << avg_color.x << ", " << avg_color.y << ", " << avg_color.z << ")\n";
                    
                    std::cout << "Sample colors:\n";
                    for (size_t i = 0; i < std::min<size_t>(5, mesh.colors.size()); ++i) {
                        std::cout << "  Color " << i << ": (" << mesh.colors[i].x 
                                  << ", " << mesh.colors[i].y << ", " << mesh.colors[i].z << ")\n";
                    }
                }
            }
            
            ProceduralObjectCollection mesh_collection = SmartMeshConverter::ConvertMeshToProcedural(
                mesh, args.verbose || args.debug_colors);
            
            // Calculate anchor offset for this mesh
            size_t anchor_offset = combined_collection.anchor_pool.size();
            
            // Add anchors
            combined_collection.anchor_pool.insert(
                combined_collection.anchor_pool.end(),
                mesh_collection.anchor_pool.begin(),
                mesh_collection.anchor_pool.end()
            );
            
            // Add surfaces with corrected anchor indices
            for (auto surface : mesh_collection.surface_pool) {
                for (int i = 0; i < 4; ++i) {
                    surface.anchor_point_indices[i] += static_cast<uint16_t>(anchor_offset);
                }
                combined_collection.surface_pool.push_back(surface);
            }
            
            if (args.debug_colors) {
                std::cout << "Added " << mesh_collection.anchor_pool.size() << " anchors and " 
                          << mesh_collection.surface_pool.size() << " surfaces\n";
            }
        }

        combined_collection.bone_pool = bone_pool;

        // Simplified vertex skinning
        if (!mesh_nodes.empty() && !bone_pool.empty()) {
            std::map<FbxNode*, uint32_t> node_to_bone_index;
            for (size_t i = 0; i < bone_pool.size(); ++i) {
                if (i < mesh_nodes.size()) {
                    node_to_bone_index[mesh_nodes[i]] = static_cast<uint32_t>(i);
                }
            }
            LinkVertexSkinning(mesh_nodes, {}, combined_collection.anchor_pool, node_to_bone_index, 
                              args.verbose || args.debug_colors);
        }

        combined_collection.UpdateHeader();

        if (!combined_collection.SaveToFile(args.output_file)) {
            std::cout << "❌ Failed to save PSF file: " << args.output_file << "\n";
            manager->Destroy();
            return 1;
        }

        std::cout << "✅ Successfully converted FBX to PSF";
        if (args.color_preserve_mode) {
            std::cout << " with color preservation";
        }
        std::cout << ": " << args.output_file << "\n";
        
        if (args.debug_colors || args.color_preserve_mode) {
            VerifyColorPreservation(args.output_file, true);
        }

        if (args.verbose || args.debug_colors) {
            std::cout << "\nOutput stats:\n";
            std::cout << "  Anchors: " << combined_collection.anchor_pool.size() << "\n";
            std::cout << "  Surfaces: " << combined_collection.surface_pool.size() << "\n";
            std::cout << "  Bones: " << combined_collection.bone_pool.size() << "\n";
            
            if (args.debug_colors) {
                std::cout << "\nTo verify colors are preserved:\n";
                std::cout << "  View: " << argv[0] << " --view " << args.output_file << " --debug-colors\n";
                std::cout << "  Export: " << argv[0] << " --convert " << args.output_file << " --debug-colors\n";
            }
        }

        manager->Destroy();
        return 0;
    }
#endif

    if (is_psf) {
        return VerifyPSF(args.input_file, args.verbose || args.debug_colors) ? 0 : 1;
    }

    std::cout << "❌ Unsupported operation\n";
    return 1;
}