using System.IO;
using UnityEngine;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace ProceduralSurface.Unity
{
    /// <summary>
    /// Binary loader for PSF files - converts raw bytes to Unity-compatible structures
    /// Matches the C++ PSF format exactly for compatibility
    /// Place this file at: Assets/Scripts/PSF/PSFLoader.cs
    /// </summary>
    public static class PSFLoader
    {
        // PSF file format constants (must match C++ version)
        private const uint PSF_MAGIC = 0x00465350; // "PSF\0" in little-endian
        private const uint EXPECTED_VERSION = 1;
        private const int HEADER_SIZE = 64;
        
        /// <summary>
        /// Load PSF collection from byte array
        /// </summary>
        public static PSFCollection LoadFromBytes(byte[] data)
        {
            if (data == null || data.Length < HEADER_SIZE)
            {
                throw new System.Exception("PSF data is null or too small");
            }
            
            using (var stream = new MemoryStream(data))
            using (var reader = new BinaryReader(stream))
            {
                return LoadFromStream(reader);
            }
        }
        
        /// <summary>
        /// Load PSF collection from file path
        /// </summary>
        public static PSFCollection LoadFromFile(string filePath)
        {
            if (!File.Exists(filePath))
            {
                throw new global::System.IO.FileNotFoundException($"PSF file not found: {filePath}");
            }
            
            byte[] data = File.ReadAllBytes(filePath);
            return LoadFromBytes(data);
        }
        
        /// <summary>
        /// Load PSF collection from stream
        /// </summary>
        private static PSFCollection LoadFromStream(BinaryReader reader)
        {
            try
            {
                // Read and validate header
                var header = ReadHeader(reader);
                ValidateHeader(header);
                
                // Skip metadata for now (could be extended later)
                if (header.metadataSize > 0)
                {
                    reader.BaseStream.Seek(header.metadataSize, SeekOrigin.Current);
                }
                
                // Read data pools
                var bones = ReadBonePool(reader, header.bonePoolSize);
                var anchors = ReadAnchorPool(reader, header.anchorPoolSize);
                var surfaces = ReadSurfacePool(reader, header.surfacePoolSize);
                
                // Create collection
                var collection = new PSFCollection
                {
                    header = header,
                    bones = bones,
                    anchors = anchors,
                    surfaces = surfaces
                };
                
                Debug.Log($"PSF loaded successfully: {surfaces.Length} surfaces, {anchors.Length} anchors, {bones.Length} bones");
                
                return collection;
            }
            catch (System.Exception e)
            {
                throw new System.Exception($"Failed to load PSF data: {e.Message}", e);
            }
        }
        
        /// <summary>
        /// Read PSF file header (exactly 64 bytes)
        /// </summary>
        private static PSFHeader ReadHeader(BinaryReader reader)
        {
            var header = PSFStructExtensions.InitializeHeader();
            
            header.magic = reader.ReadUInt32();
            header.version = reader.ReadUInt32();
            header.objectCount = reader.ReadUInt32();
            header.bonePoolSize = reader.ReadUInt32();
            header.anchorPoolSize = reader.ReadUInt32();
            header.surfacePoolSize = reader.ReadUInt32();
            header.metadataSize = reader.ReadUInt32();
            header.flags = reader.ReadUInt32();
            
            // Read bounding box (6 floats)
            for (int i = 0; i < 6; i++)
            {
                header.boundingBox[i] = reader.ReadSingle();
            }
            
            // Read timestamp (split into two 32-bit values)
            header.creationTimestampLow = reader.ReadUInt32();
            header.creationTimestampHigh = reader.ReadUInt32();
            
            return header;
        }
        
        /// <summary>
        /// Validate PSF header
        /// </summary>
        private static void ValidateHeader(PSFHeader header)
        {
            if (header.magic != PSF_MAGIC)
            {
                throw new System.Exception($"Invalid PSF magic number: 0x{header.magic:X8}, expected 0x{PSF_MAGIC:X8}");
            }
            
            if (header.version != EXPECTED_VERSION)
            {
                Debug.LogWarning($"PSF version mismatch: {header.version}, expected {EXPECTED_VERSION}");
            }
            
            if (header.surfacePoolSize == 0 && header.anchorPoolSize == 0)
            {
                throw new System.Exception("PSF file contains no geometry data");
            }
            
            // Sanity checks for reasonable limits
            if (header.anchorPoolSize > 1000000 || header.surfacePoolSize > 1000000 || header.bonePoolSize > 10000)
            {
                throw new System.Exception("PSF file contains unreasonably large data pools - possibly corrupted");
            }
        }
        
        /// <summary>
        /// Read bone pool (each bone is exactly 64 bytes)
        /// </summary>
        private static BoneBuffer[] ReadBonePool(BinaryReader reader, uint count)
        {
            if (count == 0) return new BoneBuffer[0];
            
            var bones = new BoneBuffer[count];
            
            for (int i = 0; i < count; i++)
            {
                var bone = PSFStructExtensions.InitializeBone();
                
                bone.parent_bone_index = reader.ReadUInt32();
                bone.child_bone_index = reader.ReadUInt32();
                
                // Read position (3 x uint16)
                for (int j = 0; j < 3; j++)
                {
                    bone.pos_xyz[j] = reader.ReadUInt16();
                }
                
                bone.orientation = reader.ReadUInt32();
                
                // Read angular velocity (3 x float)
                for (int j = 0; j < 3; j++)
                {
                    bone.ang_velocity[j] = reader.ReadSingle();
                }
                
                // Read angular acceleration (3 x float)
                for (int j = 0; j < 3; j++)
                {
                    bone.ang_accel[j] = reader.ReadSingle();
                }
                
                // Read physical parameters (2 x float)
                for (int j = 0; j < 2; j++)
                {
                    bone.physical_params[j] = reader.ReadSingle();
                }
                
                bone.flags = reader.ReadByte();
                
                // Read reserved bytes (13 bytes to total 64)
                bone.reserved = reader.ReadBytes(13);
                
                bones[i] = bone;
            }
            
            return bones;
        }
        
        /// <summary>
        /// Read anchor pool (each anchor is exactly 64 bytes)
        /// </summary>
        private static AnchorPointBuffer[] ReadAnchorPool(BinaryReader reader, uint count)
        {
            if (count == 0) return new AnchorPointBuffer[0];
            
            var anchors = new AnchorPointBuffer[count];
            
            for (int i = 0; i < count; i++)
            {
                var anchor = PSFStructExtensions.InitializeAnchor();
                
                anchor.bone_index = reader.ReadUInt32();
                
                // Read connected anchors (4 x uint16)
                for (int j = 0; j < 4; j++)
                {
                    anchor.connected_anchors[j] = reader.ReadUInt16();
                }
                
                // Read curve types (4 bytes)
                anchor.curve_types = reader.ReadBytes(4);
                
                // Read curve parameters (16 bytes total - 4 sets of 4 bytes each)
                anchor.curve_params_0 = reader.ReadBytes(4);
                anchor.curve_params_1 = reader.ReadBytes(4);
                anchor.curve_params_2 = reader.ReadBytes(4);
                anchor.curve_params_3 = reader.ReadBytes(4);
                
                // Read position (3 x uint16)
                for (int j = 0; j < 3; j++)
                {
                    anchor.pos_xyz[j] = reader.ReadUInt16();
                }
                
                // Read delta position (3 x int16)
                for (int j = 0; j < 3; j++)
                {
                    anchor.delta_xyz[j] = reader.ReadInt16();
                }
                
                // Read normal (3 bytes)
                anchor.normal = reader.ReadBytes(3);
                
                // Read tangents (2 + 2 bytes)
                anchor.tangent_u = reader.ReadBytes(2);
                anchor.tangent_v = reader.ReadBytes(2);
                
                // Read color (3 bytes)
                anchor.color = reader.ReadBytes(3);
                
                // Read material properties
                anchor.roughness = reader.ReadByte();
                anchor.metallic = reader.ReadByte();
                anchor.anim_weight = reader.ReadByte();
                
                // Read curvature
                anchor.curvature_u = reader.ReadByte();
                anchor.curvature_v = reader.ReadByte();
                anchor.continuity_flags = reader.ReadByte();
                
                // Read reserved bytes (4 bytes to total 64)
                anchor.reserved = reader.ReadBytes(4);
                
                anchors[i] = anchor;
            }
            
            return anchors;
        }
        
        /// <summary>
        /// Read surface pool (each surface is exactly 64 bytes)
        /// </summary>
        private static ProceduralSurfaceBuffer[] ReadSurfacePool(BinaryReader reader, uint count)
        {
            if (count == 0) return new ProceduralSurfaceBuffer[0];
            
            var surfaces = new ProceduralSurfaceBuffer[count];
            
            for (int i = 0; i < count; i++)
            {
                var surface = PSFStructExtensions.InitializeSurface();
                
                // Read anchor point indices (4 x uint16)
                for (int j = 0; j < 4; j++)
                {
                    surface.anchor_point_indices[j] = reader.ReadUInt16();
                }
                
                surface.surface_definition_index = reader.ReadUInt16();
                
                // Read equation parameters (8 x float)
                for (int j = 0; j < 8; j++)
                {
                    surface.eqn_params[j] = reader.ReadSingle();
                }
                
                // Read surface properties
                surface.secondary_surface_type = reader.ReadByte();
                surface.combination_param = reader.ReadByte();
                surface.deformation_strength = reader.ReadByte();
                surface.animation_param = reader.ReadByte();
                
                // Read color equations and parameters
                surface.color_eqn_r = reader.ReadByte();
                surface.color_param_r = reader.ReadByte();
                surface.color_eqn_g = reader.ReadByte();
                surface.color_param_g = reader.ReadByte();
                surface.color_eqn_b = reader.ReadByte();
                surface.color_param_b = reader.ReadByte();
                
                // Read material properties
                surface.roughness = reader.ReadByte();
                surface.metallic = reader.ReadByte();
                surface.emissive_strength = reader.ReadByte();
                surface.specular_tint = reader.ReadByte();
                surface.normal_mode = reader.ReadByte();
                surface.lod_bias = reader.ReadByte();
                surface.surface_modifiers = reader.ReadByte();
                
                // Read reserved bytes (5 bytes to total 64)
                surface.reserved = reader.ReadBytes(5);
                
                surfaces[i] = surface;
            }
            
            return surfaces;
        }
        
        /// <summary>
        /// Save PSF collection to byte array
        /// </summary>
        public static byte[] SaveToBytes(PSFCollection collection)
        {
            using (var stream = new MemoryStream())
            using (var writer = new BinaryWriter(stream))
            {
                WriteToStream(writer, collection);
                return stream.ToArray();
            }
        }
        
        /// <summary>
        /// Save PSF collection to file
        /// </summary>
        public static void SaveToFile(PSFCollection collection, string filePath)
        {
            byte[] data = SaveToBytes(collection);
            File.WriteAllBytes(filePath, data);
        }
        
        /// <summary>
        /// Write PSF collection to stream
        /// </summary>
        private static void WriteToStream(BinaryWriter writer, PSFCollection collection)
        {
            // Write header
            WriteHeader(writer, collection.header);
            
            // Skip metadata for now (write zero-length metadata)
            // Could be extended to write object name, creator info, etc.
            
            // Write data pools
            WriteBonePool(writer, collection.bones);
            WriteAnchorPool(writer, collection.anchors);
            WriteSurfacePool(writer, collection.surfaces);
        }
        
        /// <summary>
        /// Write PSF header
        /// </summary>
        private static void WriteHeader(BinaryWriter writer, PSFHeader header)
        {
            writer.Write(header.magic);
            writer.Write(header.version);
            writer.Write(header.objectCount);
            writer.Write(header.bonePoolSize);
            writer.Write(header.anchorPoolSize);
            writer.Write(header.surfacePoolSize);
            writer.Write(header.metadataSize);
            writer.Write(header.flags);
            
            // Write bounding box
            for (int i = 0; i < 6; i++)
            {
                writer.Write(header.boundingBox[i]);
            }
            
            // Write timestamp
            writer.Write(header.creationTimestampLow);
            writer.Write(header.creationTimestampHigh);
        }
        
        /// <summary>
        /// Write bone pool
        /// </summary>
        private static void WriteBonePool(BinaryWriter writer, BoneBuffer[] bones)
        {
            for (int i = 0; i < bones.Length; i++)
            {
                var bone = bones[i];
                
                writer.Write(bone.parent_bone_index);
                writer.Write(bone.child_bone_index);
                
                for (int j = 0; j < 3; j++)
                {
                    writer.Write(bone.pos_xyz[j]);
                }
                
                writer.Write(bone.orientation);
                
                for (int j = 0; j < 3; j++)
                {
                    writer.Write(bone.ang_velocity[j]);
                }
                
                for (int j = 0; j < 3; j++)
                {
                    writer.Write(bone.ang_accel[j]);
                }
                
                for (int j = 0; j < 2; j++)
                {
                    writer.Write(bone.physical_params[j]);
                }
                
                writer.Write(bone.flags);
                writer.Write(bone.reserved);
            }
        }
        
        /// <summary>
        /// Write anchor pool
        /// </summary>
        private static void WriteAnchorPool(BinaryWriter writer, AnchorPointBuffer[] anchors)
        {
            for (int i = 0; i < anchors.Length; i++)
            {
                var anchor = anchors[i];
                
                writer.Write(anchor.bone_index);
                
                for (int j = 0; j < 4; j++)
                {
                    writer.Write(anchor.connected_anchors[j]);
                }
                
                writer.Write(anchor.curve_types);
                writer.Write(anchor.curve_params_0);
                writer.Write(anchor.curve_params_1);
                writer.Write(anchor.curve_params_2);
                writer.Write(anchor.curve_params_3);
                
                for (int j = 0; j < 3; j++)
                {
                    writer.Write(anchor.pos_xyz[j]);
                }
                
                for (int j = 0; j < 3; j++)
                {
                    writer.Write(anchor.delta_xyz[j]);
                }
                
                writer.Write(anchor.normal);
                writer.Write(anchor.tangent_u);
                writer.Write(anchor.tangent_v);
                writer.Write(anchor.color);
                writer.Write(anchor.roughness);
                writer.Write(anchor.metallic);
                writer.Write(anchor.anim_weight);
                writer.Write(anchor.curvature_u);
                writer.Write(anchor.curvature_v);
                writer.Write(anchor.continuity_flags);
                writer.Write(anchor.reserved);
            }
        }
        
        /// <summary>
        /// Write surface pool
        /// </summary>
        private static void WriteSurfacePool(BinaryWriter writer, ProceduralSurfaceBuffer[] surfaces)
        {
            for (int i = 0; i < surfaces.Length; i++)
            {
                var surface = surfaces[i];
                
                for (int j = 0; j < 4; j++)
                {
                    writer.Write(surface.anchor_point_indices[j]);
                }
                
                writer.Write(surface.surface_definition_index);
                
                for (int j = 0; j < 8; j++)
                {
                    writer.Write(surface.eqn_params[j]);
                }
                
                writer.Write(surface.secondary_surface_type);
                writer.Write(surface.combination_param);
                writer.Write(surface.deformation_strength);
                writer.Write(surface.animation_param);
                writer.Write(surface.color_eqn_r);
                writer.Write(surface.color_param_r);
                writer.Write(surface.color_eqn_g);
                writer.Write(surface.color_param_g);
                writer.Write(surface.color_eqn_b);
                writer.Write(surface.color_param_b);
                writer.Write(surface.roughness);
                writer.Write(surface.metallic);
                writer.Write(surface.emissive_strength);
                writer.Write(surface.specular_tint);
                writer.Write(surface.normal_mode);
                writer.Write(surface.lod_bias);
                writer.Write(surface.surface_modifiers);
                writer.Write(surface.reserved);
            }
        }
        
        /// <summary>
        /// Validate PSF collection integrity
        /// </summary>
        public static bool ValidateCollection(PSFCollection collection, out string errorMessage)
        {
            errorMessage = "";
            
            try
            {
                // Check header validity
                if (collection.header.magic != PSF_MAGIC)
                {
                    errorMessage = "Invalid magic number";
                    return false;
                }
                
                // Check data consistency
                if (collection.header.anchorPoolSize != collection.anchors.Length)
                {
                    errorMessage = $"Anchor count mismatch: header={collection.header.anchorPoolSize}, actual={collection.anchors.Length}";
                    return false;
                }
                
                if (collection.header.surfacePoolSize != collection.surfaces.Length)
                {
                    errorMessage = $"Surface count mismatch: header={collection.header.surfacePoolSize}, actual={collection.surfaces.Length}";
                    return false;
                }
                
                if (collection.header.bonePoolSize != collection.bones.Length)
                {
                    errorMessage = $"Bone count mismatch: header={collection.header.bonePoolSize}, actual={collection.bones.Length}";
                    return false;
                }
                
                // Validate surface anchor indices
                for (int i = 0; i < collection.surfaces.Length; i++)
                {
                    var surface = collection.surfaces[i];
                    for (int j = 0; j < 4; j++)
                    {
                        if (surface.anchor_point_indices[j] >= collection.anchors.Length)
                        {
                            errorMessage = $"Surface {i} has invalid anchor index {surface.anchor_point_indices[j]} (max: {collection.anchors.Length - 1})";
                            return false;
                        }
                    }
                }
                
                // Validate anchor bone indices
                if (collection.bones.Length > 0)
                {
                    for (int i = 0; i < collection.anchors.Length; i++)
                    {
                        var anchor = collection.anchors[i];
                        if (anchor.bone_index >= collection.bones.Length)
                        {
                            errorMessage = $"Anchor {i} has invalid bone index {anchor.bone_index} (max: {collection.bones.Length - 1})";
                            return false;
                        }
                    }
                }
                
                return true;
            }
            catch (System.Exception e)
            {
                errorMessage = $"Validation exception: {e.Message}";
                return false;
            }
        }
        
        /// <summary>
        /// Create a simple test PSF collection
        /// </summary>
        public static PSFCollection CreateTestCollection()
        {
            var header = PSFStructExtensions.InitializeHeader();
            header.objectCount = 1;
            header.anchorPoolSize = 4;
            header.surfacePoolSize = 1;
            header.bonePoolSize = 0;
            header.metadataSize = 0;
            header.flags = 0;
            header.SetBounds(new Bounds(Vector3.zero, Vector3.one * 2));
            
            // Create 4 anchors for a simple quad
            var anchors = new AnchorPointBuffer[4];
            Vector3[] positions = {
                new Vector3(-1, -1, 0),
                new Vector3(1, -1, 0),
                new Vector3(1, 1, 0),
                new Vector3(-1, 1, 0)
            };
            
            for (int i = 0; i < 4; i++)
            {
                var anchor = PSFStructExtensions.InitializeAnchor();
                
                // Encode position to PSF format
                anchor.pos_xyz[0] = (ushort)((positions[i].x / 1.8f + 1.0f) * 32768);
                anchor.pos_xyz[1] = (ushort)((positions[i].y / 1.8f + 1.0f) * 32768);
                anchor.pos_xyz[2] = (ushort)((positions[i].z / 1.8f + 1.0f) * 32768);
                
                // Set normal pointing up
                anchor.normal = new byte[] { 127, 127, 255 };
                
                // Set different colors per corner
                switch (i)
                {
                    case 0: anchor.color = new byte[] { 255, 0, 0 }; break;
                    case 1: anchor.color = new byte[] { 0, 255, 0 }; break;
                    case 2: anchor.color = new byte[] { 0, 0, 255 }; break;
                    case 3: anchor.color = new byte[] { 255, 255, 0 }; break;
                }
                
                anchor.roughness = 128;
                anchor.metallic = 0;
                anchor.bone_index = 0;
                
                anchors[i] = anchor;
            }
            
            // Create surface
            var surfaces = new ProceduralSurfaceBuffer[1];
            var surface = PSFStructExtensions.InitializeSurface();
            
            surface.anchor_point_indices[0] = 0;
            surface.anchor_point_indices[1] = 1;
            surface.anchor_point_indices[2] = 2;
            surface.anchor_point_indices[3] = 3;
            surface.surface_definition_index = 0;
            surface.eqn_params[2] = 1.0f; // Normal Z component
            
            surface.secondary_surface_type = 0; // Plane
            surface.color_eqn_r = 1; // Linear U
            surface.color_eqn_g = 2; // Linear V
            surface.color_eqn_b = 0; // Constant
            surface.color_param_r = 200;
            surface.color_param_g = 180;
            surface.color_param_b = 160;
            surface.roughness = 128;
            surface.metallic = 0;
            
            surfaces[0] = surface;
            
            // Create empty bone array
            var bones = new BoneBuffer[0];
            
            return new PSFCollection
            {
                header = header,
                anchors = anchors,
                surfaces = surfaces,
                bones = bones
            };
        }
        
        /// <summary>
        /// Get memory footprint of a PSF collection
        /// </summary>
        public static long GetMemoryFootprint(PSFCollection collection)
        {
            long size = 0;
            
            size += HEADER_SIZE; // Header
            size += collection.bones.Length * 64; // Bones (64 bytes each)
            size += collection.anchors.Length * 64; // Anchors (64 bytes each)
            size += collection.surfaces.Length * 64; // Surfaces (64 bytes each)
            
            return size;
        }
        
        /// <summary>
        /// Create a PSF collection summary for debugging
        /// </summary>
        public static string GetCollectionSummary(PSFCollection collection)
        {
            var summary = new System.Text.StringBuilder();
            
            summary.AppendLine($"PSF Collection Summary:");
            summary.AppendLine($"  Magic: 0x{collection.header.magic:X8}");
            summary.AppendLine($"  Version: {collection.header.version}");
            summary.AppendLine($"  Anchors: {collection.anchors.Length}");
            summary.AppendLine($"  Surfaces: {collection.surfaces.Length}");
            summary.AppendLine($"  Bones: {collection.bones.Length}");
            summary.AppendLine($"  Bounds: {collection.header.GetBounds()}");
            summary.AppendLine($"  Memory: {GetMemoryFootprint(collection)} bytes");
            summary.AppendLine($"  Created: {System.DateTimeOffset.FromUnixTimeSeconds(collection.header.CreationTimestamp)}");
            
            // Analyze surface types
            var surfaceTypes = new Dictionary<byte, int>();
            for (int i = 0; i < collection.surfaces.Length; i++)
            {
                byte type = collection.surfaces[i].secondary_surface_type;
                if (!surfaceTypes.ContainsKey(type))
                    surfaceTypes[type] = 0;
                surfaceTypes[type]++;
            }
            
            summary.AppendLine($"  Surface Types:");
            foreach (var kvp in surfaceTypes)
            {
                summary.AppendLine($"    Type {kvp.Key}: {kvp.Value} surfaces");
            }
            
            return summary.ToString();
        }
    }
}

/*
USAGE INSTRUCTIONS:

1. Save this file as: Assets/Scripts/PSF/PSFLoader.cs

2. Loading PSF files:
   // From byte array (used by PSFAsset)
   PSFCollection collection = PSFLoader.LoadFromBytes(psfAsset.Data);
   
   // From file path
   PSFCollection collection = PSFLoader.LoadFromFile("path/to/file.psf");

3. Validation:
   if (PSFLoader.ValidateCollection(collection, out string error))
   {
       Debug.Log("PSF collection is valid");
   }
   else
   {
       Debug.LogError($"PSF validation failed: {error}");
   }

4. Saving (if needed):
   byte[] data = PSFLoader.SaveToBytes(collection);
   PSFLoader.SaveToFile(collection, "output.psf");

5. Creating test data:
   PSFCollection testCollection = PSFLoader.CreateTestCollection();

6. Memory management:
   collection.Dispose(); // Important: dispose when done to free native memory

7. Debugging:
   string summary = PSFLoader.GetCollectionSummary(collection);
   Debug.Log(summary);

CHANGES MADE:
- Removed Unity.Collections dependency
- Changed all NativeArray<T> to T[] (regular C# arrays)
- Updated PSFCollection to use regular arrays instead of NativeArrays
- Fixed GetValueOrDefault compatibility issue in GetCollectionSummary
- Removed Allocator.Persistent calls
- All read/write methods now work with regular arrays
- This should resolve the "unmanaged types" error completely
*/