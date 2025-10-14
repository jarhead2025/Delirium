using System.IO;
using UnityEngine;
using UnityEditor;
using UnityEditor.AssetImporters;

namespace ProceduralSurface.Unity
{
    /// <summary>
    /// Unity Asset Importer for .psf files
    /// This registers the .psf extension with Unity's asset pipeline
    /// Place this file at: Assets/Scripts/PSF/Editor/PSFAssetImporter.cs
    /// </summary>
    [ScriptedImporter(
        version: 1,                    // Increment this to force reimport of all .psf files
        ext: "psf",                   // File extension WITHOUT the dot - this is the key registration
        importQueueOffset: 100,       // Import priority (higher numbers import later)
        AllowCaching = true           // Enable Unity's import caching system
    )]
    public class PSFAssetImporter : ScriptedImporter
    {
        [Header("Import Settings")]
        [Tooltip("Automatically generate collision meshes for physics")]
        public bool autoGenerateColliders = true;
        
        [Tooltip("Enable skeletal animation if bones are present")]
        public bool enableAnimation = true;
        
        [Tooltip("Scale factor applied to all geometry")]
        [Range(0.01f, 100f)]
        public float scaleFactor = 1.0f;
        
        [Tooltip("Quality level for imported geometry")]
        public PSFImportQuality importQuality = PSFImportQuality.High;
        
        [Header("Optimization")]
        [Tooltip("Simplify geometry during import to reduce complexity")]
        public bool simplifyOnImport = false;
        
        [Tooltip("Threshold for geometry simplification (0 = no simplification, 1 = maximum)")]
        [Range(0f, 1f)]
        public float simplificationThreshold = 0.1f;
        
        [Tooltip("Generate multiple LOD levels automatically")]
        public bool generateLODs = true;
        
        [Tooltip("Maximum number of LOD levels to generate")]
        [Range(1, 5)]
        public int maxLODLevels = 3;
        
        [Header("Materials")]
        [Tooltip("Create default materials for each PSF object")]
        public bool createDefaultMaterial = true;
        
        [Tooltip("Shader to use for default materials")]
        public Shader defaultShader;

        /// <summary>
        /// Main import method called by Unity for every .psf file
        /// </summary>
        /// <param name="ctx">Import context provided by Unity</param>
        public override void OnImportAsset(AssetImportContext ctx)
        {
            try
            {
                ImportPSFFile(ctx);
            }
            catch (System.Exception e)
            {
                ctx.LogImportError($"Failed to import PSF file '{ctx.assetPath}': {e.Message}");
                Debug.LogError($"PSF Import Error: {e}");
                
                // Create a placeholder asset so import doesn't completely fail
                CreateErrorAsset(ctx, e.Message);
            }
        }

        private void ImportPSFFile(AssetImportContext ctx)
        {
            // Step 1: Read the PSF file data
            byte[] psfData = File.ReadAllBytes(ctx.assetPath);
            
            if (psfData.Length < 64) // PSF header is exactly 64 bytes
            {
                throw new System.Exception("File too small to be a valid PSF file");
            }

            // Step 2: Validate PSF magic number
            if (!ValidatePSFFile(psfData))
            {
                throw new System.Exception("Invalid PSF file format - magic number mismatch");
            }

            // Step 3: Create the main PSF asset
            PSFAsset psfAsset = CreatePSFAsset(ctx, psfData);
            
            // Step 4: Register main asset with Unity
            ctx.AddObjectToAsset("main", psfAsset);
            ctx.SetMainObject(psfAsset);
            
            // Step 5: Generate additional assets based on import settings
            GenerateSubAssets(ctx, psfAsset);
            
            // Step 6: Log successful import
            LogImportSuccess(ctx, psfAsset);
        }

        private bool ValidatePSFFile(byte[] data)
        {
            if (data.Length < 4) return false;
            
            // Check PSF magic number: "PSF\0" = 0x00465350 in little-endian
            uint magic = System.BitConverter.ToUInt32(data, 0);
            return magic == 0x00465350;
        }

        private PSFAsset CreatePSFAsset(AssetImportContext ctx, byte[] psfData)
        {
            PSFAsset asset = ScriptableObject.CreateInstance<PSFAsset>();
            asset.name = Path.GetFileNameWithoutExtension(ctx.assetPath);
            
            // Set import settings
            asset.autoGenerateColliders = autoGenerateColliders;
            asset.enableAnimation = enableAnimation;
            asset.scaleFactor = scaleFactor;
            asset.importQuality = importQuality; // This should now work without casting
            
            // Set PSF data - this parses the binary format
            asset.SetData(psfData, ctx.assetPath);
            
            return asset;
        }

        private void GenerateSubAssets(AssetImportContext ctx, PSFAsset psfAsset)
        {
            var collection = psfAsset.GetCollection();
            if (collection == null)
            {
                ctx.LogImportWarning("Failed to parse PSF collection - no sub-assets generated");
                return;
            }

            // Generate collision mesh if requested
            if (autoGenerateColliders)
            {
                GenerateCollisionMesh(ctx, psfAsset, collection);
            }

            // Generate preview mesh for inspector
            GeneratePreviewMesh(ctx, psfAsset, collection);

            // Generate default material if requested
            if (createDefaultMaterial)
            {
                GenerateDefaultMaterial(ctx, psfAsset);
            }

            // Generate LOD assets if requested
            if (generateLODs)
            {
                GenerateLODAssets(ctx, psfAsset, collection);
            }
        }

        private void GenerateCollisionMesh(AssetImportContext ctx, PSFAsset psfAsset, PSFCollection collection)
        {
            try
            {
                Mesh collisionMesh = CreateSimpleCollisionMesh(collection);
                collisionMesh.name = psfAsset.name + "_Collision";
                
                ctx.AddObjectToAsset("collision", collisionMesh);
                psfAsset.collisionMesh = collisionMesh; // Assign to asset
                
                Debug.Log($"Generated collision mesh: {collisionMesh.vertexCount} vertices, {collisionMesh.triangles.Length/3} triangles");
            }
            catch (System.Exception e)
            {
                ctx.LogImportWarning($"Failed to generate collision mesh: {e.Message}");
            }
        }

        private void GeneratePreviewMesh(AssetImportContext ctx, PSFAsset psfAsset, PSFCollection collection)
        {
            try
            {
                int previewResolution = GetPreviewResolution(importQuality);
                Mesh previewMesh = CreateSimplePreviewMesh(collection, previewResolution);
                previewMesh.name = psfAsset.name + "_Preview";
                
                ctx.AddObjectToAsset("preview", previewMesh);
                psfAsset.previewMesh = previewMesh; // Assign to asset
                
                Debug.Log($"Generated preview mesh: {previewMesh.vertexCount} vertices at resolution {previewResolution}");
            }
            catch (System.Exception e)
            {
                ctx.LogImportWarning($"Failed to generate preview mesh: {e.Message}");
            }
        }

        private void GenerateDefaultMaterial(AssetImportContext ctx, PSFAsset psfAsset)
        {
            try
            {
                // Find the PSF shader or fall back to Standard
                Shader shader = defaultShader;
                if (shader == null)
                {
                    shader = Shader.Find("PSF/ProceduralPoints");
                    if (shader == null)
                    {
                        shader = Shader.Find("Standard");
                    }
                }

                Material material = new Material(shader);
                material.name = psfAsset.name + "_Material";
                
                // Configure material based on PSF content
                ConfigureMaterial(material, psfAsset);
                
                ctx.AddObjectToAsset("material", material);
                psfAsset.defaultMaterial = material; // Assign to asset
                
                Debug.Log($"Generated default material using shader: {shader.name}");
            }
            catch (System.Exception e)
            {
                ctx.LogImportWarning($"Failed to generate default material: {e.Message}");
            }
        }

        private void ConfigureMaterial(Material material, PSFAsset psfAsset)
        {
            var info = psfAsset.Info;
            
            // Set point size based on complexity
            if (material.HasProperty("_PointSize"))
            {
                float pointSize = Mathf.Lerp(0.005f, 0.05f, 1.0f - info.ComplexityScore);
                material.SetFloat("_PointSize", pointSize);
            }
            
            // Enable animation if bones are present
            if (info.HasAnimation && material.HasProperty("_AnimationSpeed"))
            {
                material.SetFloat("_AnimationSpeed", 1.0f);
                material.EnableKeyword("_ANIMATION_ON");
            }
            
            // Set reasonable material defaults
            if (material.HasProperty("_Metallic"))
                material.SetFloat("_Metallic", 0.0f);
            
            if (material.HasProperty("_Roughness"))
                material.SetFloat("_Roughness", 0.5f);
        }

        private void GenerateLODAssets(AssetImportContext ctx, PSFAsset psfAsset, PSFCollection collection)
        {
            for (int lod = 1; lod <= maxLODLevels; lod++)
            {
                try
                {
                    // For now, create simplified versions of the original
                    // In a full implementation, this would use your C++ simplification algorithm
                    PSFAsset lodAsset = ScriptableObject.CreateInstance<PSFAsset>();
                    lodAsset.name = $"{psfAsset.name}_LOD{lod}";
                    
                    // Create simplified data (placeholder - would use actual simplification)
                    byte[] simplifiedData = CreateSimplifiedPSFData(psfAsset.Data, lod);
                    lodAsset.SetData(simplifiedData, ctx.assetPath);
                    
                    ctx.AddObjectToAsset($"lod{lod}", lodAsset);
                    
                    Debug.Log($"Generated LOD {lod} asset");
                }
                catch (System.Exception e)
                {
                    ctx.LogImportWarning($"Failed to generate LOD {lod}: {e.Message}");
                }
            }
        }

        private Mesh CreateSimpleCollisionMesh(PSFCollection collection)
        {
            var mesh = new Mesh();
            var vertices = new System.Collections.Generic.List<Vector3>();
            var triangles = new System.Collections.Generic.List<int>();
            
            // Generate simple mesh from surfaces for collision
            for (int i = 0; i < collection.surfaces.Length; i++)
            {
                var surface = collection.surfaces[i];
                
                // Get anchor positions
                var positions = new Vector3[4];
                for (int j = 0; j < 4; j++)
                {
                    if (surface.anchor_point_indices[j] < collection.anchors.Length)
                    {
                        var anchor = collection.anchors[surface.anchor_point_indices[j]];
                        positions[j] = DecodeAnchorPosition(anchor.pos_xyz);
                    }
                }
                
                // Add vertices for this surface
                int baseIndex = vertices.Count;
                vertices.AddRange(positions);
                
                // Add triangles (two triangles forming a quad)
                triangles.Add(baseIndex);
                triangles.Add(baseIndex + 1);
                triangles.Add(baseIndex + 2);
                
                triangles.Add(baseIndex);
                triangles.Add(baseIndex + 2);
                triangles.Add(baseIndex + 3);
            }
            
            mesh.vertices = vertices.ToArray();
            mesh.triangles = triangles.ToArray();
            mesh.RecalculateNormals();
            mesh.RecalculateBounds();
            
            return mesh;
        }

        private Mesh CreateSimplePreviewMesh(PSFCollection collection, int resolution)
        {
            var mesh = new Mesh();
            var vertices = new System.Collections.Generic.List<Vector3>();
            var triangles = new System.Collections.Generic.List<int>();
            var normals = new System.Collections.Generic.List<Vector3>();
            var colors = new System.Collections.Generic.List<Color>();
            
            // Generate preview mesh with specified resolution
            for (int i = 0; i < collection.surfaces.Length; i++)
            {
                var surface = collection.surfaces[i];
                
                // Get anchor positions
                var positions = new Vector3[4];
                for (int j = 0; j < 4; j++)
                {
                    if (surface.anchor_point_indices[j] < collection.anchors.Length)
                    {
                        var anchor = collection.anchors[surface.anchor_point_indices[j]];
                        positions[j] = DecodeAnchorPosition(anchor.pos_xyz);
                    }
                }
                
                // Generate grid of vertices
                for (int row = 0; row <= resolution; row++)
                {
                    for (int col = 0; col <= resolution; col++)
                    {
                        float u = (float)col / resolution;
                        float v = (float)row / resolution;
                        
                        // Bilinear interpolation
                        Vector3 top = Vector3.Lerp(positions[0], positions[1], u);
                        Vector3 bottom = Vector3.Lerp(positions[3], positions[2], u);
                        Vector3 pos = Vector3.Lerp(top, bottom, v);
                        
                        vertices.Add(pos);
                        normals.Add(Vector3.up);
                        
                        // Simple color based on position
                        Color color = new Color(u, v, 0.5f, 1.0f);
                        colors.Add(color);
                    }
                }
                
                // Generate triangles for this surface
                int baseIndex = i * (resolution + 1) * (resolution + 1);
                for (int row = 0; row < resolution; row++)
                {
                    for (int col = 0; col < resolution; col++)
                    {
                        int i0 = baseIndex + row * (resolution + 1) + col;
                        int i1 = baseIndex + row * (resolution + 1) + (col + 1);
                        int i2 = baseIndex + (row + 1) * (resolution + 1) + (col + 1);
                        int i3 = baseIndex + (row + 1) * (resolution + 1) + col;
                        
                        // Triangle 1
                        triangles.Add(i0);
                        triangles.Add(i1);
                        triangles.Add(i2);
                        
                        // Triangle 2
                        triangles.Add(i0);
                        triangles.Add(i2);
                        triangles.Add(i3);
                    }
                }
            }
            
            mesh.vertices = vertices.ToArray();
            mesh.triangles = triangles.ToArray();
            mesh.normals = normals.ToArray();
            mesh.colors = colors.ToArray();
            mesh.RecalculateBounds();
            
            return mesh;
        }

        private Vector3 DecodeAnchorPosition(ushort[] pos_xyz)
        {
            if (pos_xyz == null || pos_xyz.Length < 3)
                return Vector3.zero;
                
            return new Vector3(
                (pos_xyz[0] - 32768) / 32768.0f * 1.8f,
                (pos_xyz[1] - 32768) / 32768.0f * 1.8f,
                (pos_xyz[2] - 32768) / 32768.0f * 1.8f
            );
        }

        private byte[] CreateSimplifiedPSFData(byte[] originalData, int lodLevel)
        {
            // Placeholder: In a real implementation, this would call your C++ simplification
            // For now, just return the original data
            // TODO: Integrate with C++ UltraSimplifyPSF function
            return originalData;
        }

        private int GetPreviewResolution(PSFImportQuality quality)
        {
            return quality switch
            {
                PSFImportQuality.Low => 8,
                PSFImportQuality.Medium => 16,
                PSFImportQuality.High => 32,
                PSFImportQuality.Ultra => 64,
                _ => 16
            };
        }

        private void CreateErrorAsset(AssetImportContext ctx, string errorMessage)
        {
            // Create a placeholder asset for failed imports
            PSFAsset errorAsset = ScriptableObject.CreateInstance<PSFAsset>();
            errorAsset.name = Path.GetFileNameWithoutExtension(ctx.assetPath) + "_ERROR";
            
            ctx.AddObjectToAsset("error", errorAsset);
            ctx.SetMainObject(errorAsset);
            
            ctx.LogImportError($"Created error placeholder for failed PSF import: {errorMessage}");
        }

        private void LogImportSuccess(AssetImportContext ctx, PSFAsset psfAsset)
        {
            var info = psfAsset.Info;
            Debug.Log($"Successfully imported PSF: {psfAsset.name}\n" +
                     $"  Surfaces: {info.surfaceCount}\n" +
                     $"  Anchors: {info.anchorCount}\n" +
                     $"  Bones: {info.boneCount}\n" +
                     $"  Animation: {(info.HasAnimation ? "Yes" : "No")}\n" +
                     $"  File Size: {info.fileSize / 1024f:F1} KB\n" +
                     $"  Complexity: {info.ComplexityScore:F2}");
        }
    }
}

/*
USAGE INSTRUCTIONS:

1. Save this file as: Assets/Scripts/PSF/Editor/PSFAssetImporter.cs

2. Unity will automatically recognize .psf files after you:
   - Create the Editor folder
   - Place this script in the Editor folder
   - Let Unity compile the script

3. When you drag a .psf file into Unity's Project window:
   - Unity calls OnImportAsset() automatically
   - Creates a PSFAsset with sub-assets (collision, preview, material, LODs)
   - Shows import settings in the Inspector when you select the .psf file

4. You can reimport all .psf files by:
   - Incrementing the 'version' number in the ScriptedImporter attribute
   - Or right-clicking a .psf file and selecting "Reimport"

5. Import settings are saved per-file and can be adjusted in the Inspector

6. Changes made:
   - Removed duplicate PSFImportQuality enum (uses the one from your existing files)
   - Removed dependency on PSFMeshGenerator (created simple mesh generation internally)
   - Added DecodeAnchorPosition method for mesh generation
   - Added assignment of generated meshes and materials to the PSFAsset
   - Made the importer self-contained and compatible with your existing code
*/