using UnityEngine;
using System.Collections.Generic;

namespace ProceduralSurface.Unity
{
    /// <summary>
    /// Clean GPU compute engine for PSF triangle mesh generation
    /// Generates proper triangulated geometry from PSF mathematical equations
    /// </summary>
    public class PSFComputeEngine : MonoBehaviour
    {
        [Header("Compute Shaders")]
        [Tooltip("Triangle generation compute shader")]
        public ComputeShader triangleGenerationShader;
        
        [Tooltip("Bone animation compute shader")]
        public ComputeShader boneAnimationShader;
        
        [Header("Mesh Generation")]
        [Tooltip("Resolution for generated triangulated mesh")]
        [Range(8, 128)]
        public int meshResolution = 32;
        
        [Header("Performance")]
        [Tooltip("Compute thread group size")]
        [Range(32, 1024)]
        public int threadGroupSize = 64;
        
        [Tooltip("Maximum dispatch calls per frame")]
        [Range(1, 10)]
        public int maxDispatchesPerFrame = 4;
        
        [Header("Debug")]
        [Tooltip("Enable debug output")]
        public bool enableDebug = false;
        
        [Tooltip("Show performance stats")]
        public bool showPerformanceStats = false;
        
        // GPU Buffers for triangulated mesh
        private ComputeBuffer surfaceBuffer;
        private ComputeBuffer anchorBuffer;
        private ComputeBuffer boneBuffer;
        private ComputeBuffer vertexBuffer;
        private ComputeBuffer normalBuffer;
        private ComputeBuffer colorBuffer;
        private ComputeBuffer uvBuffer;
        private ComputeBuffer indexBuffer;
        private ComputeBuffer boneMatrixBuffer;
        private ComputeBuffer parameterBuffer;
        
        // Compute shader kernel IDs
        private int triangleGenerationKernel;
        private int indexGenerationKernel;
        private int boneAnimationKernel;
        
        // Current PSF data
        private PSFCollection currentCollection;
        private int currentSurfaceCount;
        private int currentAnchorCount;
        private int currentBoneCount;
        
        // Current mesh data
        private int currentVertexCount;
        private int currentTriangleCount;
        
        // Animation state
        private float currentAnimationTime;
        private Matrix4x4[] boneTransforms;
        
        // Performance tracking
        private int frameDispatchCount;
        private int totalDispatches;
        
        // Buffer management
        private Dictionary<string, ComputeBuffer> namedBuffers = new Dictionary<string, ComputeBuffer>();
        private bool isInitialized = false;
        
        // Events for mesh generation
        public System.Action<ComputeBuffer, ComputeBuffer, ComputeBuffer, ComputeBuffer, ComputeBuffer> OnMeshGenerated;
        public System.Action<float> OnComputeCompleted;
        
        void Start()
        {
            InitializeComputeShaders();
        }
        
        void Update()
        {
            frameDispatchCount = 0;
            
            if (showPerformanceStats)
            {
                UpdatePerformanceStats();
            }
        }
        
        void OnDestroy()
        {
            CleanupResources();
        }
        
        private void InitializeComputeShaders()
        {
            try
            {
                // Load triangle generation shader
                if (triangleGenerationShader == null)
                    triangleGenerationShader = Resources.Load<ComputeShader>("PSF/Shaders/PSFTriangleGeneration");
                
                if (boneAnimationShader == null)
                    boneAnimationShader = Resources.Load<ComputeShader>("PSF/Shaders/PSFBoneAnimation");
                
                // Get kernel IDs
                if (triangleGenerationShader != null)
                {
                    triangleGenerationKernel = triangleGenerationShader.FindKernel("CSMain");
                    indexGenerationKernel = triangleGenerationShader.FindKernel("GenerateIndices");
                }
                
                if (boneAnimationShader != null)
                    boneAnimationKernel = boneAnimationShader.FindKernel("CSMain");
                
                Debug.Log("PSF Triangle Generation Engine: Shaders initialized successfully");
            }
            catch (System.Exception e)
            {
                Debug.LogError("Failed to initialize triangle generation shaders: " + e.Message);
            }
        }
        
        public void InitializePSFData(PSFCollection collection, int unused)
        {
            if (collection == null || !collection.IsValid)
            {
                Debug.LogError("Cannot initialize with invalid PSF collection");
                return;
            }
            
            CleanupResources();
            
            currentCollection = collection;
            currentSurfaceCount = collection.surfaces.Length;
            currentAnchorCount = collection.anchors.Length;
            currentBoneCount = collection.bones.Length;
            
            // Calculate mesh data
            currentVertexCount = currentSurfaceCount * (meshResolution + 1) * (meshResolution + 1);
            currentTriangleCount = currentSurfaceCount * meshResolution * meshResolution * 2;
            
            try
            {
                CreateTriangleMeshBuffers();
                UploadDataToGPU();
                BindBuffersToShaders();
                
                isInitialized = true;
                
                Debug.Log("PSF Triangle Mesh Engine initialized: " + currentSurfaceCount + " surfaces, " +
                         currentVertexCount + " vertices, " + currentTriangleCount + " triangles");
            }
            catch (System.Exception e)
            {
                Debug.LogError("Failed to initialize PSF triangle mesh data: " + e.Message);
                CleanupResources();
            }
        }
        
        private void CreateTriangleMeshBuffers()
        {
            // PSF data buffers
            if (currentSurfaceCount > 0)
            {
                surfaceBuffer = new ComputeBuffer(currentSurfaceCount, 64);
                RegisterBuffer("_SurfaceBuffer", surfaceBuffer);
            }
            
            if (currentAnchorCount > 0)
            {
                anchorBuffer = new ComputeBuffer(currentAnchorCount, 64);
                RegisterBuffer("_AnchorBuffer", anchorBuffer);
            }
            
            if (currentBoneCount > 0)
            {
                boneBuffer = new ComputeBuffer(currentBoneCount, 64);
                boneMatrixBuffer = new ComputeBuffer(currentBoneCount, 64);
                boneTransforms = new Matrix4x4[currentBoneCount];
                RegisterBuffer("_BoneBuffer", boneBuffer);
                RegisterBuffer("_BoneMatrixBuffer", boneMatrixBuffer);
            }
            
            // Triangulated mesh buffers
            vertexBuffer = new ComputeBuffer(currentVertexCount, 12);  // float3 vertices
            normalBuffer = new ComputeBuffer(currentVertexCount, 12);  // float3 normals
            colorBuffer = new ComputeBuffer(currentVertexCount, 16);   // float4 colors  
            uvBuffer = new ComputeBuffer(currentVertexCount, 8);       // float2 UVs
            indexBuffer = new ComputeBuffer(currentTriangleCount * 3, 4); // uint indices
            
            RegisterBuffer("_VertexBuffer", vertexBuffer);
            RegisterBuffer("_NormalBuffer", normalBuffer);
            RegisterBuffer("_ColorBuffer", colorBuffer);
            RegisterBuffer("_UVBuffer", uvBuffer);
            RegisterBuffer("_IndexBuffer", indexBuffer);
            
            // Parameter buffer for dynamic updates
            parameterBuffer = new ComputeBuffer(currentSurfaceCount * 8, 4);
            RegisterBuffer("_ParameterBuffer", parameterBuffer);
        }
        
        private void UploadDataToGPU()
        {
            if (surfaceBuffer != null && currentCollection.surfaces != null)
            {
                surfaceBuffer.SetData(currentCollection.surfaces);
            }
            
            if (anchorBuffer != null && currentCollection.anchors != null)
            {
                anchorBuffer.SetData(currentCollection.anchors);
            }
            
            if (boneBuffer != null && currentCollection.bones != null)
            {
                boneBuffer.SetData(currentCollection.bones);
                
                // Initialize bone transforms to identity
                for (int i = 0; i < currentBoneCount; i++)
                {
                    boneTransforms[i] = Matrix4x4.identity;
                }
                boneMatrixBuffer.SetData(boneTransforms);
            }
            
            // Initialize parameter buffer with surface equation parameters
            if (parameterBuffer != null)
            {
                float[] parameters = new float[currentSurfaceCount * 8];
                for (int i = 0; i < currentSurfaceCount; i++)
                {
                    var surface = currentCollection.surfaces[i];
                    for (int j = 0; j < 8; j++)
                    {
                        parameters[i * 8 + j] = surface.eqn_params[j];
                    }
                }
                parameterBuffer.SetData(parameters);
            }
        }
        
        private void BindBuffersToShaders()
        {
            // Bind to triangle generation shader
            if (triangleGenerationShader != null)
            {
                BindBufferToShader(triangleGenerationShader, triangleGenerationKernel, "_SurfaceBuffer", surfaceBuffer);
                BindBufferToShader(triangleGenerationShader, triangleGenerationKernel, "_AnchorBuffer", anchorBuffer);
                BindBufferToShader(triangleGenerationShader, triangleGenerationKernel, "_BoneMatrixBuffer", boneMatrixBuffer);
                BindBufferToShader(triangleGenerationShader, triangleGenerationKernel, "_ParameterBuffer", parameterBuffer);
                BindBufferToShader(triangleGenerationShader, triangleGenerationKernel, "_VertexBuffer", vertexBuffer);
                BindBufferToShader(triangleGenerationShader, triangleGenerationKernel, "_NormalBuffer", normalBuffer);
                BindBufferToShader(triangleGenerationShader, triangleGenerationKernel, "_ColorBuffer", colorBuffer);
                BindBufferToShader(triangleGenerationShader, triangleGenerationKernel, "_UVBuffer", uvBuffer);
                
                // Bind to index generation kernel
                BindBufferToShader(triangleGenerationShader, indexGenerationKernel, "_IndexBuffer", indexBuffer);
            }
            
            // Bind to bone animation shader
            if (boneAnimationShader != null)
            {
                BindBufferToShader(boneAnimationShader, boneAnimationKernel, "_BoneBuffer", boneBuffer);
                BindBufferToShader(boneAnimationShader, boneAnimationKernel, "_BoneMatrixBuffer", boneMatrixBuffer);
            }
        }
        
        public void UpdateBoneAnimation(float animationTime, float animationSpeed)
        {
            if (!isInitialized || boneAnimationShader == null || currentBoneCount == 0) return;
            
            if (frameDispatchCount >= maxDispatchesPerFrame) return;
            
            currentAnimationTime = animationTime;
            
            float startTime = Time.realtimeSinceStartup;
            
            // Set animation parameters
            boneAnimationShader.SetFloat("_AnimationTime", animationTime);
            boneAnimationShader.SetFloat("_AnimationSpeed", animationSpeed);
            boneAnimationShader.SetFloat("_DeltaTime", Time.deltaTime);
            boneAnimationShader.SetFloat("_Time", Time.time);
            boneAnimationShader.SetInt("_BoneCount", currentBoneCount);
            
            // Dispatch bone animation compute
            int threadGroups = Mathf.CeilToInt((float)currentBoneCount / threadGroupSize);
            boneAnimationShader.Dispatch(boneAnimationKernel, threadGroups, 1, 1);
            
            frameDispatchCount++;
            
            if (enableDebug)
            {
                float computeTime = (Time.realtimeSinceStartup - startTime) * 1000f;
                Debug.Log("Bone animation compute: " + computeTime.ToString("F2") + "ms for " + currentBoneCount + " bones");
            }
        }
        
        public void GenerateTriangleMesh(float animationTime, Matrix4x4 objectToWorld)
        {
            if (!isInitialized || triangleGenerationShader == null) return;
            
            if (frameDispatchCount >= maxDispatchesPerFrame) return;
            
            float startTime = Time.realtimeSinceStartup;
            
            // Update bone animation first if needed
            if (currentBoneCount > 0)
            {
                UpdateBoneAnimation(animationTime, 1.0f);
            }
            
            // Set triangle generation parameters
            triangleGenerationShader.SetFloat("_AnimationTime", animationTime);
            triangleGenerationShader.SetFloat("_Time", Time.time);
            triangleGenerationShader.SetInt("_Resolution", meshResolution);
            triangleGenerationShader.SetInt("_SurfaceCount", currentSurfaceCount);
            triangleGenerationShader.SetMatrix("_ObjectToWorld", objectToWorld);
            
            // Generate vertices and mesh data
            int vertexThreadGroups = Mathf.CeilToInt((float)currentVertexCount / threadGroupSize);
            triangleGenerationShader.Dispatch(triangleGenerationKernel, vertexThreadGroups, 1, 1);
            
            // Generate triangle indices
            int triangleThreadGroups = Mathf.CeilToInt((float)currentTriangleCount / threadGroupSize);
            triangleGenerationShader.Dispatch(indexGenerationKernel, triangleThreadGroups, 1, 1);
            
            frameDispatchCount += 2;
            
            // Trigger mesh generated event
            if (OnMeshGenerated != null)
                OnMeshGenerated.Invoke(vertexBuffer, normalBuffer, colorBuffer, uvBuffer, indexBuffer);
            
            if (enableDebug)
            {
                float computeTime = (Time.realtimeSinceStartup - startTime) * 1000f;
                Debug.Log("Triangle mesh generation: " + computeTime.ToString("F2") + "ms for " + 
                         currentVertexCount + " vertices, " + currentTriangleCount + " triangles");
            }
        }
        
        public void UpdatePSFCompute(float animationTime, int lodLevel, int pointsPerSurface, Matrix4x4 objectToWorld)
        {
            if (!isInitialized) return;
            
            currentAnimationTime = animationTime;
            
            // Generate triangulated mesh instead of points
            GenerateTriangleMesh(animationTime, objectToWorld);
            
            if (OnComputeCompleted != null)
                OnComputeCompleted.Invoke(Time.realtimeSinceStartup);
        }
        
        public void UpdateSurfaceParameter(int surfaceIndex, int paramIndex, float value)
        {
            if (!isInitialized || surfaceIndex >= currentSurfaceCount || paramIndex >= 8) return;
            
            // Update parameter buffer directly on GPU
            float[] singleParam = { value };
            int bufferIndex = surfaceIndex * 8 + paramIndex;
            
            parameterBuffer.SetData(singleParam, 0, bufferIndex, 1);
            
            if (enableDebug)
            {
                Debug.Log("Updated surface " + surfaceIndex + " parameter " + paramIndex + " to " + value);
            }
        }
        
        public void UpdateSurfaceParameters(int surfaceIndex, float[] parameters)
        {
            if (!isInitialized || surfaceIndex >= currentSurfaceCount || parameters.Length > 8) return;
            
            int bufferOffset = surfaceIndex * 8;
            parameterBuffer.SetData(parameters, 0, bufferOffset, parameters.Length);
            
            if (enableDebug)
            {
                Debug.Log("Updated surface " + surfaceIndex + " with " + parameters.Length + " parameters");
            }
        }
        
        public void SetBuffer(string name, ComputeBuffer buffer)
        {
            if (namedBuffers.ContainsKey(name))
            {
                namedBuffers[name] = buffer;
            }
            else
            {
                namedBuffers.Add(name, buffer);
            }
        }
        
        public ComputeBuffer GetBuffer(string name)
        {
            ComputeBuffer buffer;
            if (namedBuffers.TryGetValue(name, out buffer))
                return buffer;
            return null;
        }
        
        private void RegisterBuffer(string name, ComputeBuffer buffer)
        {
            if (buffer != null)
            {
                namedBuffers[name] = buffer;
            }
        }
        
        private void BindBufferToShader(ComputeShader shader, int kernel, string name, ComputeBuffer buffer)
        {
            if (shader != null && buffer != null)
            {
                shader.SetBuffer(kernel, name, buffer);
            }
        }
        
        private void UpdatePerformanceStats()
        {
            totalDispatches += frameDispatchCount;
            
            if (Time.frameCount % 60 == 0) // Every 60 frames
            {
                float avgDispatchesPerFrame = (float)totalDispatches / 60f;
                
                if (enableDebug)
                {
                    Debug.Log("PSF Compute Performance: " + avgDispatchesPerFrame.ToString("F1") + 
                             " avg dispatches/frame, " + currentVertexCount + " vertices, " + 
                             GetTotalGPUMemoryMB().ToString("F1") + "MB GPU memory");
                }
                
                totalDispatches = 0;
            }
        }
        
        public float GetTotalGPUMemoryMB()
        {
            long totalBytes = 0;
            
            foreach (var buffer in namedBuffers.Values)
            {
                if (buffer != null)
                {
                    totalBytes += buffer.count * buffer.stride;
                }
            }
            
            return totalBytes / (1024f * 1024f);
        }
        
        private void CleanupResources()
        {
            foreach (var buffer in namedBuffers.Values)
            {
                if (buffer != null)
                    buffer.Release();
            }
            
            namedBuffers.Clear();
            
            // Clear specific buffer references
            surfaceBuffer = null;
            anchorBuffer = null;
            boneBuffer = null;
            vertexBuffer = null;
            normalBuffer = null;
            colorBuffer = null;
            uvBuffer = null;
            indexBuffer = null;
            boneMatrixBuffer = null;
            parameterBuffer = null;
            
            isInitialized = false;
            
            if (enableDebug)
            {
                Debug.Log("PSF Compute Engine: Cleaned up all GPU resources");
            }
        }
        
        // Public accessors
        public ComputeBuffer GetVertexBuffer() { return vertexBuffer; }
        public ComputeBuffer GetNormalBuffer() { return normalBuffer; }
        public ComputeBuffer GetColorBuffer() { return colorBuffer; }
        public ComputeBuffer GetUVBuffer() { return uvBuffer; }
        public ComputeBuffer GetIndexBuffer() { return indexBuffer; }
        
        public int GetCurrentVertexCount() { return currentVertexCount; }
        public int GetCurrentTriangleCount() { return currentTriangleCount; }
        public int GetCurrentSurfaceCount() { return currentSurfaceCount; }
        
        public bool IsInitialized() { return isInitialized; }
        
        // Legacy compatibility
        public ComputeBuffer GetPointBuffer() { return vertexBuffer; }
        public int GetCurrentPointCount() { return currentVertexCount; }
        
        void OnGUI()
        {
            if (showPerformanceStats && isInitialized)
            {
                GUILayout.BeginArea(new Rect(10, 150, 400, 200));
                GUILayout.Label("PSF Triangle Mesh Engine Stats:", GUI.skin.box);
                GUILayout.Label("Surfaces: " + currentSurfaceCount);
                GUILayout.Label("Anchors: " + currentAnchorCount);
                GUILayout.Label("Bones: " + currentBoneCount);
                GUILayout.Label("Vertices: " + currentVertexCount);
                GUILayout.Label("Triangles: " + currentTriangleCount);
                GUILayout.Label("GPU Memory: " + GetTotalGPUMemoryMB().ToString("F1") + " MB");
                GUILayout.Label("Resolution: " + meshResolution + "x" + meshResolution);
                GUILayout.Label("Animation Time: " + currentAnimationTime.ToString("F3"));
                GUILayout.EndArea();
            }
        }
    }
}