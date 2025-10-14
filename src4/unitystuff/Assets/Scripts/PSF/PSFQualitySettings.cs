using UnityEngine;
using System.Collections.Generic;
namespace ProceduralSurface.Unity
{
    /// <summary>
    /// Simple enum for PSF import quality levels
    /// </summary>
    public enum PSFImportQuality
    {
        Low = 0,
        Medium = 1,
        High = 2,
        Ultra = 3
    }

    /// <summary>
    /// Basic quality settings for PSF pipeline
    /// Create this file as: Assets/Scripts/PSF/PSFQualitySettings.cs
    /// </summary>
    [CreateAssetMenu(fileName = "PSFQualitySettings", menuName = "PSF/Quality Settings", order = 1)]
    public class PSFQualitySettings : ScriptableObject
    {
        [Header("Rendering Quality")]
        [Tooltip("Maximum points to generate per PSF object")]
        [Range(100, 10000)]
        public int maxPointsPerObject = 1000;
        
        [Tooltip("Enable level-of-detail system")]
        public bool enableLOD = true;
        
        [Tooltip("Global point size multiplier")]
        [Range(0.1f, 5f)]
        public float pointSizeMultiplier = 1.0f;
        
        [Header("Debug & Development")]
        [Tooltip("Show debug information in game view")]
        public bool showDebugInfo = false;
        
        /// <summary>
        /// Create default quality settings
        /// </summary>
        public static PSFQualitySettings CreateDefault()
        {
            var settings = CreateInstance<PSFQualitySettings>();
            settings.maxPointsPerObject = 1000;
            settings.enableLOD = true;
            settings.pointSizeMultiplier = 1.0f;
            settings.showDebugInfo = false;
            return settings;
        }
    }
}

/*
USAGE:
1. Save as: Assets/Scripts/PSF/PSFQualitySettings.cs
2. Create asset: Right-click in Project -> Create -> PSF -> Quality Settings
3. This provides the basic PSFQualitySettings class that PSFPipelineManager needs
*/