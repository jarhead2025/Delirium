#pragma once
#include <stdint.h>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>  // For std::cout
#include <ctime>
#include <cstring>

// =============================================================================
// STRUCT PACKING MACROS
// =============================================================================

#ifndef PROCEDURAL_ENGINE_PACKED_STRUCT_DEFINED
#define PROCEDURAL_ENGINE_PACKED_STRUCT_DEFINED
#endif

#pragma pack(push, 1)
#define PACKED_STRUCT
#define PACK_END

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

enum SurfaceEquationType : uint8_t;
enum CombinationOperation : uint8_t;
enum ColorEquationType : uint8_t;
enum SurfaceModifier : uint8_t;

// =============================================================================
// BUFFER STRUCTURES - EXACTLY 64 bytes each
// =============================================================================

// Surface Buffer (64 bytes)
typedef struct PACKED_STRUCT {
    uint16_t anchor_point_indices[4];      // 8 bytes
    uint16_t surface_definition_index;     // 2 bytes
    float    eqn_params[8];                // 32 bytes
    uint8_t  secondary_surface_type;       // 1 byte
    uint8_t  combination_param;            // 1 byte
    uint8_t  deformation_strength;         // 1 byte
    uint8_t  animation_param;              // 1 byte
    uint8_t  color_eqn_r, color_param_r;   // 2 bytes
    uint8_t  color_eqn_g, color_param_g;   // 2 bytes
    uint8_t  color_eqn_b, color_param_b;   // 2 bytes
    uint8_t  roughness;                    // 1 byte
    uint8_t  metallic;                     // 1 byte
    uint8_t  emissive_strength;            // 1 byte
    uint8_t  specular_tint;                // 1 byte
    uint8_t  normal_mode;                  // 1 byte
    uint8_t  lod_bias;                     // 1 byte
    uint8_t  surface_modifiers;            // 1 byte - Added missing field
    uint8_t  reserved[5];                  // 5 bytes (reduced to maintain 64 total)
} ProceduralSurfaceBuffer PACK_END;        // Total: 64 bytes

// Anchor Point Buffer (64 bytes)
typedef struct PACKED_STRUCT {
    uint32_t bone_index;                   // 4 bytes - bone transform reference
    
    // CONNECTIVITY: 4 separate curve connections to other anchors
    uint16_t connected_anchors[4];         // 8 bytes - indices of 4 connected anchor points
    uint8_t  curve_types[4];               // 4 bytes - curve equation type for each connection
    
    // CURVE PARAMETERS: Each connection gets its own parameter set
    uint8_t  curve_params_0[4];            // 4 bytes - parameters for connection 0
    uint8_t  curve_params_1[4];            // 4 bytes - parameters for connection 1  
    uint8_t  curve_params_2[4];            // 4 bytes - parameters for connection 2
    uint8_t  curve_params_3[4];            // 4 bytes - parameters for connection 3
    
    // POSITION AND ORIENTATION
    uint16_t pos_xyz[3];                   // 6 bytes - position (encoded)
    int16_t  delta_xyz[3];                 // 6 bytes - position delta/deformation
    uint8_t  normal[3];                    // 3 bytes - surface normal at this point
    uint8_t  tangent_u[2];                 // 2 bytes - U-direction tangent (compressed)
    uint8_t  tangent_v[2];                 // 2 bytes - V-direction tangent (compressed)
    
    // COLOR (ADDED - anchor points now have their own color)
    uint8_t  color[3];                     // 3 bytes - RGB color at this anchor point
    
    // MATERIAL AND ANIMATION  
    uint8_t  roughness;                    // 1 byte - material roughness at this point
    uint8_t  metallic;                     // 1 byte - material metallic value
    uint8_t  anim_weight;                  // 1 byte - animation influence weight
    
    // CURVATURE AND CONTINUITY
    uint8_t  curvature_u;                  // 1 byte - curvature in U direction
    uint8_t  curvature_v;                  // 1 byte - curvature in V direction  
    uint8_t  continuity_flags;             // 1 byte - C0/C1/C2 continuity requirements
    
    uint8_t  reserved[4];                  // 4 bytes - for future expansion
} AnchorPointBuffer PACK_END;  

// Bone Buffer (64 bytes)
typedef struct PACKED_STRUCT {
    uint32_t parent_bone_index;            // 4 bytes
    uint32_t child_bone_index;             // 4 bytes
    uint16_t pos_xyz[3];                   // 6 bytes
    uint32_t orientation;                  // 4 bytes
    float    ang_velocity[3];              // 12 bytes
    float    ang_accel[3];                 // 12 bytes
    float    physical_params[2];           // 8 bytes
    uint8_t  flags;                        // 1 byte
    uint8_t  reserved[13];                 // 13 bytes
} BoneBuffer PACK_END;                     // Total: 64 bytes

// Object Descriptor Buffer (64 bytes)
typedef struct PACKED_STRUCT {
    float    position[3];                  // 12 bytes
    float    velocity[3];                  // 12 bytes
    float    acceleration[3];              // 12 bytes
    uint32_t orientation;                  // 4 bytes
    float    bounding_sphere_radius;       // 4 bytes
    uint32_t surface_buffer_start_index;   // 4 bytes
    uint16_t surface_buffer_count;         // 2 bytes
    uint16_t bone_buffer_start_index;      // 2 bytes
    uint16_t bone_buffer_count;            // 2 bytes
    uint16_t state_flags;                  // 2 bytes
    uint8_t  object_type_id;               // 1 byte
    uint8_t  lod_level;                    // 1 byte
    uint8_t  reserved[6];                  // 6 bytes
} ObjectDescriptorBuffer PACK_END;         // Total: 64 bytes

// File Header (EXACTLY 64 bytes)
typedef struct PACKED_STRUCT {
    uint32_t magic;                        // 4 bytes - "PSF\0"
    uint32_t version;                      // 4 bytes
    uint32_t object_count;                 // 4 bytes
    uint32_t bone_pool_size;               // 4 bytes
    uint32_t anchor_pool_size;             // 4 bytes
    uint32_t surface_pool_size;            // 4 bytes
    uint32_t metadata_size;                // 4 bytes
    uint32_t flags;                        // 4 bytes
    float    bounding_box[6];              // 24 bytes - [min_x, min_y, min_z, max_x, max_y, max_z]
    uint32_t creation_timestamp_low;       // 4 bytes - Split 64-bit timestamp
    uint32_t creation_timestamp_high;      // 4 bytes
} ProceduralSurfaceFileHeader PACK_END;    // Total: EXACTLY 64 bytes

// =============================================================================
// FILE FORMAT CONSTANTS
// =============================================================================

constexpr uint32_t PSF_MAGIC = 0x00465350; // "PSF\0" as little-endian uint32
constexpr uint32_t PSF_VERSION = 1;

// =============================================================================
// PROCEDURAL OBJECT COLLECTION
// =============================================================================

struct ProceduralObjectCollection {
    // Header
    ProceduralSurfaceFileHeader header;
    
    // Metadata
    std::string object_name;
    std::string creator_info;
    std::string description;
    
    // Buffer pools
    std::vector<BoneBuffer> bone_pool;
    std::vector<AnchorPointBuffer> anchor_pool;
    std::vector<ProceduralSurfaceBuffer> surface_pool;
    std::vector<ObjectDescriptorBuffer> object_descriptors;
    
    // =============================================================================
    // CONSTRUCTORS
    // =============================================================================
    
    ProceduralObjectCollection();
    ProceduralObjectCollection(const std::string& name, const std::string& creator = "");
    
    // =============================================================================
    // BUFFER MANAGEMENT
    // =============================================================================
    
    uint32_t AddBone(const BoneBuffer& bone);
    uint32_t AddAnchor(const AnchorPointBuffer& anchor);
    uint32_t AddSurface(const ProceduralSurfaceBuffer& surface);
    uint32_t AddObject(const ObjectDescriptorBuffer& object);
    
    void Clear();
    void Reserve(uint32_t bones, uint32_t anchors, uint32_t surfaces, uint32_t objects);
    
    // =============================================================================
    // FILE I/O
    // =============================================================================
    
    bool SaveToFile(const std::string& filename);
    bool LoadFromFile(const std::string& filename);
    
    // =============================================================================
    // UTILITY FUNCTIONS
    // =============================================================================
    
    size_t CalculateFileSize() const;
    void UpdateHeader();
    void CalculateBoundingBox();
    bool Validate() const;
    
    // =============================================================================
    // DEBUG FUNCTIONS
    // =============================================================================
    
    void ValidateStructSizes() const;
    
    // =============================================================================
    // QUICK OBJECT CREATION
    // =============================================================================
    
    uint32_t CreateCube(float size = 1.0f, 
                       SurfaceEquationType top_type = static_cast<SurfaceEquationType>(32), // EQUATION_BEZIER_PATCH
                       const float* top_params = nullptr);
    
    uint32_t CreateSphere(float radius = 1.0f, int segments = 8);
    uint32_t CreatePlane(float width = 2.0f, float height = 2.0f);
    
private:
    bool WriteHeader(std::ofstream& file);
    bool ReadHeader(std::ifstream& file);
    bool WriteMetadata(std::ofstream& file);
    bool ReadMetadata(std::ifstream& file);
    size_t CalculateMetadataSize() const;
    
    // Helper to reconstruct 64-bit timestamp
    uint64_t GetTimestamp() const {
        return (static_cast<uint64_t>(header.creation_timestamp_high) << 32) | 
               static_cast<uint64_t>(header.creation_timestamp_low);
    }
    
    void SetTimestamp(uint64_t timestamp) {
        header.creation_timestamp_low = static_cast<uint32_t>(timestamp & 0xFFFFFFFF);
        header.creation_timestamp_high = static_cast<uint32_t>(timestamp >> 32);
    }
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline ProceduralObjectCollection::ProceduralObjectCollection() {
    memset(&header, 0, sizeof(header));
    header.magic = PSF_MAGIC;
    header.version = PSF_VERSION;
    
    // Split timestamp into two 32-bit values
    SetTimestamp(std::time(nullptr));
}

inline ProceduralObjectCollection::ProceduralObjectCollection(const std::string& name, const std::string& creator) 
    : ProceduralObjectCollection() {
    object_name = name;
    creator_info = creator;
}

inline uint32_t ProceduralObjectCollection::AddBone(const BoneBuffer& bone) {
    uint32_t index = static_cast<uint32_t>(bone_pool.size());
    bone_pool.push_back(bone);
    return index;
}

inline uint32_t ProceduralObjectCollection::AddAnchor(const AnchorPointBuffer& anchor) {
    uint32_t index = static_cast<uint32_t>(anchor_pool.size());
    anchor_pool.push_back(anchor);
    return index;
}

inline uint32_t ProceduralObjectCollection::AddSurface(const ProceduralSurfaceBuffer& surface) {
    uint32_t index = static_cast<uint32_t>(surface_pool.size());
    surface_pool.push_back(surface);
    return index;
}

inline uint32_t ProceduralObjectCollection::AddObject(const ObjectDescriptorBuffer& object) {
    uint32_t index = static_cast<uint32_t>(object_descriptors.size());
    object_descriptors.push_back(object);
    return index;
}

inline void ProceduralObjectCollection::Clear() {
    bone_pool.clear();
    anchor_pool.clear();
    surface_pool.clear();
    object_descriptors.clear();
    object_name.clear();
    creator_info.clear();
    description.clear();
}

inline void ProceduralObjectCollection::Reserve(uint32_t bones, uint32_t anchors, uint32_t surfaces, uint32_t objects) {
    bone_pool.reserve(bones);
    anchor_pool.reserve(anchors);
    surface_pool.reserve(surfaces);
    object_descriptors.reserve(objects);
}

inline void ProceduralObjectCollection::UpdateHeader() {
    header.object_count = static_cast<uint32_t>(object_descriptors.size());
    header.bone_pool_size = static_cast<uint32_t>(bone_pool.size());
    header.anchor_pool_size = static_cast<uint32_t>(anchor_pool.size());
    header.surface_pool_size = static_cast<uint32_t>(surface_pool.size());
    header.metadata_size = static_cast<uint32_t>(CalculateMetadataSize());
}

inline size_t ProceduralObjectCollection::CalculateFileSize() const {
    return sizeof(ProceduralSurfaceFileHeader) +
           CalculateMetadataSize() +
           bone_pool.size() * sizeof(BoneBuffer) +
           anchor_pool.size() * sizeof(AnchorPointBuffer) +
           surface_pool.size() * sizeof(ProceduralSurfaceBuffer) +
           object_descriptors.size() * sizeof(ObjectDescriptorBuffer);
}

inline size_t ProceduralObjectCollection::CalculateMetadataSize() const {
    return 4 + object_name.length() +    // Length + string
           4 + creator_info.length() +    // Length + string  
           4 + description.length();      // Length + string
}

inline void ProceduralObjectCollection::ValidateStructSizes() const {
    std::cout << "Struct sizes:\n";
    std::cout << "ProceduralSurfaceBuffer: " << sizeof(ProceduralSurfaceBuffer) << " bytes\n";
    std::cout << "AnchorPointBuffer: " << sizeof(AnchorPointBuffer) << " bytes\n";
    std::cout << "BoneBuffer: " << sizeof(BoneBuffer) << " bytes\n";
    std::cout << "ObjectDescriptorBuffer: " << sizeof(ObjectDescriptorBuffer) << " bytes\n";
    std::cout << "ProceduralSurfaceFileHeader: " << sizeof(ProceduralSurfaceFileHeader) << " bytes\n";
}

inline bool ProceduralObjectCollection::SaveToFile(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    UpdateHeader();
    
    // Write header
    if (!WriteHeader(file)) return false;
    
    // Write metadata
    if (!WriteMetadata(file)) return false;
    
    // Write buffer pools
    file.write(reinterpret_cast<const char*>(bone_pool.data()), 
               bone_pool.size() * sizeof(BoneBuffer));
    file.write(reinterpret_cast<const char*>(anchor_pool.data()), 
               anchor_pool.size() * sizeof(AnchorPointBuffer));
    file.write(reinterpret_cast<const char*>(surface_pool.data()), 
               surface_pool.size() * sizeof(ProceduralSurfaceBuffer));
    file.write(reinterpret_cast<const char*>(object_descriptors.data()), 
               object_descriptors.size() * sizeof(ObjectDescriptorBuffer));
    
    return file.good();
}

inline bool ProceduralObjectCollection::LoadFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read and validate header
    if (!ReadHeader(file)) return false;
    if (header.magic != PSF_MAGIC) return false;
    
    // Read metadata
    if (!ReadMetadata(file)) return false;
    
    // Read buffer pools
    bone_pool.resize(header.bone_pool_size);
    file.read(reinterpret_cast<char*>(bone_pool.data()), 
              bone_pool.size() * sizeof(BoneBuffer));
              
    anchor_pool.resize(header.anchor_pool_size);
    file.read(reinterpret_cast<char*>(anchor_pool.data()), 
              anchor_pool.size() * sizeof(AnchorPointBuffer));
              
    surface_pool.resize(header.surface_pool_size);
    file.read(reinterpret_cast<char*>(surface_pool.data()), 
              surface_pool.size() * sizeof(ProceduralSurfaceBuffer));
              
    object_descriptors.resize(header.object_count);
    file.read(reinterpret_cast<char*>(object_descriptors.data()), 
              object_descriptors.size() * sizeof(ObjectDescriptorBuffer));
    
    return file.good();
}

inline bool ProceduralObjectCollection::WriteHeader(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    return file.good();
}

inline bool ProceduralObjectCollection::ReadHeader(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    return file.good();
}

inline bool ProceduralObjectCollection::WriteMetadata(std::ofstream& file) {
    // Write string lengths and data
    uint32_t len = static_cast<uint32_t>(object_name.length());
    file.write(reinterpret_cast<const char*>(&len), 4);
    file.write(object_name.c_str(), len);
    
    len = static_cast<uint32_t>(creator_info.length());
    file.write(reinterpret_cast<const char*>(&len), 4);
    file.write(creator_info.c_str(), len);
    
    len = static_cast<uint32_t>(description.length());
    file.write(reinterpret_cast<const char*>(&len), 4);
    file.write(description.c_str(), len);
    
    return file.good();
}

inline bool ProceduralObjectCollection::ReadMetadata(std::ifstream& file) {
    // Read strings
    uint32_t len;
    
    file.read(reinterpret_cast<char*>(&len), 4);
    object_name.resize(len);
    file.read(&object_name[0], len);
    
    file.read(reinterpret_cast<char*>(&len), 4);
    creator_info.resize(len);
    file.read(&creator_info[0], len);
    
    file.read(reinterpret_cast<char*>(&len), 4);
    description.resize(len);
    file.read(&description[0], len);
    
    return file.good();
}

// =============================================================================
// CLEANUP PACKING
// =============================================================================

#if defined(_MSC_VER)
    #pragma pack(pop)
#endif

// =============================================================================
// SIZE VALIDATION - SHOULD NOW PASS
// =============================================================================

static_assert(sizeof(ProceduralSurfaceBuffer) == 64, "ProceduralSurfaceBuffer must be 64 bytes");
static_assert(sizeof(AnchorPointBuffer) == 64, "AnchorPointBuffer must be 64 bytes");
static_assert(sizeof(BoneBuffer) == 64, "BoneBuffer must be 64 bytes");
static_assert(sizeof(ObjectDescriptorBuffer) == 64, "ObjectDescriptorBuffer must be 64 bytes");
static_assert(sizeof(ProceduralSurfaceFileHeader) == 64, "ProceduralSurfaceFileHeader must be 64 bytes");