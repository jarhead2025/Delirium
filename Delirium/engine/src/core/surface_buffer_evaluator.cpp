#include "surface_buffer_evaluator.h"

#include "object_descriptor_buffers.h"

#include <cmath>

#include <vector>

#include <cstring>

#include <algorithm>

#include <numeric>

#include "anchorpoint_evaluator.h"

#include <iostream> 



#ifndef M_PI

#define M_PI 3.14159265358979323846

#endif



// =================================================================================

// STATIC HELPER FUNCTIONS

// =================================================================================



static Vec3 DecodeAnchorPosition(const uint16_t halfFloatPos[3]) {

    return Vec3{

        (static_cast<float>(halfFloatPos[0]) / 1024.0f) - 2.0f,

        (static_cast<float>(halfFloatPos[1]) / 1024.0f) - 2.0f,

        (static_cast<float>(halfFloatPos[2]) / 1024.0f) - 2.0f

    };

}



static Vec3 DecodeAnchorNormal(const uint8_t compressedNormal[3]) {

    Vec3 norm = Vec3{

        (static_cast<float>(compressedNormal[0]) / 127.5f) - 1.0f,

        (static_cast<float>(compressedNormal[1]) / 127.5f) - 1.0f,

        (static_cast<float>(compressedNormal[2]) / 127.5f) - 1.0f

    };

    

    NormalizeVector(norm);

    return norm;

}



static bool IsPrimitiveEquation(SurfaceEquationType type) {

    return (type >= EQUATION_PLANE && type <= EQUATION_HYPERBOLOID);

}



// =================================================================================

// MINIMAL MATH & NOISE LIBRARY

// =================================================================================



float PerlinNoise2D(float x, float y) {

    return (sin(x * 12.9898f + y * 78.233f) * 43758.5453f) - floor(sin(x * 12.9898f + y * 78.233f) * 43758.5453f);

}



float PerlinNoise3D(float x, float y, float z) {

    float xy = PerlinNoise2D(x, y);

    float yz = PerlinNoise2D(y, z);

    float xz = PerlinNoise2D(x, z);

    return (xy + yz + xz) / 3.0f;

}



float SmoothMin(float a, float b, float k) {

    float h = std::max(k - std::abs(a - b), 0.0f) / k;

    return std::min(a, b) - h * h * h * k * (1.0f / 6.0f);

}



// =================================================================================

// OUTWARD CURVATURE EVALUATOR

// =================================================================================



void SurfaceBufferEvaluator::EvaluateOutwardCurvature(

    SurfaceEquationType type,

    const float* params,

    float u, float v,

    float& outDisplacement)

{

    // Boundary mask that ensures zero displacement at all edges

    float boundaryMask = u * (1.0f - u) * v * (1.0f - v) * 16.0f;  // Peak = 1.0 at center

    

    float displacement = 0.0f;

    

    switch (type) {

        case EQUATION_BEZIER_PATCH: {

            // =================================================================================

            // SMOOTH BEZIER BULGE

            // =================================================================================

            float centerBulge = params[0];

            float uBias = params[1];

            float vBias = params[2];

            

            float centerU = u - 0.5f;

            float centerV = v - 0.5f;

            float distFromCenter = sqrt(centerU * centerU + centerV * centerV);

            

            float bulgeFactor = 1.0f - (distFromCenter * 2.0f);

            bulgeFactor = std::max(0.0f, bulgeFactor);

            

            // Add directional bias

            float directionalBias = 1.0f + uBias * centerU + vBias * centerV;

            

            displacement = centerBulge * boundaryMask * bulgeFactor * directionalBias;

            break;

        }

        

        case EQUATION_BIQUADRATIC: {

            // =================================================================================

            // QUADRATIC DOME - Smooth parabolic bulge

            // =================================================================================

            float height = params[0];

            float uCurvature = params[1];  // U-direction curvature strength

            float vCurvature = params[2];  // V-direction curvature strength

            

            // Quadratic functions that peak at center and go to zero at edges

            float uFactor = 4.0f * u * (1.0f - u);  // Peaks at u=0.5

            float vFactor = 4.0f * v * (1.0f - v);  // Peaks at v=0.5

            

            // Combine with curvature parameters

            float quadraticBulge = uCurvature * uFactor + vCurvature * vFactor + height * uFactor * vFactor;

            

            displacement = quadraticBulge * boundaryMask;

            break;

        }

        

        case EQUATION_BICUBIC: {

            // =================================================================================

            // CUBIC DOME - Sharper, more dramatic bulge

            // =================================================================================

            float height = params[0];

            float sharpness = params[1];

            

            // Cubic function for sharper peaks

            float centerU = (u - 0.5f) * 2.0f;  // -1 to +1

            float centerV = (v - 0.5f) * 2.0f;  // -1 to +1

            

            // Cubic bulge that's flatter near edges, sharper at center

            float cubicFactor = 1.0f - pow(centerU * centerU + centerV * centerV, sharpness);

            cubicFactor = std::max(0.0f, cubicFactor);

            

            displacement = height * boundaryMask * cubicFactor;

            break;

        }

        

        case EQUATION_SPHERE: {

            // =================================================================================

            // SPHERICAL BULGE - Perfect spherical cap

            // =================================================================================

            float radius = params[0];

            float height = params[1];

            

            // Calculate distance from center

            float centerU = (u - 0.5f) * 2.0f;

            float centerV = (v - 0.5f) * 2.0f;

            float distFromCenter = sqrt(centerU * centerU + centerV * centerV);

            

            // Spherical height calculation

            float sphericalHeight = 0.0f;

            if (distFromCenter < 1.0f) {  // Within the sphere

                sphericalHeight = sqrt(radius * radius - distFromCenter * distFromCenter * radius * radius) - radius + height;

                sphericalHeight = std::max(0.0f, sphericalHeight);

            }

            

            displacement = sphericalHeight * boundaryMask;

            break;

        }

        

        case EQUATION_TORUS: {

            // =================================================================================

            // TORUS BULGE - Donut-shaped with hole in middle

            // =================================================================================

            float majorRadius = params[0];  // Distance from center to tube center

            float minorRadius = params[1];  // Tube thickness

            float height = params[2];

            

            float centerU = (u - 0.5f) * 2.0f;

            float centerV = (v - 0.5f) * 2.0f;

            float distFromCenter = sqrt(centerU * centerU + centerV * centerV);

            

            // Torus equation: bulge around a ring

            float ringDist = fabs(distFromCenter - majorRadius);

            float torusHeight = 0.0f;

            

            if (ringDist < minorRadius) {

                torusHeight = sqrt(minorRadius * minorRadius - ringDist * ringDist) * height;

            }

            

            displacement = torusHeight * boundaryMask;

            break;

        }

        

        case EQUATION_CONE: {

            // =================================================================================

            // CONICAL BULGE - Pyramid-like peak

            // =================================================================================

            float height = params[0];

            float peakU = params[1];  // U position of peak (0-1)

            float peakV = params[2];  // V position of peak (0-1)

            

            // Distance from peak point

            float distToPeak = sqrt((u - peakU) * (u - peakU) + (v - peakV) * (v - peakV));

            

            // Linear falloff from peak

            float maxDist = sqrt(2.0f);  // Maximum possible distance in unit square

            float conicalHeight = height * (1.0f - distToPeak / maxDist);

            conicalHeight = std::max(0.0f, conicalHeight);

            

            displacement = conicalHeight * boundaryMask;

            break;

        }

        

        case EQUATION_WAVE: {

            // =================================================================================

            // WAVE BULGE - Sinusoidal ripples

            // =================================================================================

            float amplitude = params[0];

            float frequencyU = params[1];

            float frequencyV = params[2];

            float phase = params[3];

            

            // Sinusoidal waves

            float waveU = sin(u * frequencyU * 2.0f * M_PI + phase);

            float waveV = sin(v * frequencyV * 2.0f * M_PI + phase);

            

            // Combine waves

            float waveHeight = amplitude * (waveU + waveV) * 0.5f;

            

            displacement = waveHeight * boundaryMask;

            break;

        }

        

        case EQUATION_PERLIN_NOISE: {

            // =================================================================================

            // ORGANIC BULGE - Natural, irregular surface

            // =================================================================================

            float amplitude = params[0];

            float scale = params[1];

            float octaves = params[2];

            

            // Simple noise approximation

            float noise = 0.0f;

            float freq = scale;

            float amp = amplitude;

            

            for (int i = 0; i < (int)octaves && i < 4; ++i) {

                noise += sin(u * freq * 13.7f + v * freq * 7.3f) * 

                        cos(u * freq * 11.1f - v * freq * 5.7f) * amp;

                freq *= 2.0f;

                amp *= 0.5f;

            }

            

            displacement = noise * boundaryMask;

            break;

        }

        

        default:

            displacement = 0.0f;

            break;

    }

    

    outDisplacement = displacement;

}





void SurfaceBufferEvaluator::EvaluateAnchorPointFull(

    const AnchorPointBuffer& anchor,

    const std::vector<Matrix4x4>& boneTransforms,

    float currentTime,

    Vec3& outPosition,

    Vec3& outNormal,

    Vec3& outTangent,

    Vec3& outColor)

{

    // Get bone transform

    const Matrix4x4& boneMatrix = (anchor.bone_index < boneTransforms.size())

                                  ? boneTransforms[anchor.bone_index]

                                  : Matrix4x4{};



    // Base position from buffer

    Vec3 localPos = DecodeAnchorPosition(anchor.pos_xyz);

    

    // =================================================================================

    // USE DELTA_XYZ FOR DEFORMATION/ANIMATION

    // =================================================================================

    Vec3 deltaPos = {

        static_cast<float>(anchor.delta_xyz[0]) / 1024.0f,

        static_cast<float>(anchor.delta_xyz[1]) / 1024.0f,

        static_cast<float>(anchor.delta_xyz[2]) / 1024.0f

    };

    

    // Apply time-based animation to delta (if anchor has animation weight)

    if (anchor.anim_weight > 0) {

        float animFactor = static_cast<float>(anchor.anim_weight) / 255.0f;

        float timePhase = currentTime * 2.0f * M_PI;

        

        // Simple sinusoidal animation

        deltaPos.x += sin(timePhase) * animFactor * 0.1f;

        deltaPos.y += cos(timePhase * 1.3f) * animFactor * 0.1f;

        deltaPos.z += sin(timePhase * 0.7f) * animFactor * 0.1f;

    }

    

    // Combine base position with delta

    Vec3 finalLocalPos = {

        localPos.x + deltaPos.x,

        localPos.y + deltaPos.y,

        localPos.z + deltaPos.z

    };



    // Transform to world space

    outPosition = Transform(finalLocalPos, boneMatrix, false);



    // =================================================================================

    // DECODE NORMAL AND TANGENT

    // =================================================================================

    Vec3 localNorm = DecodeAnchorNormal(anchor.normal);

    outNormal = Transform(localNorm, boneMatrix, true);

    

    // Decode tangent from 2-component compressed format

    // This is a simplified tangent reconstruction

    Vec3 localTangent;

    if (anchor.tangent[0] != 0 || anchor.tangent[1] != 0) {

        float tx = (static_cast<float>(anchor.tangent[0]) / 127.5f) - 1.0f;

        float ty = (static_cast<float>(anchor.tangent[1]) / 127.5f) - 1.0f;

        float tz = sqrt(std::max(0.0f, 1.0f - tx*tx - ty*ty)); // Reconstruct Z component

        

        localTangent = {tx, ty, tz};

    } else {

        // Generate tangent from normal if not stored

        localTangent = (fabs(localNorm.x) < 0.9f) ? 

            Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};

        Vec3 temp = Cross(localNorm, localTangent);

        localTangent = Cross(temp, localNorm);

    }

    NormalizeVector(localTangent);

    outTangent = Transform(localTangent, boneMatrix, true);



    // =================================================================================

    // DECODE ANCHOR COLOR

    // =================================================================================

    outColor = {

        static_cast<float>(anchor.color[0]) / 255.0f,

        static_cast<float>(anchor.color[1]) / 255.0f,

        static_cast<float>(anchor.color[2]) / 255.0f

    };

}







void SurfaceBufferEvaluator::EvaluateSurfaceEnhanced(

    const ProceduralSurfaceBuffer* buffer,

    const std::vector<AnchorPointBuffer>& anchorPool,

    const std::vector<Matrix4x4>& boneTransforms,

    float u, float v,

    float* outPosition,

    float* outNormal,

    float* outColor,

    float* outMaterial, // [roughness, metallic, emissive, specular]

    float currentTime)

{

    if (!buffer || !outPosition) return;



    uint16_t defIndex = buffer->surface_definition_index;

    SurfaceEquationType primaryType = ExtractBaseType(defIndex);

    CombinationOperation combo = ExtractCombination(defIndex);



    // =================================================================================

    // GET ENHANCED ANCHOR DATA

    // =================================================================================

    

    Vec3 anchors[4], anchorNormals[4], anchorTangents[4], anchorColors[4];

    

    for (int i = 0; i < 4; ++i) {

        uint16_t anchorIdx = buffer->anchor_point_indices[i];

        if (anchorIdx >= anchorPool.size()) { 

            anchors[i] = {0,0,0}; 

            anchorNormals[i] = {0,0,1};

            anchorTangents[i] = {1,0,0};

            anchorColors[i] = {1,1,1};

            continue; 

        }

        

        EvaluateAnchorPointFull(anchorPool[anchorIdx], boneTransforms, currentTime,

                               anchors[i], anchorNormals[i], anchorTangents[i], anchorColors[i]);

    }



    // =================================================================================

    // SURFACE EVALUATION (same as before)

    // =================================================================================

    

    Vec3 finalPos, finalNorm;

    

    if (primaryType == EQUATION_BILINEAR) {

        // Bilinear interpolation

        float u1 = 1.0f - u;

        float v1 = 1.0f - v;

        

        finalPos.x = u1*v1*anchors[0].x + u*v1*anchors[1].x + u*v*anchors[2].x + u1*v*anchors[3].x;

        finalPos.y = u1*v1*anchors[0].y + u*v1*anchors[1].y + u*v*anchors[2].y + u1*v*anchors[3].y;

        finalPos.z = u1*v1*anchors[0].z + u*v1*anchors[1].z + u*v*anchors[2].z + u1*v*anchors[3].z;

        

        finalNorm.x = u1*v1*anchorNormals[0].x + u*v1*anchorNormals[1].x + u*v*anchorNormals[2].x + u1*v*anchorNormals[3].x;

        finalNorm.y = u1*v1*anchorNormals[0].y + u*v1*anchorNormals[1].y + u*v*anchorNormals[2].y + u1*v*anchorNormals[3].y;

        finalNorm.z = u1*v1*anchorNormals[0].z + u*v1*anchorNormals[1].z + u*v*anchorNormals[2].z + u1*v*anchorNormals[3].z;

        

    } else {

        // Curved surface evaluation (same algorithm as before)

        float u1 = 1.0f - u;

        float v1 = 1.0f - v;

        

        Vec3 basePos;

        basePos.x = u1*v1*anchors[0].x + u*v1*anchors[1].x + u*v*anchors[2].x + u1*v*anchors[3].x;

        basePos.y = u1*v1*anchors[0].y + u*v1*anchors[1].y + u*v*anchors[2].y + u1*v*anchors[3].y;

        basePos.z = u1*v1*anchors[0].z + u*v1*anchors[1].z + u*v*anchors[2].z + u1*v*anchors[3].z;

        

        // Calculate surface normal (simplified approach from before)

        Vec3 uVec = { anchors[1].x - anchors[0].x, anchors[1].y - anchors[0].y, anchors[1].z - anchors[0].z };

        Vec3 vVec = { anchors[3].x - anchors[0].x, anchors[3].y - anchors[0].y, anchors[3].z - anchors[0].z };

        Vec3 surfaceNormal = Cross(uVec, vVec);

        NormalizeVector(surfaceNormal);

        

        // Evaluate displacement

        float displacement;

        EvaluateOutwardCurvature(primaryType, buffer->eqn_params, u, v, displacement);

        

        finalPos.x = basePos.x + surfaceNormal.x * displacement;

        finalPos.y = basePos.y + surfaceNormal.y * displacement;

        finalPos.z = basePos.z + surfaceNormal.z * displacement;

        

        finalNorm = surfaceNormal;

    }



    // =================================================================================

    // HANDLE COMBINATIONS (use secondary_surface_type and combination_param)

    // =================================================================================

    

    if (combo != COMBO_NONE && buffer->secondary_surface_type != 0) {

        SurfaceEquationType secondaryType = static_cast<SurfaceEquationType>(buffer->secondary_surface_type);

        float blendWeight = static_cast<float>(buffer->combination_param) / 255.0f;

        

        // Evaluate secondary surface

        float secondaryDisplacement;

        EvaluateOutwardCurvature(secondaryType, &buffer->eqn_params[4], u, v, secondaryDisplacement);

        

        // Simple blend for now (could be more sophisticated)

        Vec3 secondaryPos = {

            finalPos.x + finalNorm.x * secondaryDisplacement * blendWeight,

            finalPos.y + finalNorm.y * secondaryDisplacement * blendWeight,

            finalPos.z + finalNorm.z * secondaryDisplacement * blendWeight

        };

        

        // Blend positions

        finalPos.x = finalPos.x * (1.0f - blendWeight) + secondaryPos.x * blendWeight;

        finalPos.y = finalPos.y * (1.0f - blendWeight) + secondaryPos.y * blendWeight;

        finalPos.z = finalPos.z * (1.0f - blendWeight) + secondaryPos.z * blendWeight;

    }



    // =================================================================================

    // OUTPUT RESULTS

    // =================================================================================

    

    outPosition[0] = finalPos.x;

    outPosition[1] = finalPos.y;

    outPosition[2] = finalPos.z;



    if (outNormal) {

        NormalizeVector(finalNorm);

        outNormal[0] = finalNorm.x;

        outNormal[1] = finalNorm.y;

        outNormal[2] = finalNorm.z;

    }



    if (outColor) {

        // Interpolate anchor colors

        float u1 = 1.0f - u;

        float v1 = 1.0f - v;

        

        outColor[0] = u1*v1*anchorColors[0].x + u*v1*anchorColors[1].x + u*v*anchorColors[2].x + u1*v*anchorColors[3].x;

        outColor[1] = u1*v1*anchorColors[0].y + u*v1*anchorColors[1].y + u*v*anchorColors[2].y + u1*v*anchorColors[3].y;

        outColor[2] = u1*v1*anchorColors[0].z + u*v1*anchorColors[1].z + u*v*anchorColors[2].z + u1*v*anchorColors[3].z;

        

        // Apply procedural color equations from buffer

        if (buffer->color_eqn_r != 0 || buffer->color_eqn_g != 0 || buffer->color_eqn_b != 0) {

            // This would use your mathematical_color_evaluator.h

            // MathematicalColorEvaluator::EvaluateColorFromBuffer(buffer, u, v, proceduralColor);

            // Then blend with anchor colors...

        }

    }



    if (outMaterial) {

        // Extract material properties from buffer

        outMaterial[0] = static_cast<float>(buffer->roughness) / 255.0f;

        outMaterial[1] = static_cast<float>(buffer->metallic) / 255.0f;

        outMaterial[2] = static_cast<float>(buffer->emissive_strength) / 255.0f;

        outMaterial[3] = static_cast<float>(buffer->specular_tint) / 255.0f;

    }

}







void SurfaceBufferEvaluator::EvaluateSurface(

    const ProceduralSurfaceBuffer* buffer,

    const std::vector<AnchorPointBuffer>& anchorPool,

    const std::vector<Matrix4x4>& boneTransforms,

    float u, float v,

    float* outPosition,

    float* outNormal,

    float currentTime)

{

    if (!buffer || !outPosition) {

        return;

    }



    // Extract surface type

    uint16_t defIndex = buffer->surface_definition_index;

    SurfaceEquationType primaryType = ExtractBaseType(defIndex);



    // =================================================================================

    // STEP 1: GET ANCHOR POSITIONS (WORLD SPACE)

    // =================================================================================

    

    Vec3 anchors[4];

    Vec3 anchorNormals[4];

    

    for (int i = 0; i < 4; ++i) {

        uint16_t anchorIdx = buffer->anchor_point_indices[i];

        if (anchorIdx >= anchorPool.size()) { 

            anchors[i] = {0,0,0}; 

            anchorNormals[i] = {0,0,1}; 

            continue; 

        }

        

        const AnchorPointBuffer& anchor = anchorPool[anchorIdx];

        

        // Decode anchor position

        Vec3 localPos = DecodeAnchorPosition(anchor.pos_xyz);

        Vec3 localNorm = DecodeAnchorNormal(anchor.normal);

        

        // Transform to world space

        const Matrix4x4& boneMatrix = (anchor.bone_index < boneTransforms.size())

                                    ? boneTransforms[anchor.bone_index]

                                    : Matrix4x4{};

        

        anchors[i] = Transform(localPos, boneMatrix, false);

        anchorNormals[i] = Transform(localNorm, boneMatrix, true);

    }



    // =================================================================================

    // STEP 2: SURFACE EVALUATION BASED ON TYPE

    // =================================================================================

    

    Vec3 finalPos;

    Vec3 finalNorm;

    

    if (primaryType == EQUATION_BILINEAR) {

        // =================================================================================

        // BILINEAR: Simple interpolation between 4 anchors

        // =================================================================================

        

        // Standard bilinear interpolation

        float u1 = 1.0f - u;

        float v1 = 1.0f - v;

        

        finalPos.x = u1*v1*anchors[0].x + u*v1*anchors[1].x + u*v*anchors[2].x + u1*v*anchors[3].x;

        finalPos.y = u1*v1*anchors[0].y + u*v1*anchors[1].y + u*v*anchors[2].y + u1*v*anchors[3].y;

        finalPos.z = u1*v1*anchors[0].z + u*v1*anchors[1].z + u*v*anchors[2].z + u1*v*anchors[3].z;

        

        finalNorm.x = u1*v1*anchorNormals[0].x + u*v1*anchorNormals[1].x + u*v*anchorNormals[2].x + u1*v*anchorNormals[3].x;

        finalNorm.y = u1*v1*anchorNormals[0].y + u*v1*anchorNormals[1].y + u*v*anchorNormals[2].y + u1*v*anchorNormals[3].y;

        finalNorm.z = u1*v1*anchorNormals[0].z + u*v1*anchorNormals[1].z + u*v*anchorNormals[2].z + u1*v*anchorNormals[3].z;

        

    } else if (primaryType == EQUATION_BEZIER_PATCH || 

               primaryType == EQUATION_BIQUADRATIC ||

               primaryType == EQUATION_BICUBIC ||

               primaryType == EQUATION_SPHERE ||

               primaryType == EQUATION_TORUS ||

               primaryType == EQUATION_CONE ||

               primaryType == EQUATION_WAVE ||

               primaryType == EQUATION_PERLIN_NOISE) {

        

        // =================================================================================

        // CURVED SURFACES: Controlled surface that MUST pass through anchors at boundaries

        // =================================================================================

        

        // First, calculate bilinear base

        float u1 = 1.0f - u;

        float v1 = 1.0f - v;

        

        Vec3 basePos;

        basePos.x = u1*v1*anchors[0].x + u*v1*anchors[1].x + u*v*anchors[2].x + u1*v*anchors[3].x;

        basePos.y = u1*v1*anchors[0].y + u*v1*anchors[1].y + u*v*anchors[2].y + u1*v*anchors[3].y;

        basePos.z = u1*v1*anchors[0].z + u*v1*anchors[1].z + u*v*anchors[2].z + u1*v*anchors[3].z;

        

        // =================================================================================

        // IMPROVED NORMAL CALCULATION - Works for all face orientations

        // =================================================================================

        

        // Calculate surface normal from anchor frame

        Vec3 uVec = { anchors[1].x - anchors[0].x, anchors[1].y - anchors[0].y, anchors[1].z - anchors[0].z };

        Vec3 vVec = { anchors[3].x - anchors[0].x, anchors[3].y - anchors[0].y, anchors[3].z - anchors[0].z };

        

        // Calculate both possible normal directions

        Vec3 normal1 = Cross(uVec, vVec);

        Vec3 normal2 = Cross(vVec, uVec);

        

        // Calculate the center point of the face

        Vec3 faceCenter = {

            (anchors[0].x + anchors[1].x + anchors[2].x + anchors[3].x) * 0.25f,

            (anchors[0].y + anchors[1].y + anchors[2].y + anchors[3].y) * 0.25f,

            (anchors[0].z + anchors[1].z + anchors[2].z + anchors[3].z) * 0.25f

        };

        

        // Determine which normal points "outward" from the cube center

        // Estimate cube center (you might need to adjust this based on your setup)

        Vec3 cubeCenter = {0.0f, 0.0f, 0.0f}; // Adjust if your cube center is elsewhere

        

        Vec3 outwardDirection = {

            faceCenter.x - cubeCenter.x,

            faceCenter.y - cubeCenter.y,

            faceCenter.z - cubeCenter.z

        };

        

        // Choose the normal that points in the same general direction as outward

        Vec3 surfaceNormal;

        if (Dot(normal1, outwardDirection) > Dot(normal2, outwardDirection)) {

            surfaceNormal = normal1;

        } else {

            surfaceNormal = normal2;

        }

        

        NormalizeVector(surfaceNormal);

        

        std::cout << "DEBUG: Face center: (" << faceCenter.x << ", " << faceCenter.y << ", " << faceCenter.z << ")\n";

        std::cout << "DEBUG: Outward direction: (" << outwardDirection.x << ", " << outwardDirection.y << ", " << outwardDirection.z << ")\n";

        std::cout << "DEBUG: Chosen normal: (" << surfaceNormal.x << ", " << surfaceNormal.y << ", " << surfaceNormal.z << ")\n";

        

        // =================================================================================

        // EVALUATE CURVATURE DISPLACEMENT

        // =================================================================================

        

        float displacement;

        EvaluateOutwardCurvature(primaryType, buffer->eqn_params, u, v, displacement);

        

        // Apply displacement along surface normal

        finalPos.x = basePos.x + surfaceNormal.x * displacement;

        finalPos.y = basePos.y + surfaceNormal.y * displacement;

        finalPos.z = basePos.z + surfaceNormal.z * displacement;

        

        finalNorm = surfaceNormal;

        

        // =================================================================================

        // VERIFICATION: Check that we get zero displacement at boundaries

        // =================================================================================

        if (u < 0.01f || u > 0.99f || v < 0.01f || v > 0.99f) {

            std::cout << "BOUNDARY CHECK: u=" << u << ", v=" << v 

                      << ", displacement=" << displacement << "\n";

        }

        

    } else {

        // Default: fallback to bilinear

        float u1 = 1.0f - u;

        float v1 = 1.0f - v;

        

        finalPos.x = u1*v1*anchors[0].x + u*v1*anchors[1].x + u*v*anchors[2].x + u1*v*anchors[3].x;

        finalPos.y = u1*v1*anchors[0].y + u*v1*anchors[1].y + u*v*anchors[2].y + u1*v*anchors[3].y;

        finalPos.z = u1*v1*anchors[0].z + u*v1*anchors[1].z + u*v*anchors[2].z + u1*v*anchors[3].z;

        

        finalNorm.x = u1*v1*anchorNormals[0].x + u*v1*anchorNormals[1].x + u*v*anchorNormals[2].x + u1*v*anchorNormals[3].x;

        finalNorm.y = u1*v1*anchorNormals[0].y + u*v1*anchorNormals[1].y + u*v*anchorNormals[2].y + u1*v*anchorNormals[3].y;

        finalNorm.z = u1*v1*anchorNormals[0].z + u*v1*anchorNormals[1].z + u*v*anchorNormals[2].z + u1*v*anchorNormals[3].z;

    }



    // =================================================================================

    // OUTPUT

    // =================================================================================

    

    outPosition[0] = finalPos.x;

    outPosition[1] = finalPos.y;

    outPosition[2] = finalPos.z;



    if (outNormal) {

        NormalizeVector(finalNorm);

        outNormal[0] = finalNorm.x;

        outNormal[1] = finalNorm.y;

        outNormal[2] = finalNorm.z;

    }

}



// =================================================================================

// MESH GENERATION

// =================================================================================



void SurfaceBufferEvaluator::EvaluateSurfaceMesh(

    const ProceduralSurfaceBuffer* buffer,

    const std::vector<AnchorPointBuffer>& anchorPool,

    const std::vector<Matrix4x4>& boneTransforms,

    int resolution,

    std::vector<float>& outVertices,

    std::vector<uint32_t>& outIndices,

    std::vector<float>& outNormals,

    float currentTime)

{

    if (!buffer) {

        return;

    }

    

    outVertices.clear();

    outNormals.clear();

    outIndices.clear();

    

    // Reserve space for efficiency

    int vertexCount = (resolution + 1) * (resolution + 1);

    outVertices.reserve(vertexCount * 3);

    outNormals.reserve(vertexCount * 3);

    

    // Generate vertices

    for (int v = 0; v <= resolution; ++v) {

        for (int u = 0; u <= resolution; ++u) {

            float uf = static_cast<float>(u) / resolution;

            float vf = static_cast<float>(v) / resolution;

            

            float position[3], normal[3];

            EvaluateSurface(buffer, anchorPool, boneTransforms, uf, vf, position, normal, currentTime);

            

            outVertices.insert(outVertices.end(), {position[0], position[1], position[2]});

            outNormals.insert(outNormals.end(), {normal[0], normal[1], normal[2]});

        }

    }

    

    // Generate indices

    MeshGenerator::GenerateGridIndices(resolution, outIndices);

}



// =================================================================================

// OTHER REQUIRED FUNCTIONS (stubs for now)

// =================================================================================



Vec3 SurfaceBufferEvaluator::CalculateSurfaceNormal(

    const ProceduralSurfaceBuffer* buffer,

    const std::vector<AnchorPointBuffer>& anchorPool,

    const std::vector<Matrix4x4>& boneTransforms,

    float u, float v,

    float currentTime)

{

    const float epsilon = 0.01f;

    

    float pos1[3], pos2[3], pos3[3];

    

    EvaluateSurface(buffer, anchorPool, boneTransforms, u, v, pos1, nullptr, currentTime);

    EvaluateSurface(buffer, anchorPool, boneTransforms, u + epsilon, v, pos2, nullptr, currentTime);

    EvaluateSurface(buffer, anchorPool, boneTransforms, u, v + epsilon, pos3, nullptr, currentTime);

    

    Vec3 du = { pos2[0] - pos1[0], pos2[1] - pos1[1], pos2[2] - pos1[2] };

    Vec3 dv = { pos3[0] - pos1[0], pos3[1] - pos1[1], pos3[2] - pos1[2] };

    

    Vec3 normal = Cross(du, dv);

    NormalizeVector(normal);

    

    return normal;

}



void SurfaceBufferEvaluator::EvaluateBasePatch(const Vec3 corners[4], const Vec3 cornerNormals[4],

                                              float u, float v,

                                              Vec3& outPosition, Vec3& outNormal)

{

    float u_inv = 1.0f - u;

    float v_inv = 1.0f - v;

    

    float w00 = u_inv * v_inv;

    float w10 = u     * v_inv;

    float w11 = u     * v;

    float w01 = u_inv * v;

    

    outPosition.x = w00 * corners[0].x + w10 * corners[1].x + w11 * corners[2].x + w01 * corners[3].x;

    outPosition.y = w00 * corners[0].y + w10 * corners[1].y + w11 * corners[2].y + w01 * corners[3].y;

    outPosition.z = w00 * corners[0].z + w10 * corners[1].z + w11 * corners[2].z + w01 * corners[3].z;



    outNormal.x = w00 * cornerNormals[0].x + w10 * cornerNormals[1].x + w11 * cornerNormals[2].x + w01 * cornerNormals[3].x;

    outNormal.y = w00 * cornerNormals[0].y + w10 * cornerNormals[1].y + w11 * cornerNormals[2].y + w01 * cornerNormals[3].y;

    outNormal.z = w00 * cornerNormals[0].z + w10 * cornerNormals[1].z + w11 * cornerNormals[2].z + w01 * cornerNormals[3].z;

    

    NormalizeVector(outNormal);

}



void SurfaceBufferEvaluator::ApplyCombination(CombinationOperation op, const Vec3& dispA, const Vec3& dispB, float weight, Vec3& outDisp) {

    switch (op) {

        case COMBO_BLEND:

            outDisp = Lerp(dispA, dispB, weight);

            break;

        case COMBO_ADD:

            outDisp = { dispA.x + dispB.x, dispA.y + dispB.y, dispA.z + dispB.z };

            break;

        default:

            outDisp = dispA;

            break;

    }

}



void SurfaceBufferEvaluator::ApplyModifiers(uint8_t modifiers, uint8_t strength_enc, float animPhase, float time, float u, float v, Vec3& pos, Vec3& norm) {

    // Stub - implement as needed

}



void SurfaceBufferEvaluator::EvaluatePrimitive_Absolute(SurfaceEquationType type, const float* p, float u, float v, Vec3& pos, Vec3& norm) {

    // Stub - implement as needed

    pos = {0,0,0};

    norm = {0,0,1};

}



void SurfaceBufferEvaluator::EvaluatePatch_Displacement(SurfaceEquationType type, const float* p, float u, float v, const Vec3& baseNorm, Vec3& outDisp) {

    // Stub - implement as needed

    outDisp = {0,0,0};

}





// =============================================================================

// MESH GENERATOR IMPLEMENTATION

// =============================================================================



void MeshGenerator::GenerateGridIndices(int resolution, std::vector<uint32_t>& indices) {

    indices.clear();

    indices.reserve(resolution * resolution * 6);

    

    for (int v = 0; v < resolution; ++v) {

        for (int u = 0; u < resolution; ++u) {

            uint32_t bottomLeft = v * (resolution + 1) + u;

            uint32_t bottomRight = bottomLeft + 1;

            uint32_t topLeft = (v + 1) * (resolution + 1) + u;

            uint32_t topRight = topLeft + 1;

            

            indices.push_back(bottomLeft);

            indices.push_back(topLeft);

            indices.push_back(bottomRight);

            

            indices.push_back(bottomRight);

            indices.push_back(topLeft);

            indices.push_back(topRight);

        }

    }

}



bool MeshGenerator::ExportToOBJ(const std::string& filename,

                               const std::vector<float>& vertices,

                               const std::vector<uint32_t>& indices,

                               const std::vector<float>& normals) {

    std::ofstream file(filename);

    if (!file.is_open()) return false;

    

    file << "# OBJ file generated by Procedural Engine\n";

    

    for (size_t i = 0; i < vertices.size(); i += 3) {

        file << "v " << vertices[i] << " " << vertices[i + 1] << " " << vertices[i + 2] << "\n";

    }

    

    for (size_t i = 0; i < normals.size(); i += 3) {

        file << "vn " << normals[i] << " " << normals[i + 1] << " " << normals[i + 2] << "\n";

    }

    

    for (size_t i = 0; i < indices.size(); i += 3) {

        file << "f " << (indices[i] + 1) << " " << (indices[i + 1] + 1) << " " << (indices[i + 2] + 1) << "\n";

    }

    

    return true;

}