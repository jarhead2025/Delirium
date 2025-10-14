using System.Collections.Generic;
using UnityEngine;

namespace ProceduralSurface.Unity
{
    /// <summary>
    /// Allows building complex objects from multiple PSF components
    /// Handles assembly, joints, and hierarchical management of PSF building blocks
    /// Place this file at: Assets/Scripts/PSF/PSFCompositeObject.cs
    /// </summary>
    [System.Serializable]
    public class PSFCompositeObject : MonoBehaviour
    {
        [Header("Component Definitions")]
        [Tooltip("List of PSF components that make up this composite object")]
        public PSFComponentDefinition[] components = new PSFComponentDefinition[0];
        
        [Header("Assembly Settings")]
        [Tooltip("Automatically assemble components on Start")]
        public bool autoAssemble = true;
        
        [Tooltip("Global offset applied to all components")]
        public Vector3 assemblyOffset = Vector3.zero;
        
        [Tooltip("Global scale applied to all components")]
        [Range(0.1f, 10f)]
        public float assemblyScale = 1f;
        
        [Tooltip("Enable physics joints between components")]
        public bool enableJoints = true;
        
        [Tooltip("Type of joints to create between components")]
        public PSFJointType defaultJointType = PSFJointType.Fixed;
        
        [Header("Animation Coordination")]
        [Tooltip("Synchronize animation across all components")]
        public bool synchronizeAnimation = true;
        
        [Tooltip("Master animation speed for all components")]
        [Range(0f, 5f)]
        public float masterAnimationSpeed = 1f;
        
        [Tooltip("Animation offset between components (creates wave effects)")]
        [Range(0f, 1f)]
        public float animationOffset = 0f;
        
        [Header("LOD Management")]
        [Tooltip("Use unified LOD for all components")]
        public bool unifiedLOD = true;
        
        [Tooltip("LOD distance multiplier for this composite")]
        [Range(0.1f, 5f)]
        public float lodDistanceMultiplier = 1f;
        
        [Header("Physics")]
        [Tooltip("Total mass of the composite object")]
        [Range(0.1f, 1000f)]
        public float totalMass = 10f;
        
        [Tooltip("Distribute mass based on component size")]
        public bool distributeMassBySize = true;
        
        [Tooltip("Physics material for all components")]
        public PhysicMaterial compositeMaterial;
        
        [Header("Destruction")]
        [Tooltip("Enable destruction of individual components")]
        public bool enableDestruction = false;
        
        [Tooltip("Force threshold for component separation")]
        [Range(1f, 1000f)]
        public float destructionThreshold = 100f;
        
        // Runtime state
        private List<PSFComponentInstance> instantiatedComponents = new List<PSFComponentInstance>();
        private List<Joint> joints = new List<Joint>();
        private PSFComponentInstance mainComponent;
        private bool isAssembled = false;
        private Bounds compositeBounds;
        
        // Animation state
        private float masterAnimationTime = 0f;
        private bool isAnimationPlaying = false;
        
        // Events
        public System.Action<PSFCompositeObject> OnAssemblyComplete;
        public System.Action<PSFCompositeObject> OnDisassemblyComplete;
        public System.Action<PSFCompositeObject, PSFComponentInstance> OnComponentDestroyed;
        public System.Action<PSFCompositeObject, float> OnAnimationUpdated;
        
        // Properties
        public bool IsAssembled => isAssembled;
        public int ComponentCount => instantiatedComponents.Count;
        public PSFComponentInstance MainComponent => mainComponent;
        public Bounds CompositeBounds => compositeBounds;
        public bool IsAnimating => isAnimationPlaying;
        
        void Start()
        {
            if (autoAssemble)
            {
                AssembleComponents();
            }
        }
        
        void Update()
        {
            if (isAssembled)
            {
                UpdateAnimation();
                UpdateLOD();
                UpdateBounds();
                
                if (enableDestruction)
                {
                    CheckForDestruction();
                }
            }
        }
        
        void OnDestroy()
        {
            DisassembleComponents();
        }
        
        /// <summary>
        /// Assemble all components into the composite object
        /// </summary>
        public void AssembleComponents()
        {
            if (isAssembled)
            {
                Debug.LogWarning($"PSFCompositeObject '{name}' is already assembled");
                return;
            }
            
            Debug.Log($"Assembling PSF composite object: {name} with {components.Length} components");
            
            try
            {
                CreateComponents();
                SetupPhysics();
                SetupAnimation();
                CalculateCompositeBounds();
                
                isAssembled = true;
                OnAssemblyComplete?.Invoke(this);
                
                Debug.Log($"Successfully assembled composite object: {name}");
            }
            catch (System.Exception e)
            {
                Debug.LogError($"Failed to assemble composite object '{name}': {e.Message}");
                DisassembleComponents();
            }
        }
        
        /// <summary>
        /// Disassemble all components
        /// </summary>
        public void DisassembleComponents()
        {
            if (!isAssembled) return;
            
            Debug.Log($"Disassembling PSF composite object: {name}");
            
            // Destroy all joints
            foreach (var joint in joints)
            {
                if (joint != null) DestroyImmediate(joint);
            }
            joints.Clear();
            
            // Destroy all instantiated components
            foreach (var component in instantiatedComponents)
            {
                if (component.gameObject != null) 
                    DestroyImmediate(component.gameObject);
            }
            instantiatedComponents.Clear();
            
            mainComponent = null;
            isAssembled = false;
            
            OnDisassemblyComplete?.Invoke(this);
        }
        
        /// <summary>
        /// Create and instantiate all components
        /// </summary>
        private void CreateComponents()
        {
            for (int i = 0; i < components.Length; i++)
            {
                var componentDef = components[i];
                
                if (componentDef.psfPrefab == null)
                {
                    Debug.LogWarning($"Component {i} has no PSF prefab assigned");
                    continue;
                }
                
                // Instantiate the component
                GameObject instance = Instantiate(componentDef.psfPrefab, transform);
                instance.name = $"{componentDef.componentName}_{i}";
                
                // Apply transforms
                instance.transform.localPosition = componentDef.localPosition + assemblyOffset;
                instance.transform.localRotation = Quaternion.Euler(componentDef.localRotation);
                instance.transform.localScale = componentDef.localScale * assemblyScale;
                
                // Get or add PSFObject component
                PSFObject psfObj = instance.GetComponent<PSFObject>();
                if (psfObj == null)
                {
                    Debug.LogWarning($"Component prefab '{componentDef.psfPrefab.name}' doesn't have PSFObject component, adding one");
                    psfObj = instance.AddComponent<PSFObject>();
                }
                
                // Create component instance
                var componentInstance = new PSFComponentInstance
                {
                    gameObject = instance,
                    psfObject = psfObj,
                    definition = componentDef,
                    componentIndex = i
                };
                
                instantiatedComponents.Add(componentInstance);
                
                // Set main component
                if (componentDef.isMainComponent || mainComponent == null)
                {
                    mainComponent = componentInstance;
                }
                
                Debug.Log($"Created component: {instance.name} at position {instance.transform.localPosition}");
            }
        }
        
        /// <summary>
        /// Setup physics and joints
        /// </summary>
        private void SetupPhysics()
        {
            if (!enableJoints) return;
            
            // Ensure main component has Rigidbody
            if (mainComponent?.gameObject != null)
            {
                Rigidbody mainRb = mainComponent.gameObject.GetComponent<Rigidbody>();
                if (mainRb == null)
                {
                    mainRb = mainComponent.gameObject.AddComponent<Rigidbody>();
                }
                
                // Set main component mass
                float mainMass = distributeMassBySize ? CalculateComponentMass(mainComponent) : totalMass * 0.5f;
                mainRb.mass = mainMass;
                
                if (compositeMaterial != null)
                {
                    var colliders = mainComponent.gameObject.GetComponentsInChildren<Collider>();
                    foreach (var collider in colliders)
                    {
                        collider.material = compositeMaterial;
                    }
                }
            }
            
            // Create joints for non-main components
            foreach (var component in instantiatedComponents)
            {
                if (component == mainComponent) continue;
                
                CreateJoint(component);
            }
        }
        
        /// <summary>
        /// Create physics joint for a component
        /// </summary>
        private void CreateJoint(PSFComponentInstance component)
        {
            if (component.gameObject == null || mainComponent?.gameObject == null) return;
            
            // Add Rigidbody if needed
            Rigidbody rb = component.gameObject.GetComponent<Rigidbody>();
            if (rb == null)
            {
                rb = component.gameObject.AddComponent<Rigidbody>();
            }
            
            // Set component mass
            float componentMass = distributeMassBySize ? CalculateComponentMass(component) : totalMass / instantiatedComponents.Count;
            rb.mass = componentMass;
            
            // Apply physics material
            if (compositeMaterial != null)
            {
                var colliders = component.gameObject.GetComponentsInChildren<Collider>();
                foreach (var collider in colliders)
                {
                    collider.material = compositeMaterial;
                }
            }
            
            // Create joint based on type
            Joint joint = null;
            switch (component.definition.jointType != PSFJointType.Default ? component.definition.jointType : defaultJointType)
            {
                case PSFJointType.Fixed:
                    joint = CreateFixedJoint(component, rb);
                    break;
                    
                case PSFJointType.Hinge:
                    joint = CreateHingeJoint(component, rb);
                    break;
                    
                case PSFJointType.Spring:
                    joint = CreateSpringJoint(component, rb);
                    break;
                    
                case PSFJointType.Configurable:
                    joint = CreateConfigurableJoint(component, rb);
                    break;
            }
            
            if (joint != null)
            {
                joints.Add(joint);
                Debug.Log($"Created {joint.GetType().Name} for component: {component.gameObject.name}");
            }
        }
        
        /// <summary>
        /// Create fixed joint
        /// </summary>
        private Joint CreateFixedJoint(PSFComponentInstance component, Rigidbody rb)
        {
            FixedJoint joint = component.gameObject.AddComponent<FixedJoint>();
            joint.connectedBody = mainComponent.gameObject.GetComponent<Rigidbody>();
            joint.breakForce = destructionThreshold;
            joint.breakTorque = destructionThreshold;
            return joint;
        }
        
        /// <summary>
        /// Create hinge joint
        /// </summary>
        private Joint CreateHingeJoint(PSFComponentInstance component, Rigidbody rb)
        {
            HingeJoint joint = component.gameObject.AddComponent<HingeJoint>();
            joint.connectedBody = mainComponent.gameObject.GetComponent<Rigidbody>();
            joint.axis = component.definition.jointAxis;
            joint.breakForce = destructionThreshold;
            joint.breakTorque = destructionThreshold;
            
            // Configure limits if specified
            if (component.definition.useLimits)
            {
                joint.useLimits = true;
                joint.limits = new JointLimits
                {
                    min = component.definition.jointLimits.x,
                    max = component.definition.jointLimits.y
                };
            }
            
            return joint;
        }
        
        /// <summary>
        /// Create spring joint
        /// </summary>
        private Joint CreateSpringJoint(PSFComponentInstance component, Rigidbody rb)
        {
            SpringJoint joint = component.gameObject.AddComponent<SpringJoint>();
            joint.connectedBody = mainComponent.gameObject.GetComponent<Rigidbody>();
            joint.spring = component.definition.springForce;
            joint.damper = component.definition.damping;
            joint.breakForce = destructionThreshold;
            joint.breakTorque = destructionThreshold;
            return joint;
        }
        
        /// <summary>
        /// Create configurable joint
        /// </summary>
        private Joint CreateConfigurableJoint(PSFComponentInstance component, Rigidbody rb)
        {
            ConfigurableJoint joint = component.gameObject.AddComponent<ConfigurableJoint>();
            joint.connectedBody = mainComponent.gameObject.GetComponent<Rigidbody>();
            joint.breakForce = destructionThreshold;
            joint.breakTorque = destructionThreshold;
            
            // Configure motion constraints
            joint.xMotion = component.definition.lockX ? ConfigurableJointMotion.Locked : ConfigurableJointMotion.Free;
            joint.yMotion = component.definition.lockY ? ConfigurableJointMotion.Locked : ConfigurableJointMotion.Free;
            joint.zMotion = component.definition.lockZ ? ConfigurableJointMotion.Locked : ConfigurableJointMotion.Free;
            
            return joint;
        }
        
        /// <summary>
        /// Calculate mass for a component based on its size
        /// </summary>
        private float CalculateComponentMass(PSFComponentInstance component)
        {
            if (component.psfObject?.psfAsset?.Info.isValid == true)
            {
                var bounds = component.psfObject.psfAsset.Info.boundingBox;
                float volume = bounds.size.x * bounds.size.y * bounds.size.z;
                return Mathf.Clamp(volume * totalMass * 0.1f, 0.1f, totalMass * 0.8f);
            }
            
            return totalMass / instantiatedComponents.Count;
        }
        
        /// <summary>
        /// Setup animation coordination
        /// </summary>
        private void SetupAnimation()
        {
            if (!synchronizeAnimation) return;
            
            foreach (var component in instantiatedComponents)
            {
                if (component.psfObject != null)
                {
                    component.psfObject.animationSpeed = masterAnimationSpeed;
                    component.psfObject.enableAnimation = true;
                }
            }
        }
        
        /// <summary>
        /// Update animation across all components
        /// </summary>
        private void UpdateAnimation()
        {
            if (!synchronizeAnimation) return;
            
            masterAnimationTime += Time.deltaTime * masterAnimationSpeed;
            if (masterAnimationTime > 1f) masterAnimationTime -= 1f;
            
            for (int i = 0; i < instantiatedComponents.Count; i++)
            {
                var component = instantiatedComponents[i];
                if (component.psfObject != null)
                {
                    float componentTime = masterAnimationTime + (i * animationOffset);
                    if (componentTime > 1f) componentTime -= 1f;
                    
                    component.psfObject.SetAnimationTime(componentTime);
                }
            }
            
            OnAnimationUpdated?.Invoke(this, masterAnimationTime);
        }
        
        /// <summary>
        /// Update LOD for all components
        /// </summary>
        private void UpdateLOD()
        {
            if (!unifiedLOD) return;
            
            Camera mainCamera = Camera.main;
            if (mainCamera == null) return;
            
            float distance = Vector3.Distance(transform.position, mainCamera.transform.position);
            distance /= lodDistanceMultiplier;
            
            foreach (var component in instantiatedComponents)
            {
                if (component.psfObject != null && component.psfObject.enableLOD)
                {
                    int lod = Mathf.Clamp(Mathf.FloorToInt(distance / component.psfObject.lodDistance), 0, component.psfObject.maxLODLevel);
                    component.psfObject.SetLOD(lod);
                }
            }
        }
        
        /// <summary>
        /// Calculate composite bounds
        /// </summary>
        private void CalculateCompositeBounds()
        {
            if (instantiatedComponents.Count == 0) return;
            
            Bounds bounds = new Bounds(transform.position, Vector3.zero);
            bool first = true;
            
            foreach (var component in instantiatedComponents)
            {
                if (component.gameObject != null)
                {
                    Renderer[] renderers = component.gameObject.GetComponentsInChildren<Renderer>();
                    foreach (var renderer in renderers)
                    {
                        if (first)
                        {
                            bounds = renderer.bounds;
                            first = false;
                        }
                        else
                        {
                            bounds.Encapsulate(renderer.bounds);
                        }
                    }
                }
            }
            
            compositeBounds = bounds;
        }
        
        /// <summary>
        /// Update composite bounds
        /// </summary>
        private void UpdateBounds()
        {
            // Update bounds periodically or when components move significantly
            if (Time.frameCount % 30 == 0) // Every 30 frames
            {
                CalculateCompositeBounds();
            }
        }
        
        /// <summary>
        /// Check for destruction of components
        /// </summary>
        private void CheckForDestruction()
        {
            for (int i = joints.Count - 1; i >= 0; i--)
            {
                if (joints[i] == null) // Joint was broken
                {
                    joints.RemoveAt(i);
                    
                    // Find which component lost its joint
                    var brokenComponent = instantiatedComponents.Find(c => 
                        c.gameObject != null && c.gameObject.GetComponent<Joint>() == null);
                    
                    if (brokenComponent != null)
                    {
                        OnComponentDestroyed?.Invoke(this, brokenComponent);
                        
                        // Make the broken component independent
                        brokenComponent.gameObject.transform.SetParent(null);
                        instantiatedComponents.Remove(brokenComponent);
                        
                        Debug.Log($"Component '{brokenComponent.gameObject.name}' separated from composite");
                    }
                }
            }
        }
        
        // =============================================================================
        // PUBLIC API METHODS
        // =============================================================================
        
        /// <summary>
        /// Play animation on all components
        /// </summary>
        public void PlayAnimation()
        {
            isAnimationPlaying = true;
            
            foreach (var component in instantiatedComponents)
            {
                if (component.psfObject != null)
                {
                    component.psfObject.PlayAnimation();
                }
            }
        }
        
        /// <summary>
        /// Pause animation on all components
        /// </summary>
        public void PauseAnimation()
        {
            isAnimationPlaying = false;
            
            foreach (var component in instantiatedComponents)
            {
                if (component.psfObject != null)
                {
                    component.psfObject.PauseAnimation();
                }
            }
        }
        
        /// <summary>
        /// Set animation speed for all components
        /// </summary>
        public void SetAnimationSpeed(float speed)
        {
            masterAnimationSpeed = speed;
            
            foreach (var component in instantiatedComponents)
            {
                if (component.psfObject != null)
                {
                    component.psfObject.animationSpeed = speed;
                }
            }
        }
        
        /// <summary>
        /// Get component by name
        /// </summary>
        public PSFComponentInstance GetComponent(string componentName)
        {
            return instantiatedComponents.Find(c => c.definition.componentName == componentName);
        }
        
        /// <summary>
        /// Get all components of a specific type
        /// </summary>
        public List<PSFComponentInstance> GetComponentsByType(PSFComponentType type)
        {
            return instantiatedComponents.FindAll(c => c.definition.componentType == type);
        }
        
        /// <summary>
        /// Replace a component with a new prefab
        /// </summary>
        public bool ReplaceComponent(string componentName, GameObject newPrefab)
        {
            var component = GetComponent(componentName);
            if (component == null) return false;
            
            Vector3 position = component.gameObject.transform.position;
            Quaternion rotation = component.gameObject.transform.rotation;
            Vector3 scale = component.gameObject.transform.localScale;
            
            // Remove old component
            instantiatedComponents.Remove(component);
            DestroyImmediate(component.gameObject);
            
            // Create new component
            GameObject newInstance = Instantiate(newPrefab, transform);
            newInstance.transform.position = position;
            newInstance.transform.rotation = rotation;
            newInstance.transform.localScale = scale;
            
            // Update component definition
            var definition = component.definition;
            definition.psfPrefab = newPrefab;
            
            var newComponentInstance = new PSFComponentInstance
            {
                gameObject = newInstance,
                psfObject = newInstance.GetComponent<PSFObject>(),
                definition = definition,
                componentIndex = component.componentIndex
            };
            
            instantiatedComponents.Add(newComponentInstance);
            
            // Recreate joint if needed
            if (enableJoints && !definition.isMainComponent)
            {
                CreateJoint(newComponentInstance);
            }
            
            return true;
        }
        
        /// <summary>
        /// Get total memory usage of all components
        /// </summary>
        public long GetTotalMemoryUsage()
        {
            long total = 0;
            foreach (var component in instantiatedComponents)
            {
                if (component.psfObject != null)
                {
                    total += component.psfObject.GetMemoryUsage();
                }
            }
            return total;
        }
        
        void OnDrawGizmosSelected()
        {
            // Draw composite bounds
            Gizmos.color = Color.cyan;
            Gizmos.DrawWireCube(compositeBounds.center, compositeBounds.size);
            
            // Draw component connections
            if (isAssembled && mainComponent != null)
            {
                Gizmos.color = Color.yellow;
                Vector3 mainPos = mainComponent.gameObject.transform.position;
                
                foreach (var component in instantiatedComponents)
                {
                    if (component != mainComponent && component.gameObject != null)
                    {
                        Vector3 componentPos = component.gameObject.transform.position;
                        Gizmos.DrawLine(mainPos, componentPos);
                    }
                }
            }
        }
        
#if UNITY_EDITOR
        [UnityEditor.MenuItem("GameObject/3D Object/PSF Composite Object", false, 11)]
        static void CreatePSFCompositeObject()
        {
            GameObject go = new GameObject("PSF Composite Object");
            go.AddComponent<PSFCompositeObject>();
            UnityEditor.Selection.activeGameObject = go;
        }
#endif
    }

    // =============================================================================
    // SUPPORTING STRUCTURES
    // =============================================================================
    
    [System.Serializable]
    public struct PSFComponentDefinition
    {
        [Header("Basic Properties")]
        public string componentName;
        public GameObject psfPrefab;
        public PSFComponentType componentType;
        public bool isMainComponent;
        
        [Header("Transform")]
        public Vector3 localPosition;
        public Vector3 localRotation;
        public Vector3 localScale;
        
        [Header("Physics")]
        public PSFJointType jointType;
        public Vector3 jointAxis;
        public bool useLimits;
        public Vector2 jointLimits;
        public float springForce;
        public float damping;
        
        [Header("Constraints")]
        public bool lockX;
        public bool lockY;
        public bool lockZ;
        
        [Header("Connection")]
        public string connectionPoint;
        public float connectionStrength;
    }
    
    [System.Serializable]
    public class PSFComponentInstance
    {
        public GameObject gameObject;
        public PSFObject psfObject;
        public PSFComponentDefinition definition;
        public int componentIndex;
        
        public bool IsValid => gameObject != null && psfObject != null;
    }
    
    public enum PSFComponentType
    {
        Body,      // Main structural component
        Limb,      // Arms, legs, appendages
        Joint,     // Connection points
        Detail,    // Decorative elements
        Functional // Tools, weapons, etc.
    }
    
    public enum PSFJointType
    {
        Default,
        Fixed,
        Hinge,
        Spring,
        Configurable
    }
}

/*
USAGE INSTRUCTIONS:

1. Save this file as: Assets/Scripts/PSF/PSFCompositeObject.cs

2. Create a composite object:
   GameObject composite = new GameObject("Robot");
   PSFCompositeObject comp = composite.AddComponent<PSFCompositeObject>();

3. Define components in the inspector:
   comp.components = new PSFComponentDefinition[] {
       new PSFComponentDefinition {
           componentName = "Body",
           psfPrefab = bodyPrefab,
           isMainComponent = true,
           componentType = PSFComponentType.Body
       },
       new PSFComponentDefinition {
           componentName = "LeftArm",
           psfPrefab = armPrefab,
           localPosition = new Vector3(-1, 0, 0),
           jointType = PSFJointType.Hinge
       }
   };

4. Assembly and control:
   comp.AssembleComponents();
   comp.PlayAnimation();
   comp.SetAnimationSpeed(2f);

5. Component management:
   var arm = comp.GetComponent("LeftArm");
   comp.ReplaceComponent("LeftArm", newArmPrefab);
*/