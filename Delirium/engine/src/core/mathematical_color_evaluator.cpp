#include "mathematical_color_evaluator.h"

#include <cmath>

#include <algorithm>

#include <cstring>

#include <chrono>

#include  "object_descriptor_buffers.h"

#ifndef M_PI

#define M_PI 3.14159265358979323846

#endif



#include "mathematical_color_evaluator.h"

#include "object_descriptor_buffers.h" // Essential for the cast to ProceduralSurfaceBuffer

#include <cmath>

#include <algorithm>

#include <cstring>

#include <chrono>



#ifndef M_PI

#define M_PI 3.14159265358979323846

#endif



// Main color channel evaluation function

float MathematicalColorEvaluator::EvaluateColorChannel(ColorEquationType equation, uint8_t param, float u, float v) {

    float paramFloat = static_cast<float>(param) / 255.0f; // Normalize parameter to 0-1 range

    

    // Route to appropriate evaluator based on equation type

    if (equation <= COLOR_CIRCLE) {

        return EvaluateBasicPattern(equation, paramFloat, u, v);

    } else if (equation <= COLOR_MOSAIC) {

        return EvaluateGeometricPattern(equation, paramFloat, u, v);

    } else if (equation <= COLOR_SQUARE_WAVE_V) {

        return EvaluateWavePattern(equation, paramFloat, u, v);

    } else if (equation <= COLOR_CRACKED) {

        return EvaluateNoisePattern(equation, paramFloat, u, v);

    } else if (equation <= COLOR_HSV_HUE_SHIFT) {

        return EvaluateGradientPattern(equation, paramFloat, u, v);

    } else if (equation <= COLOR_FRESNEL) {

        return EvaluateMathematicalFunction(equation, paramFloat, u, v);

    } else if (equation <= COLOR_CRYSTAL_LATTICE) {

        return EvaluateSpecializedPattern(equation, paramFloat, u, v);

    } else if (equation <= COLOR_IRIDESCENT) {

        return EvaluateAdvancedTexture(equation, paramFloat, u, v);

    } else {

        // Custom or unknown equation - default to constant

        return paramFloat;

    }

}



// Evaluate full RGB color

void MathematicalColorEvaluator::EvaluateColor(ColorEquationType eqnR, uint8_t paramR,

                                              ColorEquationType eqnG, uint8_t paramG,

                                              ColorEquationType eqnB, uint8_t paramB,

                                              float u, float v, float* outRGB) {

    outRGB[0] = ClampColor(EvaluateColorChannel(eqnR, paramR, u, v));

    outRGB[1] = ClampColor(EvaluateColorChannel(eqnG, paramG, u, v));

    outRGB[2] = ClampColor(EvaluateColorChannel(eqnB, paramB, u, v));

}



// Evaluate from buffer (assuming specific buffer layout) - CORRECTED

void MathematicalColorEvaluator::EvaluateColorFromBuffer(const void* buffer, float u, float v, float* outRGB) {

    // Cast the generic buffer pointer to our specific procedural surface type.

    const ProceduralSurfaceBuffer* surface = static_cast<const ProceduralSurfaceBuffer*>(buffer);

    

    // Directly access the named fields for the material definition.

    // This is robust against any future layout changes.

    EvaluateColor(static_cast<ColorEquationType>(surface->color_eqn_r), surface->color_param_r,

                  static_cast<ColorEquationType>(surface->color_eqn_g), surface->color_param_g,

                  static_cast<ColorEquationType>(surface->color_eqn_b), surface->color_param_b,

                  u, v, outRGB);

}



// [ ... The rest of the file (EvaluateBasicPattern, EvaluateGeometricPattern, etc.) is perfectly fine and remains unchanged ... ]



// [ ... The end of the file ... ]



// Basic pattern evaluators

float MathematicalColorEvaluator::EvaluateBasicPattern(ColorEquationType equation, float param, float u, float v) {

    switch (equation) {

        case COLOR_CONSTANT:

            return param;

            

        case COLOR_LINEAR_U:

            return param * u;

            

        case COLOR_LINEAR_V:

            return param * v;

            

        case COLOR_QUADRATIC_U:

            return param * u * u;

            

        case COLOR_QUADRATIC_V:

            return param * v * v;

            

        case COLOR_BILINEAR:

            return param * u * v;

            

        case COLOR_RADIAL: {

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f)) * 2.0f;

            return param * std::min(1.0f, r);

        }

        

        case COLOR_INVERSE_RADIAL: {

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            return (r > 0.001f) ? param * (1.0f - r) : param;

        }

        

        case COLOR_MANHATTAN: {

            float dist = fabs(u - 0.5f) + fabs(v - 0.5f);

            return param * dist;

        }

        

        case COLOR_CHEBYSHEV: {

            float dist = std::max(fabs(u - 0.5f), fabs(v - 0.5f));

            return param * dist;

        }

        

        case COLOR_DIAGONAL:

            return param * (u + v) * 0.5f;

            

        case COLOR_ANTIDIAGONAL:

            return param * fabs(u - v);

            

        case COLOR_CROSS: {

            float minDist = std::min({u, v, 1.0f - u, 1.0f - v});

            return param * minDist * 4.0f;

        }

        

        case COLOR_DIAMOND: {

            float dist = fabs(u - 0.5f) + fabs(v - 0.5f);

            return param * std::max(0.0f, 1.0f - dist * 2.0f);

        }

        

        case COLOR_SQUARE: {

            float dist = std::max(fabs(u - 0.5f), fabs(v - 0.5f));

            return param * std::max(0.0f, 1.0f - dist * 2.0f);

        }

        

        case COLOR_CIRCLE: {

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            return param * std::max(0.0f, 1.0f - r * 2.0f);

        }

        

        default:

            return param;

    }

}



// Geometric pattern evaluators

float MathematicalColorEvaluator::EvaluateGeometricPattern(ColorEquationType equation, float param, float u, float v) {

    switch (equation) {

        case COLOR_CHECKERBOARD:

            return CheckerboardPattern(u, v, param * 32.0f + 1.0f);

            

        case COLOR_STRIPES_U:

            return (sin(u * param * 64.0f * M_PI) + 1.0f) * 0.5f;

            

        case COLOR_STRIPES_V:

            return (sin(v * param * 64.0f * M_PI) + 1.0f) * 0.5f;

            

        case COLOR_STRIPES_DIAGONAL:

            return (sin((u + v) * param * 32.0f * M_PI) + 1.0f) * 0.5f;

            

        case COLOR_GRID:

            return GridPattern(u, v, param * 16.0f + 1.0f, param * 0.1f + 0.02f);

            

        case COLOR_DOTS: {

            float frequency = param * 16.0f + 1.0f;

            float dotU = sin(u * frequency * 2.0f * M_PI);

            float dotV = sin(v * frequency * 2.0f * M_PI);

            return (dotU * dotV + 1.0f) * 0.5f;

        }

        

        case COLOR_HEXAGON:

            return HexagonPattern(u, v, param * 8.0f + 1.0f);

            

        case COLOR_TRIANGLE: {

            float scale = param * 8.0f + 1.0f;

            float triU = fmod(u * scale, 1.0f);

            float triV = fmod(v * scale, 1.0f);

            return (triU + triV < 1.0f) ? 1.0f : 0.0f;

        }

        

        case COLOR_BRICK:

            return BrickPattern(u, v, param * 0.4f + 0.1f, param * 0.2f + 0.05f);

            

        case COLOR_HERRINGBONE: {

            float scale = param * 8.0f + 1.0f;

            float tileU = fmod(u * scale, 2.0f);

            float tileV = fmod(v * scale, 2.0f);

            

            bool flip = (static_cast<int>(u * scale / 2.0f) + static_cast<int>(v * scale / 2.0f)) % 2;

            if (flip) std::swap(tileU, tileV);

            

            return (tileU < 1.0f && tileV < 0.5f) ? 1.0f : 0.0f;

        }

        

        case COLOR_WEAVE: {

            float frequency = param * 16.0f + 2.0f;

            float weaveU = sin(u * frequency * M_PI);

            float weaveV = sin(v * frequency * M_PI);

            return (weaveU * weaveV + 1.0f) * 0.5f;

        }

        

        case COLOR_SCALES: {

            float scale = param * 16.0f + 2.0f;

            float scaleU = sin(u * scale * M_PI * 2.0f);

            float scaleV = sin(v * scale * M_PI * 1.5f);

            float scaleDiag = sin((u + v) * scale * M_PI);

            return (scaleU * scaleV * scaleDiag + 1.0f) * 0.5f;

        }

        

        case COLOR_HONEYCOMB:

            return HexagonPattern(u, v, param * 6.0f + 1.0f);

            

        case COLOR_VORONOI:

            return VoronoiPattern(u, v, param * 16.0f + 1.0f);

            

        case COLOR_DELAUNAY: {

            // Simplified Delaunay-like pattern

            float freq = param * 8.0f + 1.0f;

            float noise1 = Hash(floor(u * freq), floor(v * freq));

            float noise2 = Hash(floor(u * freq) + 1, floor(v * freq) + 1);

            return Mix(noise1, noise2, Fract(u * freq + v * freq));

        }

        

        case COLOR_MOSAIC: {

            float scale = param * 12.0f + 1.0f;

            float mosaicU = floor(u * scale + Random(floor(u * scale), floor(v * scale)) * 0.3f);

            float mosaicV = floor(v * scale + Random(floor(v * scale), floor(u * scale)) * 0.3f);

            return Hash(mosaicU, mosaicV);

        }

        

        default:

            return param;

    }

}



// Wave pattern evaluators

float MathematicalColorEvaluator::EvaluateWavePattern(ColorEquationType equation, float param, float u, float v) {

    switch (equation) {

        case COLOR_SINE_U:

            return SineWave(u, param * 16.0f + 0.5f);

            

        case COLOR_SINE_V:

            return SineWave(v, param * 16.0f + 0.5f);

            

        case COLOR_SINE_RADIAL: {

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            return SineWave(r, param * 32.0f + 1.0f);

        }

        

        case COLOR_COSINE_U:

            return CosineWave(u, param * 16.0f + 0.5f);

            

        case COLOR_COSINE_V:

            return CosineWave(v, param * 16.0f + 0.5f);

            

        case COLOR_COSINE_RADIAL: {

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            return CosineWave(r, param * 32.0f + 1.0f);

        }

        

        case COLOR_WAVE_INTERFERENCE: {

            float freq1 = param * 8.0f + 1.0f;

            float freq2 = freq1 * (1.0f + param);

            float wave1 = sin(u * freq1 * 2.0f * M_PI);

            float wave2 = sin(v * freq2 * 2.0f * M_PI);

            return (wave1 + wave2 + 2.0f) * 0.25f;

        }

        

        case COLOR_RIPPLES: {

            float freq = param * 16.0f + 2.0f;

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            float ripple = sin(r * freq * 2.0f * M_PI) * exp(-r * 4.0f);

            return (ripple + 1.0f) * 0.5f;

        }

        

        case COLOR_STANDING_WAVE: {

            float freq = param * 8.0f + 1.0f;

            float waveU = sin(u * freq * 2.0f * M_PI);

            float waveV = sin(v * freq * 2.0f * M_PI);

            return (waveU * waveV + 1.0f) * 0.5f;

        }

        

        case COLOR_SPIRAL_WAVE: {

            float angle = atan2(v - 0.5f, u - 0.5f);

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            float spiralValue = sin(angle * param * 8.0f + r * 32.0f);

            return (spiralValue + 1.0f) * 0.5f;

        }

        

        case COLOR_SAWTOOTH_U:

            return SawtoothWave(u, param * 16.0f + 0.5f);

            

        case COLOR_SAWTOOTH_V:

            return SawtoothWave(v, param * 16.0f + 0.5f);

            

        case COLOR_TRIANGLE_WAVE_U:

            return TriangleWave(u, param * 16.0f + 0.5f);

            

        case COLOR_TRIANGLE_WAVE_V:

            return TriangleWave(v, param * 16.0f + 0.5f);

            

        case COLOR_SQUARE_WAVE_U:

            return SquareWave(u, param * 16.0f + 0.5f, param);

            

        case COLOR_SQUARE_WAVE_V:

            return SquareWave(v, param * 16.0f + 0.5f, param);

            

        default:

            return param;

    }

}



// Noise and organic pattern evaluators

float MathematicalColorEvaluator::EvaluateNoisePattern(ColorEquationType equation, float param, float u, float v) {

    switch (equation) {

        case COLOR_PERLIN_NOISE: {

            float frequency = param * 8.0f + 0.1f;

            return (PerlinNoise2D(u * frequency, v * frequency) + 1.0f) * 0.5f;

        }

        

        case COLOR_SIMPLEX_NOISE: {

            float frequency = param * 8.0f + 0.1f;

            return (SimplexNoise2D(u * frequency, v * frequency) + 1.0f) * 0.5f;

        }

        

        case COLOR_FRACTAL_NOISE: {

            int octaves = static_cast<int>(param * 6.0f) + 1;

            return (FractalNoise2D(u, v, octaves, 0.5f) + 1.0f) * 0.5f;

        }

        

        case COLOR_TURBULENCE: {

            float intensity = param * 4.0f + 0.1f;

            float turb = fabs(PerlinNoise2D(u * intensity, v * intensity));

            turb += fabs(PerlinNoise2D(u * intensity * 2.0f, v * intensity * 2.0f)) * 0.5f;

            turb += fabs(PerlinNoise2D(u * intensity * 4.0f, v * intensity * 4.0f)) * 0.25f;

            return std::min(1.0f, turb);

        }

        

        case COLOR_RIDGED_NOISE: {

            float frequency = param * 4.0f + 0.5f;

            return RidgedNoise2D(u * frequency, v * frequency);

        }

        

        case COLOR_CELLULAR_NOISE: {

            float cellDensity = param * 8.0f + 1.0f;

            return CellularNoise2D(u * cellDensity, v * cellDensity);

        }

        

        case COLOR_CLOUD: {

            float density = param * 4.0f + 0.5f;

            float cloud = PerlinNoise2D(u * density, v * density);

            cloud += PerlinNoise2D(u * density * 2.0f, v * density * 2.0f) * 0.5f;

            cloud += PerlinNoise2D(u * density * 4.0f, v * density * 4.0f) * 0.25f;

            return (cloud + 1.0f) * 0.25f + 0.5f;

        }

        

        case COLOR_MARBLE: {

            float frequency = param * 4.0f + 0.5f;

            float noise1 = PerlinNoise2D(u * frequency, v * frequency);

            float noise2 = PerlinNoise2D(u * frequency * 2.0f, v * frequency * 2.0f) * 0.5f;

            float marble = sin((u * 8.0f + noise1 + noise2) * M_PI);

            return (marble + 1.0f) * 0.5f;

        }

        

        case COLOR_WOOD: {

            float frequency = param * 16.0f + 2.0f;

            float r = sqrt(u * u + v * v);

            float noise = PerlinNoise2D(u * 4.0f, v * 4.0f) * 0.1f;

            float rings = sin((r + noise) * frequency);

            return (rings + 1.0f) * 0.5f;

        }

        

        case COLOR_BARK: {

            float roughness = param * 6.0f + 1.0f;

            float bark = PerlinNoise2D(u * roughness, v * roughness * 3.0f);

            bark += PerlinNoise2D(u * roughness * 4.0f, v * roughness * 12.0f) * 0.3f;

            return (bark + 1.0f) * 0.4f + 0.3f;

        }

        

        case COLOR_FABRIC: {

            float threadDensity = param * 32.0f + 4.0f;

            float warp = sin(u * threadDensity * M_PI);

            float weft = sin(v * threadDensity * 0.8f * M_PI);

            float weave = warp * weft;

            float noise = PerlinNoise2D(u * threadDensity * 0.5f, v * threadDensity * 0.5f) * 0.2f;

            return (weave + noise + 1.0f) * 0.4f + 0.3f;

        }

        

        case COLOR_LEATHER: {

            float grainSize = param * 8.0f + 1.0f;

            float grain = CellularNoise2D(u * grainSize, v * grainSize);

            float noise = PerlinNoise2D(u * grainSize * 2.0f, v * grainSize * 2.0f) * 0.3f;

            return grain + noise;

        }

        

        case COLOR_ROCK: {

            float crystalSize = param * 6.0f + 1.0f;

            float rock = CellularNoise2D(u * crystalSize, v * crystalSize);

            rock *= PerlinNoise2D(u * crystalSize * 0.5f, v * crystalSize * 0.5f) * 0.5f + 0.5f;

            return rock;

        }

        

        case COLOR_SAND: {

            float grainDensity = param * 32.0f + 8.0f;

            float sand = 0.0f;

            for (int i = 0; i < 4; ++i) {

                float scale = grainDensity * pow(2.0f, i);

                sand += Random(floor(u * scale), floor(v * scale)) * pow(0.5f, i);

            }

            return sand;

        }

        

        case COLOR_RUST: {

            float corrosionLevel = param * 4.0f + 1.0f;

            float rust = PerlinNoise2D(u * corrosionLevel, v * corrosionLevel);

            rust = pow(fabs(rust), 0.5f);

            float spots = CellularNoise2D(u * corrosionLevel * 2.0f, v * corrosionLevel * 2.0f);

            return rust * 0.7f + spots * 0.3f;

        }

        

        case COLOR_CRACKED: {

            float crackDensity = param * 8.0f + 1.0f;

            float cracks = CellularNoise2D(u * crackDensity, v * crackDensity);

            cracks = 1.0f - pow(cracks, 2.0f);

            float noise = PerlinNoise2D(u * crackDensity * 0.5f, v * crackDensity * 0.5f) * 0.2f;

            return cracks + noise;

        }

        

        default:

            return param;

    }

}



// Gradient pattern evaluators

float MathematicalColorEvaluator::EvaluateGradientPattern(ColorEquationType equation, float param, float u, float v) {

    switch (equation) {

        case COLOR_GRADIENT_LINEAR: {

            float angle = param * 2.0f * M_PI;

            float gradientU = cos(angle);

            float gradientV = sin(angle);

            float t = (u - 0.5f) * gradientU + (v - 0.5f) * gradientV + 0.5f;

            return std::max(0.0f, std::min(1.0f, t));

        }

        

        case COLOR_GRADIENT_RADIAL: {

            float centerU = param;

            float centerV = 0.5f;

            float r = sqrt((u - centerU) * (u - centerU) + (v - centerV) * (v - centerV));

            return 1.0f - std::min(1.0f, r * 2.0f);

        }

        

        case COLOR_GRADIENT_ANGULAR: {

            float startAngle = param * 2.0f * M_PI;

            float angle = atan2(v - 0.5f, u - 0.5f);

            float normalizedAngle = (angle - startAngle) / (2.0f * M_PI);

            return Fract(normalizedAngle + 0.5f);

        }

        

        case COLOR_GRADIENT_DIAMOND: {

            float centerBias = param;

            float dist = fabs(u - 0.5f) + fabs(v - 0.5f);

            float t = dist * 2.0f;

            return Mix(centerBias, 1.0f - centerBias, t);

        }

        

        case COLOR_GRADIENT_SPIRAL: {

            float spiralRate = param * 8.0f + 0.5f;

            float angle = atan2(v - 0.5f, u - 0.5f);

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            float spiral = angle / (2.0f * M_PI) + r * spiralRate;

            return Fract(spiral);

        }

        

        case COLOR_GRADIENT_CONIC: {

            float coneAngle = param * M_PI;

            float angle = atan2(v - 0.5f, u - 0.5f);

            float normalizedAngle = (angle + M_PI) / (2.0f * M_PI);

            float cone = fabs(normalizedAngle - 0.5f) * 2.0f;

            return SmoothStep(0.0f, coneAngle / M_PI, cone);

        }

        

        default:

            return param;

    }

}



// Mathematical function evaluators

float MathematicalColorEvaluator::EvaluateMathematicalFunction(ColorEquationType equation, float param, float u, float v) {

    switch (equation) {

        case COLOR_EXPONENTIAL: {

            float exponent = param * 4.0f + 0.1f;

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            return exp(-r * exponent);

        }

        

        case COLOR_LOGARITHMIC: {

            float base = param * 8.0f + 1.1f;

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f)) + 0.001f;

            return log(r * base + 1.0f) / log(base + 1.0f);

        }

        

        case COLOR_POWER: {

            float power = param * 4.0f + 0.1f;

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            return pow(r, power);

        }

        

        case COLOR_SIGMOID: {

            float steepness = param * 20.0f + 1.0f;

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f)) * 2.0f - 1.0f;

            return 1.0f / (1.0f + exp(-r * steepness));

        }

        

        case COLOR_GAUSSIAN: {

            float sigma = param * 0.5f + 0.05f;

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            return exp(-(r * r) / (2.0f * sigma * sigma));

        }

        

        default:

            return param;

    }

}



// Specialized pattern evaluators

float MathematicalColorEvaluator::EvaluateSpecializedPattern(ColorEquationType equation, float param, float u, float v) {

    switch (equation) {

        case COLOR_MANDELBROT: {

            int maxIter = static_cast<int>(param * 64.0f) + 16;

            float x = (u - 0.5f) * 4.0f;

            float y = (v - 0.5f) * 4.0f;

            int iter = MandelbrotIteration(x, y, maxIter);

            return static_cast<float>(iter) / maxIter;

        }

        

        case COLOR_JULIA: {

            int maxIter = static_cast<int>(param * 64.0f) + 16;

            float x = (u - 0.5f) * 4.0f;

            float y = (v - 0.5f) * 4.0f;

            float cx = -0.7f;

            float cy = param * 0.5f;

            int iter = JuliaIteration(x, y, cx, cy, maxIter);

            return static_cast<float>(iter) / maxIter;

        }

        

        case COLOR_FIBONACCI_SPIRAL: {

            float growthRate = param * 2.0f + 0.5f;

            return FibonacciSpiral(u, v, growthRate);

        }

        

        case COLOR_GOLDEN_RATIO: {

            float tightness = param * 4.0f + 0.5f;

            return GoldenRatioSpiral(u, v, tightness);

        }

        

        default:

            return param;

    }

}



// Advanced texture evaluators

float MathematicalColorEvaluator::EvaluateAdvancedTexture(ColorEquationType equation, float param, float u, float v) {

    switch (equation) {

        case COLOR_DENIM: {

            float weaveTightness = param * 16.0f + 4.0f;

            float warp = sin(u * weaveTightness * M_PI) * 0.3f + 0.7f;

            float weft = sin(v * weaveTightness * 0.8f * M_PI) * 0.2f + 0.8f;

            float noise = PerlinNoise2D(u * weaveTightness * 2.0f, v * weaveTightness * 2.0f) * 0.1f;

            return warp * weft + noise;

        }

        

        case COLOR_VELVET: {

            float fiberDensity = param * 64.0f + 8.0f;

            float fibers = Random(floor(u * fiberDensity), floor(v * fiberDensity));

            float base = PerlinNoise2D(u * fiberDensity * 0.1f, v * fiberDensity * 0.1f) * 0.2f + 0.4f;

            return base + fibers * 0.4f;

        }

        

        case COLOR_SILK: {

            float threadSmoothness = param * 0.9f + 0.1f;

            float threads = sin(u * 128.0f * M_PI) * sin(v * 96.0f * M_PI);

            float smooth = SmoothStep(-threadSmoothness, threadSmoothness, threads);

            float sheen = pow(std::max(0.0f, smooth), 2.0f);

            return smooth * 0.6f + sheen * 0.4f;

        }

        

        case COLOR_METALLIC: {

            float reflectionSharpness = param * 8.0f + 1.0f;

            float r = sqrt((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));

            float reflection = pow(1.0f - r, reflectionSharpness);

            float base = PerlinNoise2D(u * 32.0f, v * 32.0f) * 0.1f + 0.4f;

            return base + reflection * 0.5f;

        }

        

        case COLOR_IRIDESCENT: {

            float colorShiftRate = param * 8.0f + 1.0f;

            float angle = atan2(v - 0.5f, u - 0.5f);

            float iridescence = sin(angle * colorShiftRate + u * 16.0f + v * 16.0f);

            return (iridescence + 1.0f) * 0.5f;

        }

        

        default:

            return param;

    }

}



// Utility functions

float MathematicalColorEvaluator::ClampColor(float value) {

    return std::max(0.0f, std::min(1.0f, value));

}



float MathematicalColorEvaluator::SmoothStep(float edge0, float edge1, float x) {

    float t = std::max(0.0f, std::min(1.0f, (x - edge0) / (edge1 - edge0)));

    return t * t * (3.0f - 2.0f * t);

}



float MathematicalColorEvaluator::SmootherStep(float edge0, float edge1, float x) {

    float t = std::max(0.0f, std::min(1.0f, (x - edge0) / (edge1 - edge0)));

    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);

}



float MathematicalColorEvaluator::Mix(float a, float b, float t) {

    return a * (1.0f - t) + b * t;

}



float MathematicalColorEvaluator::Fract(float x) {

    return x - floor(x);

}



float MathematicalColorEvaluator::Hash(float x, float y) {

    float h = sin(x * 12.9898f + y * 78.233f) * 43758.5453f;

    return Fract(h);

}



float MathematicalColorEvaluator::Random(float x, float y) {

    return Hash(x, y);

}



// Noise functions

float MathematicalColorEvaluator::PerlinNoise2D(float x, float y) {

    int xi = static_cast<int>(floor(x)) & 255;

    int yi = static_cast<int>(floor(y)) & 255;

    

    float xf = x - floor(x);

    float yf = y - floor(y);

    

    float u = SmootherStep(0.0f, 1.0f, xf);

    float v = SmootherStep(0.0f, 1.0f, yf);

    

    float a = Hash(xi, yi);

    float b = Hash(xi + 1, yi);

    float c = Hash(xi, yi + 1);

    float d = Hash(xi + 1, yi + 1);

    

    return Mix(Mix(a, b, u), Mix(c, d, u), v) * 2.0f - 1.0f;

}



float MathematicalColorEvaluator::SimplexNoise2D(float x, float y) {

    // Simplified 2D simplex noise

    const float F2 = 0.36602540378f; // (sqrt(3) - 1) / 2

    const float G2 = 0.21132486541f; // (3 - sqrt(3)) / 6

    

    float s = (x + y) * F2;

    int i = static_cast<int>(floor(x + s));

    int j = static_cast<int>(floor(y + s));

    

    float t = (i + j) * G2;

    float X0 = i - t;

    float Y0 = j - t;

    float x0 = x - X0;

    float y0 = y - Y0;

    

    int i1, j1;

    if (x0 > y0) {

        i1 = 1; j1 = 0;

    } else {

        i1 = 0; j1 = 1;

    }

    

    float x1 = x0 - i1 + G2;

    float y1 = y0 - j1 + G2;

    float x2 = x0 - 1.0f + 2.0f * G2;

    float y2 = y0 - 1.0f + 2.0f * G2;

    

    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;

    

    float t0 = 0.5f - x0 * x0 - y0 * y0;

    if (t0 >= 0.0f) {

        t0 *= t0;

        n0 = t0 * t0 * Hash(i, j);

    }

    

    float t1 = 0.5f - x1 * x1 - y1 * y1;

    if (t1 >= 0.0f) {

        t1 *= t1;

        n1 = t1 * t1 * Hash(i + i1, j + j1);

    }

    

    float t2 = 0.5f - x2 * x2 - y2 * y2;

    if (t2 >= 0.0f) {

        t2 *= t2;

        n2 = t2 * t2 * Hash(i + 1, j + 1);

    }

    

    return 70.0f * (n0 + n1 + n2);

}



float MathematicalColorEvaluator::CellularNoise2D(float x, float y) {

    int xi = static_cast<int>(floor(x));

    int yi = static_cast<int>(floor(y));

    

    float minDist = 1e10f;

    

    for (int i = -1; i <= 1; ++i) {

        for (int j = -1; j <= 1; ++j) {

            int cellX = xi + i;

            int cellY = yi + j;

            

            float pointX = cellX + Hash(cellX, cellY);

            float pointY = cellY + Hash(cellY, cellX);

            

            float dx = x - pointX;

            float dy = y - pointY;

            float dist = sqrt(dx * dx + dy * dy);

            

            minDist = std::min(minDist, dist);

        }

    }

    

    return minDist;

}



float MathematicalColorEvaluator::RidgedNoise2D(float x, float y) {

    float noise = PerlinNoise2D(x, y);

    return 1.0f - fabs(noise);

}



float MathematicalColorEvaluator::FractalNoise2D(float x, float y, int octaves, float persistence) {

    float value = 0.0f;

    float amplitude = 1.0f;

    float frequency = 1.0f;

    

    for (int i = 0; i < octaves; ++i) {

        value += PerlinNoise2D(x * frequency, y * frequency) * amplitude;

        amplitude *= persistence;

        frequency *= 2.0f;

    }

    

    return value;

}



// Wave functions

float MathematicalColorEvaluator::SineWave(float x, float frequency, float phase) {

    return (sin(x * frequency * 2.0f * M_PI + phase) + 1.0f) * 0.5f;

}



float MathematicalColorEvaluator::CosineWave(float x, float frequency, float phase) {

    return (cos(x * frequency * 2.0f * M_PI + phase) + 1.0f) * 0.5f;

}



float MathematicalColorEvaluator::SawtoothWave(float x, float frequency) {

    return Fract(x * frequency);

}



float MathematicalColorEvaluator::TriangleWave(float x, float frequency) {

    float t = Fract(x * frequency);

    return 2.0f * fabs(t - 0.5f);

}



float MathematicalColorEvaluator::SquareWave(float x, float frequency, float dutyCycle) {

    return (Fract(x * frequency) < dutyCycle) ? 1.0f : 0.0f;

}



// Pattern helpers

float MathematicalColorEvaluator::GridPattern(float u, float v, float frequency, float lineWidth) {

    float gridU = fabs(Fract(u * frequency) - 0.5f) * 2.0f;

    float gridV = fabs(Fract(v * frequency) - 0.5f) * 2.0f;

    

    bool lineU = gridU > (1.0f - lineWidth);

    bool lineV = gridV > (1.0f - lineWidth);

    

    return (lineU || lineV) ? 1.0f : 0.0f;

}



float MathematicalColorEvaluator::BrickPattern(float u, float v, float brickWidth, float brickHeight) {

    int brickU = static_cast<int>(u / brickWidth);

    int brickV = static_cast<int>(v / brickHeight);

    

    float offsetU = (brickV % 2) ? brickWidth * 0.5f : 0.0f;

    float localU = fmod(u + offsetU, brickWidth);

    float localV = fmod(v, brickHeight);

    

    bool isMortar = (localU < brickWidth * 0.05f || localU > brickWidth * 0.95f ||

                    localV < brickHeight * 0.05f || localV > brickHeight * 0.95f);

    

    return isMortar ? 0.3f : 0.8f;

}



float MathematicalColorEvaluator::HexagonPattern(float u, float v, float scale) {

    // Simplified hexagonal tiling

    float hexU = u * scale;

    float hexV = v * scale * 0.866f; // sqrt(3)/2

    

    int gridU = static_cast<int>(hexU);

    int gridV = static_cast<int>(hexV);

    

    // Offset every other row

    if (gridV % 2 == 1) {

        hexU += 0.5f;

        gridU = static_cast<int>(hexU);

    }

    

    float localU = Fract(hexU);

    float localV = Fract(hexV);

    

    // Approximate hexagon shape

    float dist = std::max(fabs(localU - 0.5f), fabs(localV - 0.5f) * 0.866f);

    return (dist < 0.4f) ? 1.0f : 0.0f;

}



float MathematicalColorEvaluator::VoronoiPattern(float u, float v, float cellCount) {

    return CellularNoise2D(u * cellCount, v * cellCount);

}



float MathematicalColorEvaluator::CheckerboardPattern(float u, float v, float frequency) {

    int checkU = static_cast<int>(u * frequency);

    int checkV = static_cast<int>(v * frequency);

    return ((checkU + checkV) % 2) ? 1.0f : 0.0f;

}



// Fractal helpers

int MathematicalColorEvaluator::MandelbrotIteration(float x, float y, int maxIterations) {

    float zx = 0.0f, zy = 0.0f;

    int iteration = 0;

    

    while (iteration < maxIterations && (zx * zx + zy * zy) < 4.0f) {

        float temp = zx * zx - zy * zy + x;

        zy = 2.0f * zx * zy + y;

        zx = temp;

        iteration++;

    }

    

    return iteration;

}



int MathematicalColorEvaluator::JuliaIteration(float x, float y, float cx, float cy, int maxIterations) {

    float zx = x, zy = y;

    int iteration = 0;

    

    while (iteration < maxIterations && (zx * zx + zy * zy) < 4.0f) {

        float temp = zx * zx - zy * zy + cx;

        zy = 2.0f * zx * zy + cy;

        zx = temp;

        iteration++;

    }

    

    return iteration;

}



float MathematicalColorEvaluator::FibonacciSpiral(float u, float v, float growthRate) {

    float x = (u - 0.5f) * 4.0f;

    float y = (v - 0.5f) * 4.0f;

    

    float r = sqrt(x * x + y * y);

    float angle = atan2(y, x);

    

    const float goldenAngle = 2.399963f; // 2π/φ

    float spiral = angle + r * growthRate;

    

    return Fract(spiral / goldenAngle);

}



float MathematicalColorEvaluator::GoldenRatioSpiral(float u, float v, float tightness) {

    float x = (u - 0.5f) * 4.0f;

    float y = (v - 0.5f) * 4.0f;

    

    float r = sqrt(x * x + y * y);

    float angle = atan2(y, x);

    

    const float phi = 1.618034f; // Golden ratio

    float spiral = angle + log(r + 0.1f) * tightness;

    

    return Fract(spiral / (2.0f * M_PI));

}



// Color utility functions

void MathematicalColorEvaluator::RGBToHSV(const float* rgb, float* hsv) {

    float r = rgb[0], g = rgb[1], b = rgb[2];

    float max_val = std::max({r, g, b});

    float min_val = std::min({r, g, b});

    float delta = max_val - min_val;

    

    // Value

    hsv[2] = max_val;

    

    // Saturation

    hsv[1] = (max_val == 0.0f) ? 0.0f : (delta / max_val);

    

    // Hue

    if (delta == 0.0f) {

        hsv[0] = 0.0f;

    } else if (max_val == r) {

        hsv[0] = 60.0f * fmod((g - b) / delta, 6.0f);

    } else if (max_val == g) {

        hsv[0] = 60.0f * ((b - r) / delta + 2.0f);

    } else {

        hsv[0] = 60.0f * ((r - g) / delta + 4.0f);

    }

    

    if (hsv[0] < 0.0f) hsv[0] += 360.0f;

}



void MathematicalColorEvaluator::HSVToRGB(const float* hsv, float* rgb) {

    float h = hsv[0], s = hsv[1], v = hsv[2];

    

    float c = v * s;

    float x = c * (1.0f - fabs(fmod(h / 60.0f, 2.0f) - 1.0f));

    float m = v - c;

    

    float r = 0, g = 0, b = 0;

    

    if (h < 60.0f) {

        r = c; g = x; b = 0;

    } else if (h < 120.0f) {

        r = x; g = c; b = 0;

    } else if (h < 180.0f) {

        r = 0; g = c; b = x;

    } else if (h < 240.0f) {

        r = 0; g = x; b = c;

    } else if (h < 300.0f) {

        r = x; g = 0; b = c;

    } else {

        r = c; g = 0; b = x;

    }

    

    rgb[0] = r + m;

    rgb[1] = g + m;

    rgb[2] = b + m;

}



float MathematicalColorEvaluator::GammaCorrect(float value, float gamma) {

    return pow(std::max(0.0f, value), 1.0f / gamma);

}



float MathematicalColorEvaluator::InverseGammaCorrect(float value, float gamma) {

    return pow(std::max(0.0f, value), gamma);

}



// Get equation names and descriptions

const char* MathematicalColorEvaluator::GetColorEquationName(ColorEquationType equation) {

    static const char* names[] = {

        "Constant", "Linear U", "Linear V", "Quadratic U", "Quadratic V",

        "Bilinear", "Radial", "Inverse Radial", "Manhattan", "Chebyshev",

        "Diagonal", "Anti-diagonal", "Cross", "Diamond", "Square", "Circle",

        "Checkerboard", "Stripes U", "Stripes V", "Stripes Diagonal", "Grid",

        "Dots", "Hexagon", "Triangle", "Brick", "Herringbone", "Weave",

        "Scales", "Honeycomb", "Voronoi", "Delaunay", "Mosaic",

        "Sine U", "Sine V", "Sine Radial", "Cosine U", "Cosine V",

        "Cosine Radial", "Wave Interference", "Ripples", "Standing Wave",

        "Spiral Wave", "Sawtooth U", "Sawtooth V", "Triangle Wave U",

        "Triangle Wave V", "Square Wave U", "Square Wave V",

        "Perlin Noise", "Simplex Noise", "Fractal Noise", "Turbulence",

        "Ridged Noise", "Cellular Noise", "Cloud", "Marble", "Wood",

        "Bark", "Fabric", "Leather", "Rock", "Sand", "Rust", "Cracked",

        "Linear Gradient", "Radial Gradient", "Angular Gradient",

        "Diamond Gradient", "Spiral Gradient", "Conic Gradient"

    };

    

    int index = static_cast<int>(equation);

    if (index >= 0 && index < sizeof(names) / sizeof(names[0])) {

        return names[index];

    }

    return "Unknown";

}



const char* MathematicalColorEvaluator::GetColorEquationDescription(ColorEquationType equation) {

    switch (equation) {

        case COLOR_CONSTANT: return "Solid color value";

        case COLOR_LINEAR_U: return "Linear gradient along U axis";

        case COLOR_LINEAR_V: return "Linear gradient along V axis";

        case COLOR_CHECKERBOARD: return "Checkerboard pattern";

        case COLOR_PERLIN_NOISE: return "Perlin noise texture";

        case COLOR_MARBLE: return "Marble veining pattern";

        case COLOR_WOOD: return "Wood grain rings";

        case COLOR_BRICK: return "Brick wall pattern";

        case COLOR_SINE_RADIAL: return "Radial sine wave pattern";

        case COLOR_MANDELBROT: return "Mandelbrot fractal set";

        default: return "Mathematical color pattern";

    }

}



// Parameter encoder/decoder implementations

uint8_t ColorParameterEncoder::EncodeFrequency(float frequency, float maxFreq) {

    float clamped = std::max(0.0f, std::min(maxFreq, frequency));

    return static_cast<uint8_t>((clamped / maxFreq) * 255.0f);

}



uint8_t ColorParameterEncoder::EncodeAngle(float angleRadians) {

    float normalized = fmod(angleRadians + 2.0f * M_PI, 2.0f * M_PI) / (2.0f * M_PI);

    return static_cast<uint8_t>(normalized * 255.0f);

}



uint8_t ColorParameterEncoder::EncodeNormalized(float value) {

    float clamped = std::max(0.0f, std::min(1.0f, value));

    return static_cast<uint8_t>(clamped * 255.0f);

}



uint8_t ColorParameterEncoder::EncodeSignedNormalized(float value) {

    float clamped = std::max(-1.0f, std::min(1.0f, value));

    return static_cast<uint8_t>((clamped + 1.0f) * 127.5f);

}



uint8_t ColorParameterEncoder::EncodeIntegerCount(int count, int maxCount) {

    int clamped = std::max(0, std::min(maxCount, count));

    return static_cast<uint8_t>((static_cast<float>(clamped) / maxCount) * 255.0f);

}



float ColorParameterEncoder::DecodeFrequency(uint8_t encoded, float maxFreq) {

    return (static_cast<float>(encoded) / 255.0f) * maxFreq;

}



float ColorParameterEncoder::DecodeAngle(uint8_t encoded) {

    return (static_cast<float>(encoded) / 255.0f) * 2.0f * M_PI;

}



float ColorParameterEncoder::DecodeNormalized(uint8_t encoded) {

    return static_cast<float>(encoded) / 255.0f;

}



float ColorParameterEncoder::DecodeSignedNormalized(uint8_t encoded) {

    return (static_cast<float>(encoded) / 127.5f) - 1.0f;

}



int ColorParameterEncoder::DecodeIntegerCount(uint8_t encoded, int maxCount) {

    return static_cast<int>((static_cast<float>(encoded) / 255.0f) * maxCount);

}