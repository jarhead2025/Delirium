// PSF GPU Rendering Shader
// Renders GPU-computed PSF points with full procedural effects
// Place this file at: Assets/Resources/PSF/Shaders/PSFGPURendering.shader

Shader "PSF/GPU Triangulated Mesh"
{
    Properties
    {
        _AnimationTime ("Animation Time", Range(0, 1)) = 0
        _LODLevel ("LOD Level", Int) = 0
        _Metallic ("Metallic", Range(0, 1)) = 0
        _Roughness ("Roughness", Range(0, 1)) = 0.5
        _EmissionStrength ("Emission Strength", Range(0, 2)) = 0
        _NormalStrength ("Normal Strength", Range(0, 2)) = 1
    }

    SubShader
    {
        Tags 
        { 
            "RenderType"="Opaque" 
            "Queue"="Geometry"
        }
        
        Pass
        {
            Name "PSFTriangulated"
            Tags { "LightMode"="ForwardBase" }
            
            Cull Back
            ZWrite On
            ZTest LEqual
            
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma target 5.0
            #pragma multi_compile_fwdbase
            
            #include "UnityCG.cginc"
            #include "AutoLight.cginc"
            #include "Lighting.cginc"

            // GPU buffers containing triangulated mesh data
            StructuredBuffer<float3> _VertexBuffer;
            StructuredBuffer<float3> _NormalBuffer;
            StructuredBuffer<float4> _ColorBuffer;
            StructuredBuffer<float2> _UVBuffer;
            StructuredBuffer<uint> _IndexBuffer;
            
            // Material properties
            float _AnimationTime;
            int _LODLevel;
            int _VertexCount;
            int _TriangleCount;
            float _Metallic;
            float _Roughness;
            float _EmissionStrength;
            float _NormalStrength;
            
            struct appdata
            {
                uint vertexID : SV_VertexID;
            };

            struct v2f
            {
                float4 pos : SV_POSITION;
                float3 worldPos : TEXCOORD0;
                float3 normal : TEXCOORD1;
                float4 color : TEXCOORD2;
                float2 uv : TEXCOORD3;
                float3 viewDir : TEXCOORD4;
                LIGHTING_COORDS(5,6)
            };

            v2f vert(appdata v)
            {
                v2f o;
                
                // Get triangle index from vertex ID
                uint triangleIndex = v.vertexID / 3;
                uint vertexInTriangle = v.vertexID % 3;
                
                // Bounds check
                if (triangleIndex >= (uint)_TriangleCount)
                {
                    o.pos = float4(0, 0, 0, 0);
                    return o;
                }
                
                // Get actual vertex index from index buffer
                uint actualVertexIndex = _IndexBuffer[triangleIndex * 3 + vertexInTriangle];
                
                if (actualVertexIndex >= (uint)_VertexCount)
                {
                    o.pos = float4(0, 0, 0, 0);
                    return o;
                }
                
                // Read mesh data from GPU buffers
                float3 worldPos = _VertexBuffer[actualVertexIndex];
                float3 worldNormal = normalize(_NormalBuffer[actualVertexIndex]);
                float4 color = _ColorBuffer[actualVertexIndex];
                float2 uv = _UVBuffer[actualVertexIndex];
                
                // Set output data
                o.worldPos = worldPos;
                o.normal = worldNormal;
                o.color = color;
                o.uv = uv;
                o.viewDir = normalize(_WorldSpaceCameraPos - worldPos);
                
                // Transform to clip space
                o.pos = mul(UNITY_MATRIX_VP, float4(worldPos, 1.0));
                
                // Calculate lighting
                TRANSFER_VERTEX_TO_FRAGMENT(o);
                
                return o;
            }

            // Enhanced lighting for triangulated mesh
            float3 CalculateTriangleLighting(float3 worldPos, float3 normal, float4 albedo)
            {
                // Main directional light
                float3 lightDir = normalize(_WorldSpaceLightPos0.xyz);
                float3 viewDir = normalize(_WorldSpaceCameraPos - worldPos);
                float3 halfDir = normalize(lightDir + viewDir);
                
                // Diffuse lighting (Lambert)
                float NdotL = max(0, dot(normal, lightDir));
                float3 diffuse = albedo.rgb * _LightColor0.rgb * NdotL;
                
                // Specular lighting (Blinn-Phong)
                float NdotH = max(0, dot(normal, halfDir));
                float specularPower = exp2(10.0 * (1.0 - _Roughness) + 1.0);
                float3 specularColor = lerp(float3(0.04, 0.04, 0.04), albedo.rgb, _Metallic);
                float3 specular = specularColor * _LightColor0.rgb * pow(NdotH, specularPower) * NdotL;
                
                // Ambient lighting
                float3 ambient = ShadeSH9(float4(normal, 1.0)) * albedo.rgb * 0.3;
                
                return ambient + diffuse + specular;
            }

            // Procedural effects for triangulated surface
            float4 ApplyProceduralEffects(float4 baseColor, float3 worldPos, float3 normal, float2 uv)
            {
                // Animation-based color modulation
                float timeEffect = sin(_AnimationTime * 6.28318 + worldPos.x * 2.0 + worldPos.z * 1.5) * 0.1 + 0.9;
                baseColor.rgb *= timeEffect;
                
                // UV-based effects
                float uvEffect = sin(uv.x * 10.0) * sin(uv.y * 10.0) * 0.05 + 0.95;
                baseColor.rgb *= uvEffect;
                
                // Normal-based rim lighting
                float3 viewDir = normalize(_WorldSpaceCameraPos - worldPos);
                float rim = 1.0 - saturate(dot(normal, viewDir));
                rim = pow(rim, 2.0);
                baseColor.rgb += rim * float3(0.2, 0.4, 0.8) * 0.3;
                
                // Height-based color variation
                float heightEffect = sin(worldPos.y * 5.0) * 0.1 + 0.9;
                baseColor.rgb *= heightEffect;
                
                return baseColor;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                // Base color from GPU computation
                float4 baseColor = i.color;
                
                // Apply procedural effects
                baseColor = ApplyProceduralEffects(baseColor, i.worldPos, i.normal, i.uv);
                
                // Calculate lighting for triangulated surface
                float3 litColor = CalculateTriangleLighting(i.worldPos, i.normal, baseColor);
                
                // Add emission
                litColor += baseColor.rgb * _EmissionStrength;
                
                // Apply normal strength
                float3 enhancedNormal = normalize(lerp(float3(0, 1, 0), i.normal, _NormalStrength));
                float normalEffect = dot(enhancedNormal, normalize(_WorldSpaceLightPos0.xyz)) * 0.2 + 0.8;
                litColor *= normalEffect;
                
                // Final result
                float4 result = float4(litColor, baseColor.a);
                
                // Apply attenuation
                fixed atten = LIGHT_ATTENUATION(i);
                result.rgb *= atten;
                
                return result;
            }
            ENDCG
        }
        
        // Shadow casting pass for triangulated mesh
        Pass
        {
            Name "ShadowCaster"
            Tags { "LightMode"="ShadowCaster" }
            
            ZWrite On
            ZTest LEqual
            ColorMask 0
            Cull Back
            
            CGPROGRAM
            #pragma vertex vertShadow
            #pragma fragment fragShadow
            #pragma target 5.0
            
            #include "UnityCG.cginc"
            
            StructuredBuffer<float3> _VertexBuffer;
            StructuredBuffer<uint> _IndexBuffer;
            int _VertexCount;
            int _TriangleCount;
            
            struct appdata
            {
                uint vertexID : SV_VertexID;
            };

            struct v2f
            {
                V2F_SHADOW_CASTER;
            };

            v2f vertShadow(appdata v)
            {
                v2f o;
                
                uint triangleIndex = v.vertexID / 3;
                uint vertexInTriangle = v.vertexID % 3;
                
                if (triangleIndex >= (uint)_TriangleCount)
                {
                    o.pos = float4(0, 0, 0, 0);
                    return o;
                }
                
                uint actualVertexIndex = _IndexBuffer[triangleIndex * 3 + vertexInTriangle];
                
                if (actualVertexIndex >= (uint)_VertexCount)
                {
                    o.pos = float4(0, 0, 0, 0);
                    return o;
                }
                
                float3 worldPos = _VertexBuffer[actualVertexIndex];
                
                TRANSFER_SHADOW_CASTER_NORMALOFFSET(o)
                return o;
            }

            float4 fragShadow(v2f i) : SV_Target
            {
                SHADOW_CASTER_FRAGMENT(i)
            }
            ENDCG
        }
    }
    
    Fallback "Diffuse"
}
{
    Properties
    {
        _PointSize ("Point Size", Range(0.001, 0.1)) = 0.01
        _AnimationTime ("Animation Time", Range(0, 1)) = 0
        _LODLevel ("LOD Level", Int) = 0
        _Metallic ("Metallic", Range(0, 1)) = 0
        _Roughness ("Roughness", Range(0, 1)) = 0.5
        _EmissionStrength ("Emission Strength", Range(0, 2)) = 0
        _NormalStrength ("Normal Strength", Range(0, 2)) = 1
    }

    SubShader
    {
        Tags 
        { 
            "RenderType"="Opaque" 
            "Queue"="Geometry"
        }
        
        Pass
        {
            Name "PSFForward"
            Tags { "LightMode"="ForwardBase" }
            
            Blend SrcAlpha OneMinusSrcAlpha
            ZWrite On
            ZTest LEqual
            Cull Off
            
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma target 5.0
            #pragma multi_compile_fwdbase
            
            #include "UnityCG.cginc"
            #include "AutoLight.cginc"
            #include "Lighting.cginc"

            // GPU buffers containing computed PSF data
            StructuredBuffer<float3> _PointBuffer;
            StructuredBuffer<float4> _ColorBuffer;
            StructuredBuffer<float3> _NormalBuffer;
            
            // Material properties
            float _PointSize;
            float _AnimationTime;
            int _LODLevel;
            int _PointCount;
            float _Metallic;
            float _Roughness;
            float _EmissionStrength;
            float _NormalStrength;
            float4x4 _ObjectToWorld;
            
            struct appdata
            {
                uint vertexID : SV_VertexID;
            };

            struct v2f
            {
                float4 pos : SV_POSITION;
                float3 worldPos : TEXCOORD0;
                float3 normal : TEXCOORD1;
                float4 color : TEXCOORD2;
                float2 uv : TEXCOORD3;
                float pointSize : PSIZE;
                float3 viewDir : TEXCOORD4;
                LIGHTING_COORDS(5,6)
            };

            v2f vert(appdata v)
            {
                v2f o;
                
                // Get point index from vertex ID
                uint pointIndex = v.vertexID;
                
                // Bounds check
                if (pointIndex >= (uint)_PointCount)
                {
                    // Output degenerate vertex
                    o.pos = float4(0, 0, 0, 0);
                    return o;
                }
                
                // Read computed data from GPU buffers
                float3 worldPos = _PointBuffer[pointIndex];
                float4 color = _ColorBuffer[pointIndex];
                float3 worldNormal = normalize(_NormalBuffer[pointIndex]);
                
                // Set output data
                o.worldPos = worldPos;
                o.normal = worldNormal;
                o.color = color;
                o.viewDir = normalize(_WorldSpaceCameraPos - worldPos);
                
                // Transform to clip space
                o.pos = mul(UNITY_MATRIX_VP, float4(worldPos, 1.0));
                
                // Calculate point size based on distance and LOD
                float distance = length(_WorldSpaceCameraPos - worldPos);
                float lodScale = 1.0 / (1.0 + (float)_LODLevel * 0.5);
                o.pointSize = _PointSize * lodScale * (10.0 / max(distance, 1.0));
                
                // Generate UV coordinates for point sprite
                o.uv = float2(0.5, 0.5); // Center of point
                
                // Calculate lighting
                TRANSFER_VERTEX_TO_FRAGMENT(o);
                
                return o;
            }

            // Point sprite generation
            float4 GeneratePointSprite(float2 uv, float pointSize)
            {
                // Convert UV to circle coordinates
                float2 centeredUV = (uv - 0.5) * 2.0;
                float radius = length(centeredUV);
                
                // Anti-aliased circle
                float circle = 1.0 - smoothstep(0.8, 1.0, radius);
                
                // Add some visual interest
                float innerGlow = 1.0 - smoothstep(0.0, 0.5, radius);
                float outerRim = smoothstep(0.7, 0.9, radius) * (1.0 - smoothstep(0.9, 1.0, radius));
                
                float alpha = circle * (innerGlow * 0.8 + outerRim * 0.2);
                
                return float4(1, 1, 1, alpha);
            }

            // Procedural effects
            float4 ApplyProceduralEffects(float4 baseColor, float3 worldPos, float3 normal, float2 uv)
            {
                // Animation-based color modulation
                float timeEffect = sin(_AnimationTime * 6.28318 + worldPos.x * 2.0 + worldPos.z * 1.5) * 0.1 + 0.9;
                baseColor.rgb *= timeEffect;
                
                // Position-based color variation
                float positionEffect = sin(worldPos.y * 3.0) * 0.1 + 0.9;
                baseColor.rgb *= positionEffect;
                
                // Normal-based rim lighting
                float rim = 1.0 - saturate(dot(normal, normalize(_WorldSpaceCameraPos - worldPos)));
                rim = pow(rim, 2.0);
                baseColor.rgb += rim * float3(0.2, 0.4, 0.8) * 0.5;
                
                // LOD-based effects
                float lodEffect = 1.0 - (float)_LODLevel * 0.1;
                baseColor.rgb *= lodEffect;
                
                return baseColor;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                // Generate point sprite
                float4 spriteAlpha = GeneratePointSprite(i.uv, i.pointSize);
                
                // Base color from GPU computation
                float4 baseColor = i.color;
                
                // Apply procedural effects
                baseColor = ApplyProceduralEffects(baseColor, i.worldPos, i.normal, i.uv);
                
                // Simple lighting
                float3 lightDir = normalize(_WorldSpaceLightPos0.xyz);
                float NdotL = max(0, dot(i.normal, lightDir));
                float3 diffuse = baseColor.rgb * _LightColor0.rgb * NdotL;
                
                // Add ambient
                float3 ambient = ShadeSH9(float4(i.normal, 1.0)) * baseColor.rgb;
                
                // Combine lighting
                float3 finalColor = ambient + diffuse;
                
                // Add emission
                finalColor += baseColor.rgb * _EmissionStrength;
                
                // Apply sprite alpha
                float4 result = float4(finalColor, baseColor.a * spriteAlpha.a);
                
                // Apply attenuation
                fixed atten = LIGHT_ATTENUATION(i);
                result.rgb *= atten;
                
                return result;
            }
            ENDCG
        }
    }
    
    Fallback "Diffuse"
} vertShadow(AttributesShadow input)
            {
                VaryingsShadow output = (VaryingsShadow)0;
                
                UNITY_SETUP_INSTANCE_ID(input);
                UNITY_TRANSFER_INSTANCE_ID(input, output);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(output);
                
                uint pointIndex = input.vertexID;
                
                if (pointIndex >= (uint)_PointCount)
                {
                    output.positionCS = float4(0, 0, 0, 0);
                    return output;
                }
                
                float3 worldPos = _PointBuffer[pointIndex];
                output.positionCS = TransformWorldToHClip(ApplyShadowBias(worldPos, float3(0,1,0), _MainLightPosition.xyz));
                
                return output;
            }

            float4 fragShadow(VaryingsShadow input) : SV_Target
            {
                UNITY_SETUP_INSTANCE_ID(input);
                UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
                return 0;
            }
            ENDHLSL
        }
        
        // Depth-only pass
        Pass
        {
            Name "DepthOnly"
            Tags { "LightMode"="DepthOnly" }
            
            ZWrite On
            ZTest LEqual
            ColorMask 0
            Cull Off
            
            HLSLPROGRAM
            #pragma vertex vertDepth
            #pragma fragment fragDepth
            #pragma target 4.5
            #pragma multi_compile_instancing
            
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            
            StructuredBuffer<float3> _PointBuffer;
            int _PointCount;
            
            struct AttributesDepth
            {
                uint vertexID : SV_VertexID;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct VaryingsDepth
            {
                float4 positionCS : SV_POSITION;
                UNITY_VERTEX_INPUT_INSTANCE_ID
                UNITY_VERTEX_OUTPUT_STEREO
            };

            VaryingsDepth vertDepth(AttributesDepth input)
            {
                VaryingsDepth output = (VaryingsDepth)0;
                
                UNITY_SETUP_INSTANCE_ID(input);
                UNITY_TRANSFER_INSTANCE_ID(input, output);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(output);
                
                uint pointIndex = input.vertexID;
                
                if (pointIndex >= (uint)_PointCount)
                {
                    output.positionCS = float4(0, 0, 0, 0);
                    return output;
                }
                
                float3 worldPos = _PointBuffer[pointIndex];
                output.positionCS = TransformWorldToHClip(worldPos);
                
                return output;
            }

            float4 fragDepth(VaryingsDepth input) : SV_Target
            {
                UNITY_SETUP_INSTANCE_ID(input);
                UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
                return 0;
            }
            ENDHLSL
        }
    }
    
    Fallback "Hidden/Universal Render Pipeline/FallbackError"
}