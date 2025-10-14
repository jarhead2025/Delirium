using System;
using System.Runtime.InteropServices;
using UnityEngine;
using Unity.Collections;
using System.Collections.Generic;
namespace ProceduralSurface.Unity
{
    /// <summary>
    /// Unity C# data structures that exactly match the C++ PSF format
    /// All structures are exactly 64 bytes to match the C++ implementation
    /// Place this file at: Assets/Scripts/PSF/PSFDataStructures.cs
    /// </summary>

    // =============================================================================
    // PSF FILE FORMAT CONSTANTS
    // =============================================================================
    
    public static class PSFConstants
    {
        public const uint PSF_MAGIC = 0x00465350; // "PSF\0" as little-endian uint32
        public const uint PSF_VERSION = 1;
        public const int STRUCT_SIZE = 64; // All PSF structures are exactly 64 bytes
    }

    // =============================================================================
    // SURFACE BUFFER (64 bytes) - Matches ProceduralSurfaceBuffer
    // =============================================================================
    
    [StructLayout(LayoutKind.Sequential, Pack = 1, Size = 64)]
    public struct ProceduralSurfaceBuffer
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public ushort[] anchor_point_indices;      // 8 bytes
        
        public ushort surface_definition_index;    // 2 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public float[] eqn_params;                 // 32 bytes
        
        public byte secondary_surface_type;        // 1 byte
        public byte combination_param;             // 1 byte
        public byte deformation_strength;          // 1 byte
        public byte animation_param;               // 1 byte
        
        public byte color_eqn_r;                   // 1 byte
        public byte color_param_r;                 // 1 byte
        public byte color_eqn_g;                   // 1 byte
        public byte color_param_g;                 // 1 byte
        public byte color_eqn_b;                   // 1 byte
        public byte color_param_b;                 // 1 byte
        
        public byte roughness;                     // 1 byte
        public byte metallic;                      // 1 byte
        public byte emissive_strength;             // 1 byte
        public byte specular_tint;                 // 1 byte
        public byte normal_mode;                   // 1 byte
        public byte lod_bias;                      // 1 byte
        public byte surface_modifiers;             // 1 byte
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 5)]
        public byte[] reserved;                    // 5 bytes
        
        // Total: 64 bytes
    }

    // =============================================================================
    // ANCHOR POINT BUFFER (64 bytes) - Matches AnchorPointBuffer
    // =============================================================================
    
    [StructLayout(LayoutKind.Sequential, Pack = 1, Size = 64)]
    public struct AnchorPointBuffer
    {
        public uint bone_index;                    // 4 bytes - bone transform reference
        
        // CONNECTIVITY: 4 separate curve connections to other anchors
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public ushort[] connected_anchors;         // 8 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public byte[] curve_types;                 // 4 bytes
        
        // CURVE PARAMETERS: Each connection gets its own parameter set
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public byte[] curve_params_0;              // 4 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public byte[] curve_params_1;              // 4 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public byte[] curve_params_2;              // 4 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public byte[] curve_params_3;              // 4 bytes
        
        // POSITION AND ORIENTATION
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public ushort[] pos_xyz;                   // 6 bytes - position (encoded)
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public short[] delta_xyz;                  // 6 bytes - position delta/deformation
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public byte[] normal;                      // 3 bytes - surface normal
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
        public byte[] tangent_u;                   // 2 bytes - U-direction tangent
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
        public byte[] tangent_v;                   // 2 bytes - V-direction tangent
        
        // COLOR (anchor points have their own color)
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public byte[] color;                       // 3 bytes - RGB color
        
        // MATERIAL AND ANIMATION
        public byte roughness;                     // 1 byte
        public byte metallic;                      // 1 byte
        public byte anim_weight;                   // 1 byte
        
        // CURVATURE AND CONTINUITY
        public byte curvature_u;                   // 1 byte
        public byte curvature_v;                   // 1 byte
        public byte continuity_flags;              // 1 byte
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public byte[] reserved;                    // 4 bytes
        
        // Total: 64 bytes
    }

    // =============================================================================
    // BONE BUFFER (64 bytes) - Matches BoneBuffer
    // =============================================================================
    
    [StructLayout(LayoutKind.Sequential, Pack = 1, Size = 64)]
    public struct BoneBuffer
    {
        public uint parent_bone_index;             // 4 bytes
        public uint child_bone_index;              // 4 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public ushort[] pos_xyz;                   // 6 bytes
        
        public uint orientation;                   // 4 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] ang_velocity;               // 12 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] ang_accel;                  // 12 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
        public float[] physical_params;            // 8 bytes
        
        public byte flags;                         // 1 byte
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 13)]
        public byte[] reserved;                    // 13 bytes
        
        // Total: 64 bytes
    }

    // =============================================================================
    // OBJECT DESCRIPTOR BUFFER (64 bytes) - Matches ObjectDescriptorBuffer
    // =============================================================================
    
    [StructLayout(LayoutKind.Sequential, Pack = 1, Size = 64)]
    public struct ObjectDescriptorBuffer
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] position;                   // 12 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] velocity;                   // 12 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public float[] acceleration;               // 12 bytes
        
        public uint orientation;                   // 4 bytes
        public float bounding_sphere_radius;       // 4 bytes
        public uint surface_buffer_start_index;    // 4 bytes
        public ushort surface_buffer_count;        // 2 bytes
        public ushort bone_buffer_start_index;     // 2 bytes
        public ushort bone_buffer_count;           // 2 bytes
        public ushort state_flags;                 // 2 bytes
        public byte object_type_id;                // 1 byte
        public byte lod_level;                     // 1 byte
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
        public byte[] reserved;                    // 6 bytes
        
        // Total: 64 bytes
    }

    // =============================================================================
    // FILE HEADER (64 bytes) - Matches ProceduralSurfaceFileHeader
    // =============================================================================
    
    [StructLayout(LayoutKind.Sequential, Pack = 1, Size = 64)]
    public struct PSFHeader
    {
        public uint magic;                         // 4 bytes - "PSF\0"
        public uint version;                       // 4 bytes
        public uint objectCount;                   // 4 bytes
        public uint bonePoolSize;                  // 4 bytes
        public uint anchorPoolSize;                // 4 bytes
        public uint surfacePoolSize;               // 4 bytes
        public uint metadataSize;                  // 4 bytes
        public uint flags;                         // 4 bytes
        
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
        public float[] boundingBox;                // 24 bytes - [min_x, min_y, min_z, max_x, max_y, max_z]
        
        public uint creationTimestampLow;          // 4 bytes
        public uint creationTimestampHigh;         // 4 bytes
        
        // Total: 64 bytes
        
        // Helper properties
        public long CreationTimestamp
        {
            get => ((long)creationTimestampHigh << 32) | creationTimestampLow;
            set
            {
                creationTimestampLow = (uint)(value & 0xFFFFFFFF);
                creationTimestampHigh = (uint)(value >> 32);
            }
        }
        
        public Bounds GetBounds()
        {
            if (boundingBox == null || boundingBox.Length < 6)
                return new Bounds();
            
            Vector3 min = new Vector3(boundingBox[0], boundingBox[1], boundingBox[2]);
            Vector3 max = new Vector3(boundingBox[3], boundingBox[4], boundingBox[5]);
            return new Bounds((min + max) * 0.5f, max - min);
        }
        
        public void SetBounds(Bounds bounds)
        {
            if (boundingBox == null) boundingBox = new float[6];
            
            Vector3 min = bounds.min;
            Vector3 max = bounds.max;
            
            boundingBox[0] = min.x;
            boundingBox[1] = min.y;
            boundingBox[2] = min.z;
            boundingBox[3] = max.x;
            boundingBox[4] = max.y;
            boundingBox[5] = max.z;
        }
    }

    // =============================================================================
    // ENUMERATIONS - Match C++ enums
    // =============================================================================
    
    public enum SurfaceEquationType : byte
    {
        EQUATION_PLANE = 0,
        EQUATION_SPHERE = 1,
        EQUATION_CYLINDER = 2,
        EQUATION_CONE = 3,
        EQUATION_TORUS = 4,
        EQUATION_BEZIER_PATCH = 32,
        EQUATION_NURBS_SURFACE = 33,
        EQUATION_SUBDIVISION_SURFACE = 34,
        EQUATION_METABALL = 64,
        EQUATION_IMPLICIT_SURFACE = 65,
        EQUATION_PARAMETRIC_SURFACE = 66
    }
    
    public enum ColorEquationType : byte
    {
        COLOR_CONSTANT = 0,
        COLOR_LINEAR_U = 1,
        COLOR_LINEAR_V = 2,
        COLOR_BILINEAR = 3,
        COLOR_SINE_WAVE = 4,
        COLOR_COSINE_WAVE = 5,
        COLOR_QUADRATIC_U = 6,
        COLOR_QUADRATIC_V = 7,
        COLOR_RADIAL = 8,
        COLOR_CHECKERBOARD = 9,
        COLOR_NOISE = 10
    }
    
    public enum SurfaceModifier : byte
    {
        MOD_NONE = 0,
        MOD_COLOR = 1,
        MOD_NORMAL = 2,
        MOD_DISPLACEMENT = 4,
        MOD_ANIMATION = 8
    }

    // =============================================================================
    // UTILITY EXTENSIONS
    // =============================================================================
    
    public static class PSFStructExtensions
    {
        /// <summary>
        /// Validate that all PSF structures are exactly 64 bytes
        /// </summary>
        public static void ValidateStructureSizes()
        {
            int expectedSize = PSFConstants.STRUCT_SIZE;
            
            Debug.Assert(Marshal.SizeOf<ProceduralSurfaceBuffer>() == expectedSize, 
                $"ProceduralSurfaceBuffer size mismatch: {Marshal.SizeOf<ProceduralSurfaceBuffer>()} != {expectedSize}");
            
            Debug.Assert(Marshal.SizeOf<AnchorPointBuffer>() == expectedSize, 
                $"AnchorPointBuffer size mismatch: {Marshal.SizeOf<AnchorPointBuffer>()} != {expectedSize}");
            
            Debug.Assert(Marshal.SizeOf<BoneBuffer>() == expectedSize, 
                $"BoneBuffer size mismatch: {Marshal.SizeOf<BoneBuffer>()} != {expectedSize}");
            
            Debug.Assert(Marshal.SizeOf<ObjectDescriptorBuffer>() == expectedSize, 
                $"ObjectDescriptorBuffer size mismatch: {Marshal.SizeOf<ObjectDescriptorBuffer>()} != {expectedSize}");
            
            Debug.Assert(Marshal.SizeOf<PSFHeader>() == expectedSize, 
                $"PSFHeader size mismatch: {Marshal.SizeOf<PSFHeader>()} != {expectedSize}");
            
            Debug.Log($"✅ PSF Structure validation passed - all structures are {expectedSize} bytes");
        }
        
        /// <summary>
        /// Initialize arrays in ProceduralSurfaceBuffer
        /// </summary>
        public static ProceduralSurfaceBuffer InitializeSurface()
        {
            return new ProceduralSurfaceBuffer
            {
                anchor_point_indices = new ushort[4],
                eqn_params = new float[8],
                reserved = new byte[5]
            };
        }
        
        /// <summary>
        /// Initialize arrays in AnchorPointBuffer
        /// </summary>
        public static AnchorPointBuffer InitializeAnchor()
        {
            return new AnchorPointBuffer
            {
                connected_anchors = new ushort[4],
                curve_types = new byte[4],
                curve_params_0 = new byte[4],
                curve_params_1 = new byte[4],
                curve_params_2 = new byte[4],
                curve_params_3 = new byte[4],
                pos_xyz = new ushort[3],
                delta_xyz = new short[3],
                normal = new byte[3],
                tangent_u = new byte[2],
                tangent_v = new byte[2],
                color = new byte[3],
                reserved = new byte[4]
            };
        }
        
        /// <summary>
        /// Initialize arrays in BoneBuffer
        /// </summary>
        public static BoneBuffer InitializeBone()
        {
            return new BoneBuffer
            {
                pos_xyz = new ushort[3],
                ang_velocity = new float[3],
                ang_accel = new float[3],
                physical_params = new float[2],
                reserved = new byte[13]
            };
        }
        
        /// <summary>
        /// Initialize arrays in ObjectDescriptorBuffer
        /// </summary>
        public static ObjectDescriptorBuffer InitializeObject()
        {
            return new ObjectDescriptorBuffer
            {
                position = new float[3],
                velocity = new float[3],
                acceleration = new float[3],
                reserved = new byte[6]
            };
        }
        
        /// <summary>
        /// Initialize arrays in PSFHeader
        /// </summary>
        public static PSFHeader InitializeHeader()
        {
            var header = new PSFHeader
            {
                magic = PSFConstants.PSF_MAGIC,
                version = PSFConstants.PSF_VERSION,
                boundingBox = new float[6]
            };
            header.CreationTimestamp = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
            return header;
        }
    }

// =============================================================================
// PSF COLLECTION WRAPPER
// =============================================================================

/// <summary>
/// Complete PSF collection matching the C++ ProceduralObjectCollection
/// </summary>
public class PSFCollection : IDisposable
{
    public PSFHeader header;
    public BoneBuffer[] bones;              // Changed from NativeArray
    public AnchorPointBuffer[] anchors;     // Changed from NativeArray
    public ProceduralSurfaceBuffer[] surfaces; // Changed from NativeArray
    public ObjectDescriptorBuffer[] objects;   // Changed from NativeArray
    
    // Metadata
    public string objectName;
    public string creatorInfo;
    public string description;
    
    public bool IsValid => anchors != null && surfaces != null;
    
    public void Dispose()
    {
        // No native arrays to dispose anymore
    }
}
}

/*
USAGE INSTRUCTIONS:

1. Save this file as: Assets/Scripts/PSF/PSFDataStructures.cs

2. These structures exactly match your C++ layout:
   - All structures are 64 bytes (validated with Assert)
   - Pack=1 ensures no padding
   - Arrays use MarshalAs for exact sizing
   - Byte order matches little-endian format

3. Validation:
   PSFStructExtensions.ValidateStructureSizes(); // Call this in editor

4. Creating instances:
   var surface = PSFStructExtensions.InitializeSurface();
   var anchor = PSFStructExtensions.InitializeAnchor();
   var bone = PSFStructExtensions.InitializeBone();

5. The PSFLoader class can now properly deserialize your binary PSF files
   using these exact structure layouts.

6. These structures support:
   - Binary serialization/deserialization
   - Direct memory mapping
   - Cross-platform compatibility
   - Exact C++ interop

The structures are now ready for your Unity PSF pipeline!
*/