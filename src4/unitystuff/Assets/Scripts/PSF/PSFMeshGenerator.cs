using System.Collections.Generic;
using UnityEngine;
using Unity.Collections;

namespace ProceduralSurface.Unity
{
    /// <summary>
    /// Main PSF object component with full GPU acceleration
    /// All calculations are performed on GPU using compute shaders
    /// Place this file at: Assets/Scripts/PSF/PSFObject.cs
    /// </summary>
    [System.Serializable]
    public class PSFObject : MonoBehaviour
    {
        [Header("PSF Asset")]
        [Tooltip("PSF asset containing the procedural surface data")]
        public PSFAsset psfAsset;
        
        [Header("GPU Rendering")]
        [Tooltip("Material for rendering procedural points (must support GPU buffers)")]
        public Material proceduralMaterial;
        
        [Tooltip("Enable GPU compute shader acceleration")]
        public bool useGPUCompute = true;
        
        [Tooltip("Points per surface for rendering quality")]
        [Range(25, 10000)]
        public int pointsPerSurface = 400;
        
        [Tooltip("Point size for rendering")]
        [Range(0.001f, 0.1f)]
        public float pointSize = 0.01f;
        
        [Header("Animation")]
        [Tooltip("Enable procedural animation")]
        public bool enableAnimation = true;
        
        [Tooltip("Animation playback speed")]
        [Range(0f, 5f)]
        public float animationSpeed = 1f;
        
        [Tooltip("Current animation time (0-1)")]
        [Range(0f, 1f)]
        public float animationTime = 0f;
        
        [Header("Level of Detail")]
        [Tooltip("Enable automatic level of detail")]
        public bool enableLOD = true;
        
        [Tooltip("Distance for LOD level 0")]
        [Range(1f, 100f)]
        public float lodDistance = 10f;
        
        [Tooltip("Maximum LOD level")]
        [Range(1, 5)]
        public int maxLODLevel = 3;
        
        [Header("Physics")]
        [Tooltip("Enable dynamic collision generation")]
        public bool enableDynamicColliders = false;
        
        [Tooltip("Update colliders every N frames")]
        [Range(1, 60)]
        public int colliderUpdateFrequency = 30;
        
        // GPU Compute Resources
        private PSFComputeEngine computeEngine;
        private ComputeBuffer surfaceBuffer;
        private ComputeBuffer anchorBuffer;
        private ComputeBuffer boneBuffer;
        private ComputeBuffer pointBuffer;
        private ComputeBuffer colorBuffer;
        private ComputeBuffer normalBuffer;
        
        // Cached data
        private PSFCollection psfCollection;
        private Transform cachedTransform;
        private Bounds renderBounds;
        private Matrix4x4[] boneMatrices;
        
        // Animation state
        private bool isPlaying = false;
        private float internalAnimationTime = 0f;
        
        // LOD state
        private int currentLOD = 0;
        private int currentPointCount = 0;
        
        // Performance tracking
        private int frameCount = 0;
        private bool isDirty = true;
        private bool isVisible = true;
        private bool isGPUInitialized = false;
        
        // Events
        public System.Action<PSFObject> OnAnimationComplete;
        public System.Action<PSFObject, int> OnLODChanged;
        public System.Action<PSFObject> OnDataLoaded;
        
        // Properties
        public bool IsLoaded => psfCollection != null && psfCollection.IsValid;
        public bool IsAnimated => IsLoaded && psfCollection.bones.Length > 0;
        public bool IsVisible => isVisible;
        public bool IsPlaying => isPlaying;
        public float AnimationTime => internalAnimationTime;
        public int CurrentLOD => currentLOD;
        public int CurrentPointCount => currentPointCount;
        public Bounds RenderBounds => renderBounds;
        
        void Start()
        {
            Initialize();
        }
        
        void Update()
        {
            if (!IsLoaded) return;
            
            UpdateAnimation();
            UpdateLOD();
            UpdateGPURendering();
            UpdatePhysics();
            
            frameCount++;
        }
        
        void OnDestroy()
        {
            CleanupGPUResources();
        }
        
        void OnBecameVisible()
        {
            isVisible = true;
        }
        
        void OnBecameInvisible()
        {
            isVisible = false;
        }
        
        /// <summary>
        /// Initialize the PSF object
        /// </summary>
        private void Initialize()
        {
            cachedTransform = transform;
            
            if (psfAsset == null)
            {
                Debug.LogWarning($"PSF Object '{name}' has no PSF asset assigned");
                return;
            }
            
            LoadPSFData();
            InitializeGPUPipeline();
            InitializePhysics();
            
            if (IsAnimated && enableAnimation)
            {
                PlayAnimation();
            }
            
            // Register with pipeline manager
            if (PSFPipelineManager.Instance != null)
            {
                PSFPipelineManager.Instance.RegisterObject(this);
            }
            
            OnDataLoaded?.Invoke(this);
        }
        
        /// <summary>
        /// Load PSF data from asset
        /// </summary>
        private void LoadPSFData()
        {
            try
            {
                psfCollection = psfAsset.GetCollection();
                
                if (psfCollection == null || !psfCollection.IsValid)
                {
                    Debug.LogError($"Failed to load PSF collection from asset '{psfAsset.name}'");
                    return;
                }
                
                // Calculate render bounds
                renderBounds = psfAsset.Info.boundingBox;
                
                Debug.Log($"PSF Object '{name}' loaded: {psfCollection.surfaces.Length} surfaces, " +
                         $"{psfCollection.anchors.Length} anchors, {psfCollection.bones.Length} bones");
            }
            catch (System.Exception e)
            {
                Debug.LogError($"Failed to load PSF data for '{name}': {e.Message}");
            }
        }
        
        /// <summary>
        /// Initialize GPU compute pipeline
        /// </summary>
        private void InitializeGPUPipeline()
        {
            if (!useGPUCompute || !IsLoaded)
            {
                return;
            }
            
            try
            {
                // Get or create compute engine
                computeEngine = FindObjectOfType<PSFComputeEngine>();
                if (computeEngine == null)
                {
                    GameObject computeGO = new GameObject("PSF Compute Engine");
                    computeEngine = computeGO.AddComponent<PSFComputeEngine>();
                }
                
                // Calculate point count based on LOD
                currentPointCount = CalculatePointCount(currentLOD);
                
                // Create GPU buffers
                CreateGPUBuffers();
                
                // Upload PSF data to GPU
                UploadPSFDataToGPU();
                
                // Subscribe to GPU events
                computeEngine.OnMeshGenerated += OnGPUMeshGenerated;
                
                isGPUInitialized = true;
                
                Debug.Log($"GPU pipeline initialized for '{name}': {currentPointCount} points");
            }
            catch (System.Exception e)
            {
                Debug.LogError($"Failed to initialize GPU pipeline for '{name}': {e.Message}");
                useGPUCompute = false;
            }
        }
        
        /// <summary>
        /// Create GPU compute buffers
        /// </summary>
        private void CreateGPUBuffers()
        {
            // Surface data buffer
            if (psfCollection.surfaces.Length > 0)
            {
                surfaceBuffer = new ComputeBuffer(psfCollection.surfaces.Length, 64); // 64 bytes per surface
                surfaceBuffer.SetData(psfCollection.surfaces);
            }
            
            // Anchor data buffer
            if (psfCollection.anchors.Length > 0)
            {
                anchorBuffer = new ComputeBuffer(psfCollection.anchors.Length, 64); // 64 bytes per anchor
                anchorBuffer.SetData(psfCollection.anchors);
            }
            
            // Bone data buffer
            if (psfCollection.bones.Length > 0)
            {
                boneBuffer = new ComputeBuffer(psfCollection.bones.Length, 64); // 64 bytes per bone
                boneBuffer.SetData(psfCollection.bones);
                boneMatrices = new Matrix4x4[psfCollection.bones.Length];
            }
            
            // Output buffers for generated points
            pointBuffer = new ComputeBuffer(currentPointCount, 12);  // float3 positions
            colorBuffer = new ComputeBuffer(currentPointCount, 16);  // float4 colors
            normalBuffer = new ComputeBuffer(currentPointCount, 12); // float3 normals
        }
        
        /// <summary>
        /// Upload PSF data to GPU compute shaders
        /// </summary>
        private void UploadPSFDataToGPU()
        {
            if (computeEngine == null) return;
            
            // Initialize compute engine with our data
            computeEngine.InitializePSFData(psfCollection, pointsPerSurface);
            
            // Bind our specific buffers
            if (surfaceBuffer != null)
                computeEngine.SetBuffer("_SurfaceBuffer", surfaceBuffer);
            
            if (anchorBuffer != null)
                computeEngine.SetBuffer("_AnchorBuffer", anchorBuffer);
            
            if (boneBuffer != null)
                computeEngine.SetBuffer("_BoneBuffer", boneBuffer);
            
            if (pointBuffer != null)
                computeEngine.SetBuffer("_PointBuffer", pointBuffer);
            
            if (colorBuffer != null)
                computeEngine.SetBuffer("_ColorBuffer", colorBuffer);
            
            if (normalBuffer != null)
                computeEngine.SetBuffer("_NormalBuffer", normalBuffer);
        }
        
        /// <summary>
        /// Update animation on GPU
        /// </summary>
        private void UpdateAnimation()
        {
            if (!enableAnimation || !IsAnimated) return;
            
            if (isPlaying)
            {
                internalAnimationTime += Time.deltaTime * animationSpeed;
                if (internalAnimationTime >= 1.0f)
                {
                    internalAnimationTime = 0f;
                    OnAnimationComplete?.Invoke(this);
                }
                
                animationTime = internalAnimationTime;
                isDirty = true;
            }
        }
        
        /// <summary>
        /// Update level of detail
        /// </summary>
        private void UpdateLOD()
        {
            if (!enableLOD) return;
            
            Camera mainCamera = Camera.main;
            if (mainCamera == null) return;
            
            float distance = Vector3.Distance(cachedTransform.position, mainCamera.transform.position);
            int newLOD = Mathf.Clamp(Mathf.FloorToInt(distance / lodDistance), 0, maxLODLevel);
            
            if (newLOD != currentLOD)
            {
                SetLOD(newLOD);
            }
        }
        
        /// <summary>
        /// Update GPU rendering
        /// </summary>
        private void UpdateGPURendering()
        {
            if (!useGPUCompute || !isGPUInitialized || !isVisible) return;
            
            // Update GPU compute if dirty or animating
            if (isDirty || (enableAnimation && isPlaying))
            {
                UpdateGPUCompute();
                isDirty = false;
            }
            
            // Render using GPU buffers
            RenderGPUPoints();
        }
        
        /// <summary>
        /// Update GPU compute shaders
        /// </summary>
        private void UpdateGPUCompute()
        {
            if (computeEngine == null) return;
            
            // Update bone animation on GPU
            if (IsAnimated && enableAnimation)
            {
                computeEngine.UpdateBoneAnimation(animationTime, animationSpeed);
            }
            
            // Update surface evaluation on GPU
          computeEngine.UpdatePSFCompute(
    animationTime,
    currentLOD,
    pointsPerSurface >> currentLOD,
    cachedTransform.localToWorldMatrix
);

        }
        
        /// <summary>
        /// Render points using GPU buffers
        /// </summary>
        private void RenderGPUPoints()
        {
            if (proceduralMaterial == null || pointBuffer == null) return;
            
            // Set material properties
            proceduralMaterial.SetBuffer("_PointBuffer", pointBuffer);
            proceduralMaterial.SetBuffer("_ColorBuffer", colorBuffer);
            proceduralMaterial.SetBuffer("_NormalBuffer", normalBuffer);
            proceduralMaterial.SetInt("_PointCount", currentPointCount);
            proceduralMaterial.SetFloat("_PointSize", pointSize);
            proceduralMaterial.SetMatrix("_ObjectToWorld", cachedTransform.localToWorldMatrix);
            proceduralMaterial.SetFloat("_AnimationTime", animationTime);
            proceduralMaterial.SetInt("_LODLevel", currentLOD);
            
            // Calculate world bounds
            Bounds worldBounds = new Bounds(
                cachedTransform.TransformPoint(renderBounds.center),
                cachedTransform.TransformVector(renderBounds.size)
            );
            
            // Render procedural geometry
            Graphics.DrawProcedural(
                proceduralMaterial,
                worldBounds,
                MeshTopology.Points,
                currentPointCount,
                1,
                null,
                null,
                UnityEngine.Rendering.ShadowCastingMode.On,
                true,
                gameObject.layer
            );
        }
        
        /// <summary>
        /// Handle GPU points generation callback
        /// </summary>
   private void OnGPUMeshGenerated(ComputeBuffer vertices, ComputeBuffer normals, ComputeBuffer colors, ComputeBuffer uvs, ComputeBuffer indices)
{
    // Update our local buffers if they're different
    if (pointBuffer != vertices)  // Change 'points' to 'vertices'
    {
        pointBuffer?.Release();
        pointBuffer = vertices;    // Change 'points' to 'vertices'
    }
    
    if (colorBuffer != colors)
    {
        colorBuffer?.Release();
        colorBuffer = colors;
    }
    
    if (normalBuffer != normals)
    {
        normalBuffer?.Release();
        normalBuffer = normals;
    }
    
    // Update point count
    currentPointCount = vertices.count;  // Change 'points' to 'vertices'
}
        /// <summary>
        /// Initialize physics components
        /// </summary>
        private void InitializePhysics()
        {
            if (!enableDynamicColliders || !IsLoaded) return;
            
            // Use the collision mesh from the asset if available
            if (psfAsset.collisionMesh != null)
            {
                var collider = GetComponent<MeshCollider>();
                if (collider == null)
                {
                    collider = gameObject.AddComponent<MeshCollider>();
                }
                
                collider.sharedMesh = psfAsset.collisionMesh;
                collider.convex = true; // For performance
            }
        }
        
        /// <summary>
        /// Update physics (collision mesh generation)
        /// </summary>
        private void UpdatePhysics()
        {
            if (!enableDynamicColliders) return;
            
            // Update collision mesh every N frames
            if (frameCount % colliderUpdateFrequency == 0)
            {
                // This could be extended to generate collision mesh from GPU data
                // For now, we rely on the static collision mesh
            }
        }
        
        /// <summary>
        /// Set LOD level
        /// </summary>
        public void SetLOD(int lodLevel)
        {
            int newLOD = Mathf.Clamp(lodLevel, 0, maxLODLevel);
            
            if (newLOD != currentLOD)
            {
                currentLOD = newLOD;
                
                // Recalculate point count for new LOD
                int newPointCount = CalculatePointCount(currentLOD);
                
                if (newPointCount != currentPointCount)
                {
                    // Recreate point buffer with new size
                    if (useGPUCompute && isGPUInitialized)
                    {
                        pointBuffer?.Release();
                        colorBuffer?.Release();
                        normalBuffer?.Release();
                        
                        currentPointCount = newPointCount;
                        
                        pointBuffer = new ComputeBuffer(currentPointCount, 12);
                        colorBuffer = new ComputeBuffer(currentPointCount, 16);
                        normalBuffer = new ComputeBuffer(currentPointCount, 12);
                        
                        // Re-upload to compute engine
                        if (computeEngine != null)
                        {
                            computeEngine.SetBuffer("_PointBuffer", pointBuffer);
                            computeEngine.SetBuffer("_ColorBuffer", colorBuffer);
                            computeEngine.SetBuffer("_NormalBuffer", normalBuffer);
                        }
                    }
                }
                
                isDirty = true;
                OnLODChanged?.Invoke(this, currentLOD);
                
                Debug.Log($"PSF Object '{name}' LOD changed to {currentLOD} ({currentPointCount} points)");
            }
        }
        
        /// <summary>
        /// Calculate point count for LOD level
        /// </summary>
        private int CalculatePointCount(int lodLevel)
        {
            if (!IsLoaded) return 0;
            
            int basePoints = psfCollection.surfaces.Length * pointsPerSurface;
            
            // Reduce points exponentially with LOD level
            for (int i = 0; i < lodLevel; i++)
            {
                basePoints = Mathf.Max(basePoints / 2, psfCollection.surfaces.Length * 4); // Minimum 4 points per surface
            }
            
            return basePoints;
        }
        
        /// <summary>
        /// Play animation
        /// </summary>
        public void PlayAnimation()
        {
            if (!IsAnimated)
            {
                Debug.LogWarning($"PSF Object '{name}' has no animation data");
                return;
            }
            
            isPlaying = true;
        }
        
        /// <summary>
        /// Pause animation
        /// </summary>
        public void PauseAnimation()
        {
            isPlaying = false;
        }
        
        /// <summary>
        /// Stop animation and reset to beginning
        /// </summary>
        public void StopAnimation()
        {
            isPlaying = false;
            internalAnimationTime = 0f;
            animationTime = 0f;
            isDirty = true;
        }
        
        /// <summary>
        /// Set animation time manually
        /// </summary>
        public void SetAnimationTime(float time)
        {
            internalAnimationTime = Mathf.Clamp01(time);
            animationTime = internalAnimationTime;
            isDirty = true;
        }
        
        /// <summary>
        /// Set surface parameter on GPU
        /// </summary>
        public void SetSurfaceParameter(int surfaceIndex, int paramIndex, float value)
        {
            if (!IsLoaded || surfaceIndex >= psfCollection.surfaces.Length) return;
            
            if (useGPUCompute && computeEngine != null)
            {
                // Update parameter on GPU
                computeEngine.UpdateSurfaceParameter(surfaceIndex, paramIndex, value);
            }
            else
            {
                // Update in CPU data
                psfCollection.surfaces[surfaceIndex].eqn_params[paramIndex] = value;
                
                // Re-upload to GPU if needed
                if (useGPUCompute && surfaceBuffer != null)
                {
                    surfaceBuffer.SetData(psfCollection.surfaces);
                }
            }
            
            isDirty = true;
        }
        
        /// <summary>
        /// Get estimated memory usage
        /// </summary>
        public long GetMemoryUsage()
        {
            long usage = 0;
            
            if (surfaceBuffer != null)
                usage += surfaceBuffer.count * surfaceBuffer.stride;
            
            if (anchorBuffer != null)
                usage += anchorBuffer.count * anchorBuffer.stride;
            
            if (boneBuffer != null)
                usage += boneBuffer.count * boneBuffer.stride;
            
            if (pointBuffer != null)
                usage += pointBuffer.count * pointBuffer.stride;
            
            if (colorBuffer != null)
                usage += colorBuffer.count * colorBuffer.stride;
            
            if (normalBuffer != null)
                usage += normalBuffer.count * normalBuffer.stride;
            
            return usage;
        }
        
        /// <summary>
        /// Force refresh of all GPU data
        /// </summary>
        public void RefreshGPUData()
        {
            if (!useGPUCompute || !isGPUInitialized) return;
            
            CleanupGPUResources();
            InitializeGPUPipeline();
        }
        
        /// <summary>
        /// Cleanup GPU resources
        /// </summary>
        private void CleanupGPUResources()
        {
            // Unsubscribe from events
            if (computeEngine != null)
            {
                computeEngine.OnMeshGenerated -= OnGPUMeshGenerated;
            }
            
            // Release buffers
            surfaceBuffer?.Release();
            anchorBuffer?.Release();
            boneBuffer?.Release();
            pointBuffer?.Release();
            colorBuffer?.Release();
            normalBuffer?.Release();
            
            // Clear references
            surfaceBuffer = null;
            anchorBuffer = null;
            boneBuffer = null;
            pointBuffer = null;
            colorBuffer = null;
            normalBuffer = null;
            
            isGPUInitialized = false;
        }
        
        void OnDrawGizmosSelected()
        {
            // Draw render bounds
            Gizmos.color = Color.cyan;
            Gizmos.matrix = transform.localToWorldMatrix;
            Gizmos.DrawWireCube(renderBounds.center, renderBounds.size);
            
            // Draw LOD level indicator
            Gizmos.color = Color.yellow;
            Gizmos.DrawWireSphere(Vector3.zero, lodDistance * (currentLOD + 1));
        }
        
#if UNITY_EDITOR
        /// <summary>
        /// Validate settings in editor
        /// </summary>
        void OnValidate()
        {
            // Ensure reasonable values
            pointsPerSurface = Mathf.Clamp(pointsPerSurface, 25, 10000);
            pointSize = Mathf.Clamp(pointSize, 0.001f, 0.1f);
            animationSpeed = Mathf.Clamp(animationSpeed, 0f, 5f);
            animationTime = Mathf.Clamp01(animationTime);
            lodDistance = Mathf.Clamp(lodDistance, 1f, 100f);
            maxLODLevel = Mathf.Clamp(maxLODLevel, 1, 5);
            colliderUpdateFrequency = Mathf.Clamp(colliderUpdateFrequency, 1, 60);
        }
#endif
    }
}