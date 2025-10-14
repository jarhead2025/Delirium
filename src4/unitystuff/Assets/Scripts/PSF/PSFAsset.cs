using System.IO;
using UnityEngine;
using Unity.Collections;

namespace ProceduralSurface.Unity
{
    /// <summary>
    /// ScriptableObject representation of a .psf file in Unity
    /// This holds the binary PSF data and provides access to parsed content
    /// Place this file at: Assets/Scripts/PSF/PSFAsset.cs
    /// </summary>
    [CreateAssetMenu(fileName = "NewPSFAsset", menuName = "PSF/PSF Asset", order = 1)]
    public class PSFAsset : ScriptableObject
    {
        [Header("PSF File Data")]
        [SerializeField] private byte[] psfData;
        [SerializeField] private string originalFilePath;
        [SerializeField] private PSFAssetInfo assetInfo;
        
        [Header("Import Settings")]
        [Tooltip("Automatically generate collision meshes")]
        public bool autoGenerateColliders = true;
        
        [Tooltip("Enable skeletal animation")]
        public bool enableAnimation = true;
        
        [Tooltip("Scale factor for geometry")]
        [Range(0.01f, 100f)]
        public float scaleFactor = 1.0f;
        
        [Tooltip("Import quality level")]
        public PSFImportQuality importQuality = PSFImportQuality.High;
        
        [Header("Runtime Properties")]
        [Tooltip("Default material for rendering")]
        public Material defaultMaterial;
        
        [Tooltip("Collision mesh for physics")]
        public Mesh collisionMesh;
        
        [Tooltip("Preview mesh for inspector")]
        public Mesh previewMesh;
        
        // Cached runtime data
        private PSFCollection cachedCollection;
        private bool isLoaded = false;
        private bool hasValidationErrors = false;
        
        // Properties
        public byte[] Data => psfData;
        public PSFAssetInfo Info => assetInfo;
        public bool IsLoaded => isLoaded;
        public bool HasValidationErrors => hasValidationErrors;
        public string OriginalFilePath => originalFilePath;
        
        /// <summary>
        /// Get the parsed PSF collection, loading it if necessary
        /// </summary>
        public PSFCollection GetCollection()
        {
            if (!isLoaded && psfData != null && psfData.Length > 0)
            {
                try
                {
                    cachedCollection = PSFLoader.LoadFromBytes(psfData);
                    isLoaded = true;
                    hasValidationErrors = false;
                }
                catch (System.Exception e)
                {
                    Debug.LogError($"Failed to load PSF collection from {name}: {e.Message}");
                    hasValidationErrors = true;
                    isLoaded = false;
                }
            }
            return cachedCollection;
        }
        
        /// <summary>
        /// Set the binary PSF data (called by importer)
        /// </summary>
        public void SetData(byte[] data, string filePath)
        {
            psfData = data;
            originalFilePath = filePath;
            isLoaded = false;
            hasValidationErrors = false;
            
            // Parse basic file information for inspector display
            ParseAssetInfo();
            
#if UNITY_EDITOR
            UnityEditor.EditorUtility.SetDirty(this);
#endif
        }
        
        /// <summary>
        /// Parse PSF header information without fully loading the collection
        /// </summary>
        private void ParseAssetInfo()
        {
            if (psfData == null || psfData.Length < 64) 
            {
                assetInfo = new PSFAssetInfo { isValid = false };
                return;
            }
            
            try
            {
                using (var stream = new MemoryStream(psfData))
                using (var reader = new BinaryReader(stream))
                {
                    // Read PSF header (64 bytes total)
                    uint magic = reader.ReadUInt32();
                    if (magic != 0x00465350) // "PSF\0"
                    {
                        assetInfo = new PSFAssetInfo { isValid = false };
                        return;
                    }
                    
                    assetInfo.version = reader.ReadUInt32();
                    assetInfo.objectCount = reader.ReadUInt32();
                    assetInfo.boneCount = reader.ReadUInt32();
                    assetInfo.anchorCount = reader.ReadUInt32();
                    assetInfo.surfaceCount = reader.ReadUInt32();
                    assetInfo.metadataSize = reader.ReadUInt32();
                    assetInfo.flags = reader.ReadUInt32();
                    
                    // Read bounding box (6 floats)
                    float minX = reader.ReadSingle();
                    float minY = reader.ReadSingle();
                    float minZ = reader.ReadSingle();
                    float maxX = reader.ReadSingle();
                    float maxY = reader.ReadSingle();
                    float maxZ = reader.ReadSingle();
                    
                    Vector3 min = new Vector3(minX, minY, minZ);
                    Vector3 max = new Vector3(maxX, maxY, maxZ);
                    assetInfo.boundingBox = new Bounds((min + max) * 0.5f, max - min);
                    
                    // Read timestamps (split into low/high 32-bit values)
                    uint timestampLow = reader.ReadUInt32();
                    uint timestampHigh = reader.ReadUInt32();
                    assetInfo.creationTimestamp = ((long)timestampHigh << 32) | timestampLow;
                    
                    // Set additional info
                    assetInfo.fileSize = psfData.Length;
                    assetInfo.isValid = true;
                }
            }
            catch (System.Exception e)
            {
                Debug.LogError($"Failed to parse PSF asset info for {name}: {e.Message}");
                assetInfo = new PSFAssetInfo { isValid = false };
            }
        }
        
        /// <summary>
        /// Validate the PSF data integrity
        /// </summary>
        public bool Validate()
        {
            if (!assetInfo.isValid) return false;
            
            try
            {
                var collection = GetCollection();
                if (collection == null) return false;
                
                // Basic validation checks
                bool valid = true;
                
                // Check that anchor indices in surfaces are valid
                for (int i = 0; i < collection.surfaces.Length; i++)
                {
                    var surface = collection.surfaces[i];
                    for (int j = 0; j < 4; j++)
                    {
                        if (surface.anchor_point_indices[j] >= collection.anchors.Length)
                        {
                            Debug.LogWarning($"Surface {i} has invalid anchor index: {surface.anchor_point_indices[j]}");
                            valid = false;
                        }
                    }
                }
                
                // Check that bone indices in anchors are valid
                for (int i = 0; i < collection.anchors.Length; i++)
                {
                    var anchor = collection.anchors[i];
                    if (anchor.bone_index >= collection.bones.Length && collection.bones.Length > 0)
                    {
                        Debug.LogWarning($"Anchor {i} has invalid bone index: {anchor.bone_index}");
                        valid = false;
                    }
                }
                
                return valid;
            }
            catch (System.Exception e)
            {
                Debug.LogError($"Validation failed for PSF asset {name}: {e.Message}");
                return false;
            }
        }
        
        /// <summary>
        /// Get estimated memory usage
        /// </summary>
        public long GetMemoryUsage()
        {
            long usage = 0;
            
            if (psfData != null) usage += psfData.Length;
            if (collisionMesh != null) usage += GetMeshMemorySize(collisionMesh);
            if (previewMesh != null) usage += GetMeshMemorySize(previewMesh);
            
            return usage;
        }
        
        private long GetMeshMemorySize(Mesh mesh)
        {
            if (mesh == null) return 0;
            
            // Rough estimate: vertices + normals + colors + triangles
            return (mesh.vertexCount * (3 + 3 + 4) * 4) + (mesh.triangles.Length * 4);
        }
        
        /// <summary>
        /// Create a GameObject with PSFObject component configured for this asset
        /// </summary>
        public GameObject CreateGameObject(string name = null)
        {
            if (name == null) name = this.name;
            
            GameObject go = new GameObject(name);
            PSFObject psfObj = go.AddComponent<PSFObject>();
            
            // Configure PSF object with this asset
            psfObj.psfAsset = this;
            psfObj.enableAnimation = enableAnimation && assetInfo.HasAnimation;
            psfObj.enableDynamicColliders = autoGenerateColliders;
            
            // Set default material if available
            if (defaultMaterial != null)
            {
                psfObj.proceduralMaterial = defaultMaterial;
            }
            
            return go;
        }
        
        /// <summary>
        /// Get estimated points per surface for rendering
        /// </summary>
        public int GetPointsPerSurface(PSFImportQuality quality)
        {
            return quality switch
            {
                PSFImportQuality.Low => 25,      // 5x5 grid
                PSFImportQuality.Medium => 100,  // 10x10 grid
                PSFImportQuality.High => 400,    // 20x20 grid
                PSFImportQuality.Ultra => 1600,  // 40x40 grid
                _ => 100
            };
        }
        
        void OnDestroy()
        {
            // Clean up cached collection
            if (cachedCollection != null)
            {
                cachedCollection.Dispose();
                cachedCollection = null;
            }
            isLoaded = false;
        }
        
        void OnValidate()
        {
            // Clamp scale factor to reasonable range
            scaleFactor = Mathf.Clamp(scaleFactor, 0.01f, 100f);
        }
        
#if UNITY_EDITOR
        /// <summary>
        /// Reimport this PSF asset with current settings
        /// </summary>
        [UnityEditor.MenuItem("Assets/Reimport PSF Asset", false, 1000)]
        static void ReimportPSFAsset()
        {
            Object[] selection = UnityEditor.Selection.objects;
            foreach (Object obj in selection)
            {
                if (obj is PSFAsset)
                {
                    string assetPath = UnityEditor.AssetDatabase.GetAssetPath(obj);
                    UnityEditor.AssetDatabase.ImportAsset(assetPath, UnityEditor.ImportAssetOptions.ForceUpdate);
                }
            }
        }
        
        [UnityEditor.MenuItem("Assets/Reimport PSF Asset", true)]
        static bool ReimportPSFAssetValidation()
        {
            return UnityEditor.Selection.activeObject is PSFAsset;
        }
#endif
    }

    /// <summary>
    /// Information parsed from PSF file header
    /// </summary>
    [System.Serializable]
    public struct PSFAssetInfo
    {
        public bool isValid;
        public uint version;
        public uint objectCount;
        public uint boneCount;
        public uint anchorCount;
        public uint surfaceCount;
        public uint metadataSize;
        public uint flags;
        public Bounds boundingBox;
        public long fileSize;
        public long creationTimestamp;
        
        // Computed properties
        public bool HasAnimation => isValid && boneCount > 0;
        public bool HasGeometry => isValid && surfaceCount > 0 && anchorCount > 0;
        public float ComplexityScore => isValid ? (surfaceCount * 0.5f + anchorCount * 0.3f + boneCount * 0.2f) / 1000f : 0f;
        public System.DateTime CreationDate => System.DateTimeOffset.FromUnixTimeSeconds(creationTimestamp).DateTime;
        
        public string GetSummary()
        {
            if (!isValid) return "Invalid PSF file";
            
            return $"PSF v{version}: {surfaceCount} surfaces, {anchorCount} anchors" +
                   (HasAnimation ? $", {boneCount} bones" : "") +
                   $" ({fileSize / 1024f:F1} KB)";
        }
    }
}

/*
USAGE INSTRUCTIONS:

1. Save this file as: Assets/Scripts/PSF/PSFAsset.cs

2. This class represents .psf files as Unity ScriptableObjects:
   - Holds the binary PSF data
   - Parses header information for the inspector
   - Provides lazy loading of full PSF collection
   - Validates data integrity

3. The PSFAssetImporter creates instances of this class automatically

4. You can also create PSFAssets manually:
   PSFAsset asset = ScriptableObject.CreateInstance<PSFAsset>();
   asset.SetData(myPSFBytes, "path/to/file.psf");

5. Access PSF data:
   PSFCollection collection = asset.GetCollection();
   if (collection != null) {
       // Use collection.surfaces, collection.anchors, collection.bones
   }
*/