using UnityEngine;
using UnityEditor;
using System.IO;

namespace ProceduralSurface.Unity.Editor
{
    /// <summary>
    /// Custom inspector for PSFAsset ScriptableObjects
    /// Provides detailed information, preview, and utility functions
    /// Place this file at: Assets/Scripts/PSF/Editor/PSFAssetEditor.cs
    /// </summary>
    [CustomEditor(typeof(PSFAsset))]
    public class PSFAssetEditor : UnityEditor.Editor
    {
        private bool showAssetInfo = true;
        private bool showColorAnalysis = false;
        private bool showPerformanceInfo = false;
        private bool showPreview = true;
        private bool showUtilities = false;
        
        private Mesh previewMesh;
        private bool previewMeshNeedsUpdate = true;
        private Material previewMaterial;
        private PreviewRenderUtility previewUtility;
        private Vector2 previewDir = new Vector2(120f, -20f);
        
        // Performance tracking
        private System.Diagnostics.Stopwatch loadTimer;
        private long lastLoadTime;
        
        void OnEnable()
        {
            loadTimer = new System.Diagnostics.Stopwatch();
            CreatePreviewUtility();
        }
        
        void OnDisable()
        {
            CleanupPreview();
        }
        
        public override void OnInspectorGUI()
        {
            PSFAsset psfAsset = (PSFAsset)target;
            
            EditorGUILayout.Space();
            
            // Header with asset status
            DrawHeaderSection(psfAsset);
            
            EditorGUILayout.Space();
            
            // Asset Information Section
            showAssetInfo = EditorGUILayout.Foldout(showAssetInfo, "Asset Information", true);
            if (showAssetInfo)
            {
                DrawAssetInfoSection(psfAsset);
            }
            
            EditorGUILayout.Space();
            
            // Color Analysis Section
            showColorAnalysis = EditorGUILayout.Foldout(showColorAnalysis, "Color Analysis", true);
            if (showColorAnalysis)
            {
                DrawColorAnalysisSection(psfAsset);
            }
            
            EditorGUILayout.Space();
            
            // Performance Information Section
            showPerformanceInfo = EditorGUILayout.Foldout(showPerformanceInfo, "Performance Information", true);
            if (showPerformanceInfo)
            {
                DrawPerformanceInfoSection(psfAsset);
            }
            
            EditorGUILayout.Space();
            
            // Preview Section
            showPreview = EditorGUILayout.Foldout(showPreview, "3D Preview", true);
            if (showPreview)
            {
                DrawPreviewSection(psfAsset);
            }
            
            EditorGUILayout.Space();
            
            // Import Settings (from base inspector)
            EditorGUILayout.LabelField("Import Settings", EditorStyles.boldLabel);
            DrawDefaultInspector();
            
            EditorGUILayout.Space();
            
            // Utilities Section
            showUtilities = EditorGUILayout.Foldout(showUtilities, "Utilities", true);
            if (showUtilities)
            {
                DrawUtilitiesSection(psfAsset);
            }
        }
        
        private void DrawHeaderSection(PSFAsset psfAsset)
        {
            EditorGUILayout.BeginHorizontal("box");
            
            // Status icon
            GUIContent statusIcon;
            Color statusColor;
            
            if (psfAsset.HasValidationErrors)
            {
                statusIcon = EditorGUIUtility.IconContent("console.erroricon");
                statusColor = Color.red;
            }
            else if (psfAsset.IsLoaded)
            {
                statusIcon = EditorGUIUtility.IconContent("console.infoicon");
                statusColor = Color.green;
            }
            else
            {
                statusIcon = EditorGUIUtility.IconContent("console.warnicon");
                statusColor = Color.yellow;
            }
            
            GUI.color = statusColor;
            GUILayout.Label(statusIcon, GUILayout.Width(16), GUILayout.Height(16));
            GUI.color = Color.white;
            
            // Asset name and status
            EditorGUILayout.BeginVertical();
            EditorGUILayout.LabelField(psfAsset.name, EditorStyles.boldLabel);
            
            if (psfAsset.Info.isValid)
            {
                EditorGUILayout.LabelField(psfAsset.Info.GetSummary(), EditorStyles.miniLabel);
            }
            else
            {
                EditorGUILayout.LabelField("Invalid or corrupted PSF file", EditorStyles.miniLabel);
            }
            EditorGUILayout.EndVertical();
            
            EditorGUILayout.EndHorizontal();
        }
        
        private void DrawAssetInfoSection(PSFAsset psfAsset)
        {
            EditorGUI.indentLevel++;
            
            if (!psfAsset.Info.isValid)
            {
                EditorGUILayout.HelpBox("PSF file is invalid or corrupted", MessageType.Error);
                EditorGUI.indentLevel--;
                return;
            }
            
            var info = psfAsset.Info;
            
            // Basic statistics
            EditorGUILayout.BeginVertical("box");
            EditorGUILayout.LabelField("Geometry Statistics", EditorStyles.boldLabel);
            
            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.LabelField($"Surfaces: {info.surfaceCount}", GUILayout.Width(120));
            EditorGUILayout.LabelField($"Anchors: {info.anchorCount}", GUILayout.Width(120));
            EditorGUILayout.LabelField($"Bones: {info.boneCount}");
            EditorGUILayout.EndHorizontal();
            
            EditorGUILayout.LabelField($"File Size: {info.fileSize / 1024f:F1} KB");
            EditorGUILayout.LabelField($"Complexity Score: {info.ComplexityScore:F3}");
            EditorGUILayout.LabelField($"Has Animation: {(info.HasAnimation ? "Yes" : "No")}");
            EditorGUILayout.LabelField($"Created: {info.CreationDate:yyyy-MM-dd HH:mm:ss}");
            
            EditorGUILayout.EndVertical();
            
            // Bounding box
            EditorGUILayout.BeginVertical("box");
            EditorGUILayout.LabelField("Bounding Box", EditorStyles.boldLabel);
            EditorGUILayout.LabelField($"Center: {info.boundingBox.center}");
            EditorGUILayout.LabelField($"Size: {info.boundingBox.size}");
            EditorGUILayout.LabelField($"Volume: {info.boundingBox.size.x * info.boundingBox.size.y * info.boundingBox.size.z:F3}");
            EditorGUILayout.EndVertical();
            
            EditorGUI.indentLevel--;
        }
        
        private void DrawColorAnalysisSection(PSFAsset psfAsset)
        {
            EditorGUI.indentLevel++;
            
            if (!psfAsset.IsLoaded)
            {
                if (GUILayout.Button("Load PSF Data for Analysis"))
                {
                    loadTimer.Restart();
                    var loadedCollection = psfAsset.GetCollection();
                    loadTimer.Stop();
                    lastLoadTime = loadTimer.ElapsedMilliseconds;
                    
                    if (loadedCollection != null)
                    {
                        previewMeshNeedsUpdate = true;
                    }
                }
                EditorGUI.indentLevel--;
                return;
            }
            
            var collection = psfAsset.GetCollection();
            if (collection == null)
            {
                EditorGUILayout.HelpBox("Failed to load PSF collection", MessageType.Error);
                EditorGUI.indentLevel--;
                return;
            }
            
            EditorGUILayout.BeginVertical("box");
            EditorGUILayout.LabelField("Color Information", EditorStyles.boldLabel);
            
            // Analyze color equations
            var colorEquationCounts = new System.Collections.Generic.Dictionary<byte, int>();
            int totalChannels = 0;
            
            for (int i = 0; i < collection.surfaces.Length; i++)
            {
                var surface = collection.surfaces[i];
                
                // Replace GetValueOrDefault with explicit checks for compatibility
                if (!colorEquationCounts.ContainsKey(surface.color_eqn_r))
                    colorEquationCounts[surface.color_eqn_r] = 0;
                colorEquationCounts[surface.color_eqn_r]++;
                
                if (!colorEquationCounts.ContainsKey(surface.color_eqn_g))
                    colorEquationCounts[surface.color_eqn_g] = 0;
                colorEquationCounts[surface.color_eqn_g]++;
                
                if (!colorEquationCounts.ContainsKey(surface.color_eqn_b))
                    colorEquationCounts[surface.color_eqn_b] = 0;
                colorEquationCounts[surface.color_eqn_b]++;
                
                totalChannels += 3;
            }
            
            EditorGUILayout.LabelField("Color Equation Usage:");
            EditorGUI.indentLevel++;
            foreach (var kvp in colorEquationCounts)
            {
                string equationName = GetColorEquationName(kvp.Key);
                float percentage = (kvp.Value * 100f) / totalChannels;
                EditorGUILayout.LabelField($"{equationName}: {kvp.Value} ({percentage:F1}%)");
            }
            EditorGUI.indentLevel--;
            
            EditorGUILayout.EndVertical();
            
            EditorGUI.indentLevel--;
        }
        
        private void DrawPerformanceInfoSection(PSFAsset psfAsset)
        {
            EditorGUI.indentLevel++;
            
            EditorGUILayout.BeginVertical("box");
            EditorGUILayout.LabelField("Performance Metrics", EditorStyles.boldLabel);
            
            if (psfAsset.IsLoaded)
            {
                EditorGUILayout.LabelField($"Load Time: {lastLoadTime} ms");
                EditorGUILayout.LabelField($"Memory Usage: {psfAsset.GetMemoryUsage() / 1024f:F1} KB");
                
                // Estimated rendering performance
                var info = psfAsset.Info;
                int estimatedPoints = (int)(info.surfaceCount * 100); // Rough estimate
                EditorGUILayout.LabelField($"Estimated Points: {estimatedPoints:N0}");
                
                // LOD recommendations
                EditorGUILayout.LabelField("Recommended Settings:");
                EditorGUI.indentLevel++;
                
                if (info.ComplexityScore > 0.5f)
                {
                    EditorGUILayout.LabelField("• Enable LOD for this asset", EditorStyles.miniLabel);
                    EditorGUILayout.LabelField("• Consider lower point density", EditorStyles.miniLabel);
                }
                else
                {
                    EditorGUILayout.LabelField("• Good performance expected", EditorStyles.miniLabel);
                }
                
                if (info.HasAnimation)
                {
                    EditorGUILayout.LabelField("• Animation may impact performance", EditorStyles.miniLabel);
                }
                
                EditorGUI.indentLevel--;
            }
            else
            {
                EditorGUILayout.LabelField("Load PSF data to see performance metrics");
            }
            
            EditorGUILayout.EndVertical();
            
            EditorGUI.indentLevel--;
        }
        
        private void DrawPreviewSection(PSFAsset psfAsset)
        {
            EditorGUI.indentLevel++;
            
            if (!psfAsset.IsLoaded)
            {
                EditorGUILayout.HelpBox("Load PSF data to see 3D preview", MessageType.Info);
                EditorGUI.indentLevel--;
                return;
            }
            
            // Preview controls
            EditorGUILayout.BeginHorizontal();
            if (GUILayout.Button("Regenerate Preview Mesh"))
            {
                GeneratePreviewMesh(psfAsset);
            }
            
            if (GUILayout.Button("Reset Camera"))
            {
                previewDir = new Vector2(120f, -20f);
            }
            EditorGUILayout.EndHorizontal();
            
            // 3D Preview
            if (previewMesh != null && previewUtility != null)
            {
                Rect previewRect = GUILayoutUtility.GetRect(256, 256);
                
                // Handle mouse input for camera control
                Event evt = Event.current;
                if (evt.type == EventType.MouseDrag && previewRect.Contains(evt.mousePosition))
                {
                    previewDir.x += evt.delta.x;
                    previewDir.y -= evt.delta.y;
                    evt.Use();
                    Repaint();
                }
                
                // Render preview
                RenderPreview(previewRect);
            }
            else
            {
                EditorGUILayout.LabelField("No preview mesh available");
            }
            
            EditorGUI.indentLevel--;
        }
        
        private void DrawUtilitiesSection(PSFAsset psfAsset)
        {
            EditorGUI.indentLevel++;
            
            EditorGUILayout.BeginVertical("box");
            EditorGUILayout.LabelField("Asset Operations", EditorStyles.boldLabel);
            
            EditorGUILayout.BeginHorizontal();
            if (GUILayout.Button("Create GameObject"))
            {
                CreatePSFGameObject(psfAsset);
            }
            
            if (GUILayout.Button("Export to OBJ"))
            {
                ExportToOBJ(psfAsset);
            }
            EditorGUILayout.EndHorizontal();
            
            EditorGUILayout.BeginHorizontal();
            if (GUILayout.Button("Validate Asset"))
            {
                ValidateAsset(psfAsset);
            }
            
            if (GUILayout.Button("Reimport"))
            {
                ReimportAsset(psfAsset);
            }
            EditorGUILayout.EndHorizontal();
            
            EditorGUILayout.EndVertical();
            
            // Debug operations
            EditorGUILayout.BeginVertical("box");
            EditorGUILayout.LabelField("Debug Operations", EditorStyles.boldLabel);
            
            if (GUILayout.Button("Print Collection Summary"))
            {
                PrintCollectionSummary(psfAsset);
            }
            
            if (GUILayout.Button("Analyze Color Equations"))
            {
                AnalyzeColorEquations(psfAsset);
            }
            
            EditorGUILayout.EndVertical();
            
            EditorGUI.indentLevel--;
        }
        
        private void CreatePreviewUtility()
        {
            if (previewUtility == null)
            {
                previewUtility = new PreviewRenderUtility();
                previewUtility.camera.transform.position = new Vector3(0, 0, -5);
                previewUtility.camera.transform.rotation = Quaternion.identity;
                
                // Create preview material
                previewMaterial = new Material(Shader.Find("Standard"));
                previewMaterial.color = Color.white;
            }
        }
        
        private void CleanupPreview()
        {
            if (previewUtility != null)
            {
                previewUtility.Cleanup();
                previewUtility = null;
            }
            
            if (previewMaterial != null)
            {
                DestroyImmediate(previewMaterial);
                previewMaterial = null;
            }
            
            if (previewMesh != null)
            {
                DestroyImmediate(previewMesh);
                previewMesh = null;
            }
        }
        
        private void GeneratePreviewMesh(PSFAsset psfAsset)
        {
            try
            {
                var collection = psfAsset.GetCollection();
                if (collection != null)
                {
                    if (previewMesh != null)
                    {
                        DestroyImmediate(previewMesh);
                    }
                    
                    previewMesh = CreateSimplePreviewMesh(collection);
                    previewMesh.name = $"{psfAsset.name}_Preview";
                    
                    Debug.Log($"Generated preview mesh: {previewMesh.vertexCount} vertices");
                }
            }
            catch (System.Exception e)
            {
                Debug.LogError($"Failed to generate preview mesh: {e.Message}");
            }
        }
        
        private Mesh CreateSimplePreviewMesh(PSFCollection collection)
        {
            var mesh = new Mesh();
            var vertices = new System.Collections.Generic.List<Vector3>();
            var triangles = new System.Collections.Generic.List<int>();
            var normals = new System.Collections.Generic.List<Vector3>();
            
            // Generate simple mesh from surfaces
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
                
                // Add normals (pointing up for simplicity)
                for (int j = 0; j < 4; j++)
                {
                    normals.Add(Vector3.up);
                }
                
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
            mesh.normals = normals.ToArray();
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
        
        private void RenderPreview(Rect rect)
        {
            if (previewUtility == null || previewMesh == null) return;
            
            previewUtility.BeginPreview(rect, GUIStyle.none);
            
            // Set up camera
            Camera camera = previewUtility.camera;
            camera.transform.rotation = Quaternion.Euler(previewDir.y, previewDir.x, 0);
            camera.transform.position = camera.transform.rotation * Vector3.back * 5f;
            camera.nearClipPlane = 0.1f;
            camera.farClipPlane = 100f;
            
            // Render mesh
            Matrix4x4 matrix = Matrix4x4.identity;
            previewUtility.DrawMesh(previewMesh, matrix, previewMaterial, 0);
            
            // Add lighting
            previewUtility.lights[0].intensity = 1.4f;
            previewUtility.lights[0].transform.rotation = Quaternion.Euler(40f, 40f, 0f);
            previewUtility.lights[1].intensity = 1.4f;
            
            camera.Render();
            
            Texture texture = previewUtility.EndPreview();
            GUI.DrawTexture(rect, texture, ScaleMode.StretchToFill, false);
        }
        
        private void CreatePSFGameObject(PSFAsset psfAsset)
        {
            GameObject go = psfAsset.CreateGameObject();
            Selection.activeGameObject = go;
            EditorGUIUtility.PingObject(go);
            
            Debug.Log($"Created PSF GameObject: {go.name}");
        }
        
        private void ExportToOBJ(PSFAsset psfAsset)
        {
            string path = EditorUtility.SaveFilePanel(
                "Export PSF to OBJ", 
                "", 
                psfAsset.name + ".obj", 
                "obj"
            );
            
            if (!string.IsNullOrEmpty(path))
            {
                try
                {
                    var collection = psfAsset.GetCollection();
                    if (collection != null)
                    {
                        Mesh mesh = CreateSimplePreviewMesh(collection);
                        PSFOBJExporter.Export(mesh, path);
                        
                        Debug.Log($"Exported PSF to OBJ: {path}");
                        EditorUtility.DisplayDialog("Export Complete", $"PSF exported to:\n{path}", "OK");
                    }
                }
                catch (System.Exception e)
                {
                    Debug.LogError($"Export failed: {e.Message}");
                    EditorUtility.DisplayDialog("Export Failed", $"Failed to export PSF:\n{e.Message}", "OK");
                }
            }
        }
        
        private void ValidateAsset(PSFAsset psfAsset)
        {
            if (psfAsset.Validate())
            {
                EditorUtility.DisplayDialog("Validation", "PSF asset is valid!", "OK");
            }
            else
            {
                EditorUtility.DisplayDialog("Validation", "PSF asset has validation errors. Check console for details.", "OK");
            }
        }
        
        private void ReimportAsset(PSFAsset psfAsset)
        {
            string assetPath = AssetDatabase.GetAssetPath(psfAsset);
            AssetDatabase.ImportAsset(assetPath, ImportAssetOptions.ForceUpdate);
            
            Debug.Log($"Reimported PSF asset: {assetPath}");
        }
        
        private void PrintCollectionSummary(PSFAsset psfAsset)
        {
            var collection = psfAsset.GetCollection();
            if (collection != null)
            {
                string summary = PSFLoader.GetCollectionSummary(collection);
                Debug.Log(summary);
            }
        }
        
        private void AnalyzeColorEquations(PSFAsset psfAsset)
        {
            var collection = psfAsset.GetCollection();
            if (collection == null) return;
            
            Debug.Log($"=== COLOR ANALYSIS: {psfAsset.name} ===");
            
            for (int i = 0; i < Mathf.Min(collection.surfaces.Length, 5); i++)
            {
                var surface = collection.surfaces[i];
                Debug.Log($"Surface {i}:");
                Debug.Log($"  R: Equation {surface.color_eqn_r}, Param {surface.color_param_r}");
                Debug.Log($"  G: Equation {surface.color_eqn_g}, Param {surface.color_param_g}");
                Debug.Log($"  B: Equation {surface.color_eqn_b}, Param {surface.color_param_b}");
                
                // Test color evaluation
                float r = surface.color_param_r / 255f;
                float g = surface.color_param_g / 255f;
                float b = surface.color_param_b / 255f;
                Debug.Log($"  Sample color: ({r:F3}, {g:F3}, {b:F3})");
            }
        }
        
        private string GetColorEquationName(byte equation)
        {
            return equation switch
            {
                0 => "Constant",
                1 => "Linear U",
                2 => "Linear V", 
                3 => "Bilinear",
                4 => "Sine Wave",
                5 => "Cosine Wave",
                6 => "Quadratic U",
                7 => "Quadratic V",
                _ => $"Unknown ({equation})"
            };
        }
    }
    
    /// <summary>
    /// Simple OBJ exporter for PSF meshes
    /// </summary>
    public static class PSFOBJExporter
    {
        public static void Export(Mesh mesh, string filePath)
        {
            using (StreamWriter writer = new StreamWriter(filePath))
            {
                writer.WriteLine($"# PSF Mesh Export - {System.DateTime.Now}");
                writer.WriteLine($"# Vertices: {mesh.vertexCount}");
                writer.WriteLine($"# Triangles: {mesh.triangles.Length / 3}");
                writer.WriteLine();
                
                // Write vertices
                foreach (Vector3 vertex in mesh.vertices)
                {
                    writer.WriteLine($"v {vertex.x:F6} {vertex.y:F6} {vertex.z:F6}");
                }
                
                // Write normals
                if (mesh.normals != null && mesh.normals.Length > 0)
                {
                    foreach (Vector3 normal in mesh.normals)
                    {
                        writer.WriteLine($"vn {normal.x:F6} {normal.y:F6} {normal.z:F6}");
                    }
                }
                
                // Write UVs
                if (mesh.uv != null && mesh.uv.Length > 0)
                {
                    foreach (Vector2 uv in mesh.uv)
                    {
                        writer.WriteLine($"vt {uv.x:F6} {uv.y:F6}");
                    }
                }
                
                // Write faces
                for (int i = 0; i < mesh.triangles.Length; i += 3)
                {
                    int v1 = mesh.triangles[i] + 1;     // OBJ indices start at 1
                    int v2 = mesh.triangles[i + 1] + 1;
                    int v3 = mesh.triangles[i + 2] + 1;
                    
                    if (mesh.normals != null && mesh.normals.Length > 0)
                    {
                        writer.WriteLine($"f {v1}//{v1} {v2}//{v2} {v3}//{v3}");
                    }
                    else
                    {
                        writer.WriteLine($"f {v1} {v2} {v3}");
                    }
                }
            }
        }
    }
}

/*
USAGE INSTRUCTIONS:

1. Save this file as: Assets/Scripts/PSF/Editor/PSFAssetEditor.cs

2. This custom inspector automatically appears when you select any PSFAsset in the Project window

3. Features provided:
   - Asset status and validation
   - Detailed geometry statistics
   - Color equation analysis
   - Performance metrics and recommendations
   - Interactive 3D preview (drag to rotate)
   - Utility functions (create GameObject, export OBJ, validate, reimport)
   - Debug operations for development

4. The inspector integrates with all other PSF classes:
   - Uses PSFLoader for data access
   - Creates simple preview meshes internally
   - Creates PSFObjects for GameObjects
   - Provides OBJ export functionality

5. Mouse controls in 3D preview:
   - Drag to rotate camera around object
   - "Reset Camera" button to return to default view
   - "Regenerate Preview Mesh" to update preview

6. Changes made:
   - Fixed variable scope conflict in DrawColorAnalysisSection
   - Replaced GetValueOrDefault with explicit dictionary checks for compatibility
   - Removed dependency on PSFMeshGenerator (created simple mesh generation internally)
   - Added DecodeAnchorPosition method for preview mesh generation
*/