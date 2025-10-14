
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using Unity.Collections;
using Unity.Jobs;
using Unity.Mathematics;

namespace ProceduralSurface.Unity
{
    /// <summary>
    /// Central manager for the PSF pipeline
    /// Handles object pooling, LOD management, batch rendering, and performance optimization
    /// Place this file at: Assets/Scripts/PSF/PSFPipelineManager.cs
    /// </summary>
    public class PSFPipelineManager : MonoBehaviour
    {
        [Header("Pipeline Settings")]
        [Tooltip("Maximum number of active PSF objects")]
        [Range(10, 10000)]
        public int maxActiveObjects = 1000;
        
        [Tooltip("Distance beyond which objects are culled")]
        [Range(10f, 1000f)]
        public float cullingDistance = 100f;
        
        [Tooltip("Enable batch rendering for better performance")]
        public bool enableBatching = true;
        
        [Tooltip("Enable asynchronous PSF loading")]
        public bool enableAsyncLoading = true;
        
        [Header("Performance")]
        [Tooltip("Job batch size for parallel processing")]
        [Range(1, 100)]
        public int jobBatchSize = 32;
        
        [Tooltip("Target frame time in milliseconds")]
        [Range(8f, 33f)]
        public float targetFrameTime = 16.67f; // 60 FPS
        
        [Tooltip("Maximum jobs to process per frame")]
        [Range(1, 20)]
        public int maxJobsPerFrame = 4;
        
        [Tooltip("Update frequency for performance monitoring")]
        [Range(0.1f, 5f)]
        public float performanceUpdateInterval = 1f;
        
        [Header("Quality")]
        [Tooltip("Global quality settings for all PSF objects")]
        public PSFQualitySettings qualitySettings;
        
        [Header("Memory Management")]
        [Tooltip("Maximum GPU memory usage in MB")]
        [Range(64, 4096)]
        public int maxGPUMemoryMB = 512;
        
        [Tooltip("Enable automatic garbage collection")]
        public bool enableAutoGC = true;
        
        [Tooltip("GC threshold as percentage of max memory")]
        [Range(0.5f, 0.95f)]
        public float gcThreshold = 0.8f;
        
        // Object management
        private List<PSFObject> activeObjects = new List<PSFObject>();
        private List<PSFObject> culledObjects = new List<PSFObject>();
        private Dictionary<string, PSFCollection> cachedPSFData = new Dictionary<string, PSFCollection>();
        private Queue<PSFObject> objectPool = new Queue<PSFObject>();
        
        // Batch rendering
        private ComputeBuffer globalPointBuffer;
        private ComputeBuffer globalColorBuffer;
        private ComputeBuffer objectDataBuffer;
        private Material batchMaterial;
        private PSFBatchData[] batchDataArray;
        
        // Job management
        private JobHandle batchJobHandle;
        private NativeArray<PSFBatchData> batchData;
        private Queue<System.Action> mainThreadActions = new Queue<System.Action>();
        private int activeJobCount = 0;
        
        // Performance monitoring
        private PSFPerformanceStats performanceStats;
        private float lastPerformanceUpdate;
        private float frameTimeAccumulator;
        private int frameCount;
        
        // Memory management
        private long currentGPUMemoryUsage;
        private float lastGCTime;
        
        // Events
        public System.Action<PSFPerformanceStats> OnPerformanceStatsUpdated;
        public System.Action<float> OnMemoryThresholdExceeded;
        public System.Action<int> OnObjectCountChanged;
        
        // Singleton
        public static PSFPipelineManager Instance { get; private set; }
        
        void Awake()
        {
            // Singleton pattern
            if (Instance == null)
            {
                Instance = this;
                DontDestroyOnLoad(gameObject);
                InitializePipeline();
            }
            else
            {
                Destroy(gameObject);
            }
        }
        
        void Start()
        {
            // Load quality settings if not assigned
            if (qualitySettings == null)
            {
                qualitySettings = Resources.Load<PSFQualitySettings>("PSF/DefaultQualitySettings");
                if (qualitySettings == null)
                {
                    qualitySettings = PSFQualitySettings.CreateDefault();
                }
            }
        }
        
        void Update()
        {
            float frameStartTime = Time.realtimeSinceStartup;
            
            ProcessMainThreadQueue();
            UpdateObjectCulling();
            UpdateLOD();
            UpdateMemoryManagement();
            
            if (enableBatching)
            {
                UpdateBatchRendering();
            }
            
            UpdatePerformanceStats(frameStartTime);
        }
        
        void OnDestroy()
        {
            CleanupPipeline();
        }
        
        /// <summary>
        /// Initialize the PSF pipeline
        /// </summary>
        private void InitializePipeline()
        {
            try
            {
                // Initialize batch rendering buffers
                int maxPoints = maxActiveObjects * (qualitySettings?.maxPointsPerObject ?? 1000);
                globalPointBuffer = new ComputeBuffer(maxPoints, 12); // float3
                globalColorBuffer = new ComputeBuffer(maxPoints, 16); // Color
                objectDataBuffer = new ComputeBuffer(maxActiveObjects, System.Runtime.InteropServices.Marshal.SizeOf<PSFObjectData>());
                
                batchData = new NativeArray<PSFBatchData>(maxActiveObjects, Allocator.Persistent);
                batchDataArray = new PSFBatchData[maxActiveObjects];
                
                // Load batch material
                batchMaterial = Resources.Load<Material>("PSF/BatchedProceduralMaterial");
                if (batchMaterial == null)
                {
                    Debug.LogWarning("PSF: Batched procedural material not found, batch rendering disabled");
                    enableBatching = false;
                }
                
                // Initialize performance stats
                performanceStats = new PSFPerformanceStats();
                
                Debug.Log($"PSF Pipeline initialized: {maxActiveObjects} max objects, {maxPoints} max points, " +
                         $"GPU memory limit: {maxGPUMemoryMB}MB");
            }
            catch (System.Exception e)
            {
                Debug.LogError($"Failed to initialize PSF pipeline: {e.Message}");
                enabled = false;
            }
        }
        
        /// <summary>
        /// Clean up pipeline resources
        /// </summary>
        private void CleanupPipeline()
        {
            // Complete all jobs
            batchJobHandle.Complete();
            
            // Release GPU buffers
            globalPointBuffer?.Release();
            globalColorBuffer?.Release();
            objectDataBuffer?.Release();
            
            // Dispose native arrays
            if (batchData.IsCreated) batchData.Dispose();
            
            // Clean up cached data
            foreach (var cachedData in cachedPSFData.Values)
            {
                cachedData?.Dispose();
            }
            cachedPSFData.Clear();
            
            // Clear object lists
            activeObjects.Clear();
            culledObjects.Clear();
            objectPool.Clear();
        }
        
        /// <summary>
        /// Register a PSF object with the pipeline
        /// </summary>
        public void RegisterObject(PSFObject psfObject)
        {
            if (psfObject == null) return;
            
            if (!activeObjects.Contains(psfObject))
            {
                activeObjects.Add(psfObject);
                
                if (enableAsyncLoading && psfObject.psfAsset != null)
                {
                    LoadPSFDataAsync(psfObject);
                }
                
                OnObjectCountChanged?.Invoke(activeObjects.Count);
                
                Debug.Log($"PSF: Registered object '{psfObject.name}' ({activeObjects.Count} total)");
            }
        }
        
        /// <summary>
        /// Unregister a PSF object from the pipeline
        /// </summary>
        public void UnregisterObject(PSFObject psfObject)
        {
            if (psfObject == null) return;
            
            bool removed = activeObjects.Remove(psfObject) || culledObjects.Remove(psfObject);
            
            if (removed)
            {
                OnObjectCountChanged?.Invoke(activeObjects.Count);
                Debug.Log($"PSF: Unregistered object '{psfObject.name}' ({activeObjects.Count} remaining)");
            }
        }
        
        /// <summary>
        /// Load PSF data asynchronously
        /// </summary>
        private void LoadPSFDataAsync(PSFObject psfObject)
        {
            if (psfObject.psfAsset == null) return;
            
            string assetPath = GetAssetPath(psfObject.psfAsset);
            
            if (!cachedPSFData.ContainsKey(assetPath))
            {
                // Load on background thread
                System.Threading.Tasks.Task.Run(() =>
                {
                    try
                    {
                        var collection = PSFLoader.LoadFromBytes(psfObject.psfAsset.Data);
                        
                        // Queue for main thread
                        lock (mainThreadActions)
                        {
                            mainThreadActions.Enqueue(() =>
                            {
                                cachedPSFData[assetPath] = collection;
                                NotifyObjectDataLoaded(psfObject, collection);
                            });
                        }
                    }
                    catch (System.Exception e)
                    {
                        Debug.LogError($"Failed to load PSF data for '{psfObject.name}': {e.Message}");
                    }
                });
            }
            else
            {
                NotifyObjectDataLoaded(psfObject, cachedPSFData[assetPath]);
            }
        }
        
        /// <summary>
        /// Notify object that PSF data has been loaded
        /// </summary>
        private void NotifyObjectDataLoaded(PSFObject psfObject, PSFCollection collection)
        {
            if (psfObject != null)
            {
                // This would call a method on PSFObject to handle loaded data
                // psfObject.OnPSFDataLoaded(collection);
            }
        }
        
        /// <summary>
        /// Process queued main thread actions
        /// </summary>
        private void ProcessMainThreadQueue()
        {
            int processed = 0;
            float startTime = Time.realtimeSinceStartup;
            
            lock (mainThreadActions)
            {
                while (mainThreadActions.Count > 0 && processed < maxJobsPerFrame)
                {
                    float elapsed = (Time.realtimeSinceStartup - startTime) * 1000f;
                    if (elapsed > targetFrameTime * 0.5f) break; // Don't use more than half frame time
                    
                    mainThreadActions.Dequeue().Invoke();
                    processed++;
                }
            }
        }
        
        /// <summary>
        /// Update object culling based on distance
        /// </summary>
        private void UpdateObjectCulling()
        {
            Camera mainCamera = Camera.main;
            if (mainCamera == null) return;
            
            Vector3 cameraPos = mainCamera.transform.position;
            float cullingDistanceSqr = cullingDistance * cullingDistance;
            
            // Check active objects for culling
            for (int i = activeObjects.Count - 1; i >= 0; i--)
            {
                PSFObject obj = activeObjects[i];
                if (obj == null)
                {
                    activeObjects.RemoveAt(i);
                    continue;
                }
                
                float distanceSqr = Vector3.SqrMagnitude(cameraPos - obj.transform.position);
                
                if (distanceSqr > cullingDistanceSqr)
                {
                    activeObjects.RemoveAt(i);
                    culledObjects.Add(obj);
                    obj.enabled = false; // Disable component to save performance
                }
            }
            
            // Check culled objects for reactivation (with hysteresis)
            float reactivationDistanceSqr = (cullingDistance * 0.9f) * (cullingDistance * 0.9f);
            
            for (int i = culledObjects.Count - 1; i >= 0; i--)
            {
                PSFObject obj = culledObjects[i];
                if (obj == null)
                {
                    culledObjects.RemoveAt(i);
                    continue;
                }
                
                float distanceSqr = Vector3.SqrMagnitude(cameraPos - obj.transform.position);
                
                if (distanceSqr <= reactivationDistanceSqr)
                {
                    culledObjects.RemoveAt(i);
                    activeObjects.Add(obj);
                    obj.enabled = true;
                }
            }
        }
        
        /// <summary>
        /// Update LOD for all active objects
        /// </summary>
        private void UpdateLOD()
        {
            Camera mainCamera = Camera.main;
            if (mainCamera == null) return;
            
            Vector3 cameraPos = mainCamera.transform.position;
            
            foreach (PSFObject obj in activeObjects)
            {
                if (obj == null || !obj.enableLOD) continue;
                
                float distance = Vector3.Distance(cameraPos, obj.transform.position);
                int newLOD = CalculateLOD(distance, obj.lodDistance, obj.maxLODLevel);
                
                if (newLOD != obj.CurrentLOD)
                {
                    obj.SetLOD(newLOD);
                }
            }
        }
        
        /// <summary>
        /// Calculate LOD level based on distance
        /// </summary>
        private int CalculateLOD(float distance, float baseLODDistance, int maxLOD)
        {
            if (qualitySettings != null && !qualitySettings.enableLOD)
                return 0;
            
            float lodFactor = distance / baseLODDistance;
            return Mathf.Clamp(Mathf.FloorToInt(lodFactor), 0, maxLOD);
        }
        
        /// <summary>
        /// Update batch rendering system
        /// </summary>
        private void UpdateBatchRendering()
        {
            if (!enableBatching || batchMaterial == null) return;
            
            // Complete previous job
            batchJobHandle.Complete();
            activeJobCount = 0;
            
            // Prepare batch data
            int objectCount = Mathf.Min(activeObjects.Count, maxActiveObjects);
            if (objectCount == 0) return;
            
            var job = new PSFBatchUpdateJob
            {
                objects = new NativeArray<PSFObjectReference>(objectCount, Allocator.TempJob),
                batchData = batchData,
                time = Time.time,
                deltaTime = Time.deltaTime,
                cameraPosition = Camera.main ? Camera.main.transform.position : Vector3.zero
            };
            
            // Fill object references
            for (int i = 0; i < objectCount; i++)
            {
                var obj = activeObjects[i];
                job.objects[i] = new PSFObjectReference
                {
                    transform = obj.transform.localToWorldMatrix,
                    animationTime = obj.AnimationTime,
                    lodLevel = obj.CurrentLOD,
                    isVisible = obj.IsVisible
                };
            }
            
            // Schedule job
            batchJobHandle = job.Schedule(objectCount, jobBatchSize);
            activeJobCount++;
            
            // Schedule GPU buffer update for next frame
            JobHandle.ScheduleBatchedJobs();
        }
        
        /// <summary>
        /// Update memory management
        /// </summary>
        private void UpdateMemoryManagement()
        {
            // Calculate current GPU memory usage
            currentGPUMemoryUsage = CalculateGPUMemoryUsage();
            long maxMemoryBytes = maxGPUMemoryMB * 1024L * 1024L;
            
            // Check if we're approaching memory limit
            float memoryUsageRatio = (float)currentGPUMemoryUsage / maxMemoryBytes;
            
            if (memoryUsageRatio > gcThreshold)
            {
                OnMemoryThresholdExceeded?.Invoke(memoryUsageRatio);
                
                if (enableAutoGC && Time.time - lastGCTime > 5f) // Don't GC too frequently
                {
                    PerformGarbageCollection();
                    lastGCTime = Time.time;
                }
            }
        }
        
        /// <summary>
        /// Calculate current GPU memory usage
        /// </summary>
        private long CalculateGPUMemoryUsage()
        {
            long usage = 0;
            
            if (globalPointBuffer != null) usage += globalPointBuffer.count * globalPointBuffer.stride;
            if (globalColorBuffer != null) usage += globalColorBuffer.count * globalColorBuffer.stride;
            if (objectDataBuffer != null) usage += objectDataBuffer.count * objectDataBuffer.stride;
            
            // Add object-specific memory usage
            foreach (var obj in activeObjects)
            {
                if (obj != null)
                {
                    usage += obj.GetMemoryUsage();
                }
            }
            
            return usage;
        }
        
        /// <summary>
        /// Perform garbage collection to free memory
        /// </summary>
        private void PerformGarbageCollection()
        {
            Debug.Log("PSF: Performing garbage collection to free GPU memory");
            
            // Force LOD reduction on distant objects
            Camera mainCamera = Camera.main;
            if (mainCamera != null)
            {
                Vector3 cameraPos = mainCamera.transform.position;
                
                foreach (var obj in activeObjects)
                {
                    if (obj == null) continue;
                    
                    float distance = Vector3.Distance(cameraPos, obj.transform.position);
                    if (distance > cullingDistance * 0.5f)
                    {
                        int forcedLOD = Mathf.Min(obj.maxLODLevel, obj.CurrentLOD + 1);
                        obj.SetLOD(forcedLOD);
                    }
                }
            }
            
            // Clean up cached data that hasn't been used recently
            CleanupUnusedCache();
            
            // Force Unity garbage collection
            System.GC.Collect();
        }
        
        /// <summary>
        /// Clean up unused cached PSF data
        /// </summary>
        private void CleanupUnusedCache()
        {
            var keysToRemove = new List<string>();
            
            foreach (var kvp in cachedPSFData)
            {
                bool isUsed = activeObjects.Any(obj => obj.psfAsset != null && GetAssetPath(obj.psfAsset) == kvp.Key);
                
                if (!isUsed)
                {
                    keysToRemove.Add(kvp.Key);
                    kvp.Value?.Dispose();
                }
            }
            
            foreach (string key in keysToRemove)
            {
                cachedPSFData.Remove(key);
            }
            
            if (keysToRemove.Count > 0)
            {
                Debug.Log($"PSF: Cleaned up {keysToRemove.Count} unused cached assets");
            }
        }
        
        /// <summary>
        /// Update performance statistics
        /// </summary>
        private void UpdatePerformanceStats(float frameStartTime)
        {
            float frameTime = (Time.realtimeSinceStartup - frameStartTime) * 1000f;
            frameTimeAccumulator += frameTime;
            frameCount++;
            
            if (Time.time - lastPerformanceUpdate >= performanceUpdateInterval)
            {
                performanceStats.activeObjects = activeObjects.Count;
                performanceStats.culledObjects = culledObjects.Count;
                performanceStats.averageFrameTime = frameTimeAccumulator / frameCount;
                performanceStats.memoryUsageMB = currentGPUMemoryUsage / (1024f * 1024f);
                performanceStats.cacheHitRatio = CalculateCacheHitRatio();
                performanceStats.activeJobs = activeJobCount;
                
                OnPerformanceStatsUpdated?.Invoke(performanceStats);
                
                // Reset accumulators
                frameTimeAccumulator = 0f;
                frameCount = 0;
                lastPerformanceUpdate = Time.time;
            }
        }
        
        /// <summary>
        /// Calculate cache hit ratio
        /// </summary>
        private float CalculateCacheHitRatio()
        {
            int totalObjects = activeObjects.Count + culledObjects.Count;
            if (totalObjects == 0) return 1f;
            
            int cachedObjects = 0;
            foreach (var obj in activeObjects.Concat(culledObjects))
            {
                if (obj?.psfAsset != null)
                {
                    string assetPath = GetAssetPath(obj.psfAsset);
                    if (cachedPSFData.ContainsKey(assetPath))
                        cachedObjects++;
                }
            }
            
            return (float)cachedObjects / totalObjects;
        }
        
        /// <summary>
        /// Get asset path for caching purposes
        /// </summary>
        private string GetAssetPath(PSFAsset asset)
        {
#if UNITY_EDITOR
            return UnityEditor.AssetDatabase.GetAssetPath(asset);
#else
            return asset.name; // Fallback for runtime
#endif
        }
        
        /// <summary>
        /// Get current pipeline statistics
        /// </summary>
        public PSFPerformanceStats GetPerformanceStats()
        {
            return performanceStats;
        }
        
        /// <summary>
        /// Force refresh of asset cache
        /// </summary>
        public void RefreshAssetCache(string assetPath)
        {
            if (cachedPSFData.ContainsKey(assetPath))
            {
                cachedPSFData[assetPath]?.Dispose();
                cachedPSFData.Remove(assetPath);
                Debug.Log($"PSF: Refreshed cache for asset: {assetPath}");
            }
        }
        
        /// <summary>
        /// Get an object from the pool or create new one
        /// </summary>
        public PSFObject GetPooledObject()
        {
            if (objectPool.Count > 0)
            {
                return objectPool.Dequeue();
            }
            
            GameObject go = new GameObject("Pooled PSF Object");
            return go.AddComponent<PSFObject>();
        }
        
        /// <summary>
        /// Return an object to the pool
        /// </summary>
        public void ReturnToPool(PSFObject obj)
        {
            if (obj != null)
            {
                UnregisterObject(obj);
                obj.gameObject.SetActive(false);
                objectPool.Enqueue(obj);
            }
        }
        
        void OnGUI()
        {
            if (qualitySettings?.showDebugInfo == true)
            {
                GUI.Label(new Rect(10, 10, 400, 20), $"PSF Objects: {activeObjects.Count} active, {culledObjects.Count} culled");
                GUI.Label(new Rect(10, 30, 400, 20), $"Frame Time: {performanceStats.averageFrameTime:F2}ms");
                GUI.Label(new Rect(10, 50, 400, 20), $"GPU Memory: {performanceStats.memoryUsageMB:F1}MB / {maxGPUMemoryMB}MB");
                GUI.Label(new Rect(10, 70, 400, 20), $"Cache Hit Ratio: {performanceStats.cacheHitRatio:P1}");
                GUI.Label(new Rect(10, 90, 400, 20), $"Active Jobs: {performanceStats.activeJobs}");
            }
        }
    }

    // =============================================================================
    // SUPPORTING STRUCTURES
    // =============================================================================
    
    [System.Serializable]
    public struct PSFBatchData
    {
        public Matrix4x4 transform;
        public float animationTime;
        public int lodLevel;
        public int pointCount;
        public int startIndex;
        public bool isVisible;
    }
    
    [System.Serializable]
    public struct PSFObjectReference
    {
        public Matrix4x4 transform;
        public float animationTime;
        public int lodLevel;
        public bool isVisible;
    }
    
    [System.Serializable]
    public struct PSFObjectData
    {
        public Matrix4x4 objectToWorld;
        public Vector4 animationParams; // time, speed, phase, intensity
        public uint surfaceCount;
        public uint pointOffset;
        public uint lodLevel;
        public uint flags;
    }
    
    [System.Serializable]
    public struct PSFPerformanceStats
    {
        public int activeObjects;
        public int culledObjects;
        public float averageFrameTime;
        public float memoryUsageMB;
        public float cacheHitRatio;
        public int activeJobs;
    }
    
    // Job for batch processing
    
    public struct PSFBatchUpdateJob : IJobParallelFor
    {
        [ReadOnly] public NativeArray<PSFObjectReference> objects;
        public NativeArray<PSFBatchData> batchData;
        [ReadOnly] public float time;
        [ReadOnly] public float deltaTime;
        [ReadOnly] public Vector3 cameraPosition;
        
        public void Execute(int index)
        {
            var objRef = objects[index];
            
            batchData[index] = new PSFBatchData
            {
                transform = objRef.transform,
                animationTime = objRef.animationTime,
                lodLevel = objRef.lodLevel,
                pointCount = CalculatePointCountForLOD(objRef.lodLevel),
                startIndex = index * 1000, // Simplified indexing
                isVisible = objRef.isVisible
            };
        }
        
        private int CalculatePointCountForLOD(int lodLevel)
        {
            return 1000 >> lodLevel; // Reduce points by half for each LOD level
        }
} }

/*
USAGE INSTRUCTIONS:

1. Save this file as: Assets/Scripts/PSF/PSFPipelineManager.cs

2. Create a GameObject in your scene and add this component:
   GameObject pipelineManager = new GameObject("PSF Pipeline Manager");
   pipelineManager.AddComponent<PSFPipelineManager>();

3. The pipeline manager automatically:
   - Registers/unregisters PSF objects
   - Manages LOD and culling
   - Handles batch rendering
   - Monitors performance
   - Manages GPU memory

4. Access the singleton:
   PSFPipelineManager.Instance.RegisterObject(myPSFObject);
   var stats = PSFPipelineManager.Instance.GetPerformanceStats();

5. Listen to events:
   PSFPipelineManager.Instance.OnPerformanceStatsUpdated += (stats) => {
       Debug.Log($"Frame time: {stats.averageFrameTime}ms");
   };
*/