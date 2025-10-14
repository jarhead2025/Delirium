#include "mathematical_color_evaluator.h"
#include "object_descriptor_buffers.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Main color channel evaluation function
float MathematicalColorEvaluator::EvaluateColorChannel(ColorEquationType equation, uint8_t param, float u, float v) {
    float paramFloat = static_cast<float>(param) / 255.0f;
    
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
        return paramFloat;
    }
}

void MathematicalColorEvaluator::EvaluateColor(ColorEquationType eqnR, uint8_t paramR,
                                              ColorEquationType eqnG, uint8_t paramG,
                                              ColorEquationType eqnB, uint8_t paramB,
                                              float u, float v, float* outRGB) {
    outRGB[0] = ClampColor(EvaluateColorChannel(eqnR, paramR, u, v));
    outRGB[1] = ClampColor(EvaluateColorChannel(eqnG, paramG, u, v));
    outRGB[2] = ClampColor(EvaluateColorChannel(eqnB, paramB, u, v));
}

void MathematicalColorEvaluator::EvaluateColorFromBuffer(const void* buffer, float u, float v, float* outRGB) {
    const ProceduralSurfaceBuffer* surface = static_cast<const ProceduralSurfaceBuffer*>(buffer);
    
    // Get the color equations and parameters
    ColorEquationType eqnR = static_cast<ColorEquationType>(surface->color_eqn_r);
    ColorEquationType eqnG = static_cast<ColorEquationType>(surface->color_eqn_g);
    ColorEquationType eqnB = static_cast<ColorEquationType>(surface->color_eqn_b);
    
    uint8_t paramR = surface->color_param_r;
    uint8_t paramG = surface->color_param_g;
    uint8_t paramB = surface->color_param_b;
    
    // JUST USE THE STANDARD MATHEMATICAL EVALUATION - NO WEIRD INTERPOLATION
    outRGB[0] = EvaluateColorChannel(eqnR, paramR, u, v);
    outRGB[1] = EvaluateColorChannel(eqnG, paramG, u, v);
    outRGB[2] = EvaluateColorChannel(eqnB, paramB, u, v);
    
    // Clamp to valid range
    outRGB[0] = ClampColor(outRGB[0]);
    outRGB[1] = ClampColor(outRGB[1]);
    outRGB[2] = ClampColor(outRGB[2]);
}
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
            float r = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f)) * 2.0f;
            return param * std::min(1.0f, r);
        }
        case COLOR_INVERSE_RADIAL: {
            float r = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
            return (r > 0.001f) ? param * (1.0f - r) : param;
        }
        case COLOR_MANHATTAN: {
            float dist = fabsf(u - 0.5f) + fabsf(v - 0.5f);
            return param * dist;
        }
        case COLOR_CHEBYSHEV: {
            float dist = std::max(fabsf(u - 0.5f), fabsf(v - 0.5f));
            return param * dist;
        }
        case COLOR_DIAGONAL:
            return param * (u + v) * 0.5f;
        case COLOR_ANTIDIAGONAL:
            return param * fabsf(u - v);
        case COLOR_CROSS: {
            float minDist = std::min({u, v, 1.0f - u, 1.0f - v});
            return param * minDist * 4.0f;
        }
        case COLOR_DIAMOND: {
            float dist = fabsf(u - 0.5f) + fabsf(v - 0.5f);
            return param * std::max(0.0f, 1.0f - dist * 2.0f);
        }
        case COLOR_SQUARE: {
            float dist = std::max(fabsf(u - 0.5f), fabsf(v - 0.5f));
            return param * std::max(0.0f, 1.0f - dist * 2.0f);
        }
        case COLOR_CIRCLE: {
            float r = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
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
            return (sinf(u * param * 64.0f * static_cast<float>(M_PI)) + 1.0f) * 0.5f;
        case COLOR_STRIPES_V:
            return (sinf(v * param * 64.0f * static_cast<float>(M_PI)) + 1.0f) * 0.5f;
        case COLOR_STRIPES_DIAGONAL:
            return (sinf((u + v) * param * 32.0f * static_cast<float>(M_PI)) + 1.0f) * 0.5f;
        case COLOR_GRID:
            return GridPattern(u, v, param * 16.0f + 1.0f, param * 0.1f + 0.02f);
        case COLOR_DOTS: {
            float frequency = param * 16.0f + 1.0f;
            float dotU = sinf(u * frequency * 2.0f * static_cast<float>(M_PI));
            float dotV = sinf(v * frequency * 2.0f * static_cast<float>(M_PI));
            return (dotU * dotV + 1.0f) * 0.5f;
        }
        case COLOR_HEXAGON:
            return HexagonPattern(u, v, param * 8.0f + 1.0f);
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
            float r = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
            return SineWave(r, param * 32.0f + 1.0f);
        }
        case COLOR_COSINE_U:
            return CosineWave(u, param * 16.0f + 0.5f);
        case COLOR_COSINE_V:
            return CosineWave(v, param * 16.0f + 0.5f);
        default:
            return param;
    }
}

// Noise pattern evaluators
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
        default:
            return param;
    }
}

// Gradient pattern evaluators
float MathematicalColorEvaluator::EvaluateGradientPattern(ColorEquationType equation, float param, float u, float v) {
    switch (equation) {
        case COLOR_GRADIENT_LINEAR: {
            float angle = param * 2.0f * static_cast<float>(M_PI);
            float gradientU = cosf(angle);
            float gradientV = sinf(angle);
            float t = (u - 0.5f) * gradientU + (v - 0.5f) * gradientV + 0.5f;
            return std::max(0.0f, std::min(1.0f, t));
        }
        case COLOR_GRADIENT_RADIAL: {
            float centerU = param;
            float centerV = 0.5f;
            float r = sqrtf((u - centerU) * (u - centerU) + (v - centerV) * (v - centerV));
            return 1.0f - std::min(1.0f, r * 2.0f);
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
            float r = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
            return expf(-r * exponent);
        }
        case COLOR_LOGARITHMIC: {
            float base = param * 8.0f + 1.1f;
            float r = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f)) + 0.001f;
            return logf(r * base + 1.0f) / logf(base + 1.0f);
        }
        case COLOR_GAUSSIAN: {
            float sigma = param * 0.5f + 0.05f;
            float r = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
            return expf(-(r * r) / (2.0f * sigma * sigma));
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
        default:
            return param;
    }
}

// Advanced texture evaluators
float MathematicalColorEvaluator::EvaluateAdvancedTexture(ColorEquationType equation, float param, float u, float v) {
    switch (equation) {
        case COLOR_METALLIC: {
            float reflectionSharpness = param * 8.0f + 1.0f;
            float r = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
            float reflection = powf(1.0f - r, reflectionSharpness);
            float base = PerlinNoise2D(u * 32.0f, v * 32.0f) * 0.1f + 0.4f;
            return base + reflection * 0.5f;
        }
        case COLOR_IRIDESCENT: {
            float colorShiftRate = param * 8.0f + 1.0f;
            float angle = atan2f(v - 0.5f, u - 0.5f);
            float iridescence = sinf(angle * colorShiftRate + u * 16.0f + v * 16.0f);
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

float MathematicalColorEvaluator::Mix(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

float MathematicalColorEvaluator::Fract(float x) {
    return x - floorf(x);
}

float MathematicalColorEvaluator::Hash(float x, float y) {
    float h = sinf(x * 12.9898f + y * 78.233f) * 43758.5453f;
    return Fract(h);
}

// Noise functions
float MathematicalColorEvaluator::PerlinNoise2D(float x, float y) {
    int xi = static_cast<int>(floorf(x)) & 255;
    int yi = static_cast<int>(floorf(y)) & 255;
    
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    
    float u = SmoothStep(0.0f, 1.0f, xf);
    float v = SmoothStep(0.0f, 1.0f, yf);
    
    float a = Hash(static_cast<float>(xi), static_cast<float>(yi));
    float b = Hash(static_cast<float>(xi + 1), static_cast<float>(yi));
    float c = Hash(static_cast<float>(xi), static_cast<float>(yi + 1));
    float d = Hash(static_cast<float>(xi + 1), static_cast<float>(yi + 1));
    
    return Mix(Mix(a, b, u), Mix(c, d, u), v) * 2.0f - 1.0f;
}

float MathematicalColorEvaluator::SimplexNoise2D(float x, float y) {
    const float F2 = 0.36602540378f;
    const float G2 = 0.21132486541f;
    
    float s = (x + y) * F2;
    int i = static_cast<int>(floorf(x + s));
    int j = static_cast<int>(floorf(y + s));
    
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
        n0 = t0 * t0 * Hash(static_cast<float>(i), static_cast<float>(j));
    }
    
    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 >= 0.0f) {
        t1 *= t1;
        n1 = t1 * t1 * Hash(static_cast<float>(i + i1), static_cast<float>(j + j1));
    }
    
    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 >= 0.0f) {
        t2 *= t2;
        n2 = t2 * t2 * Hash(static_cast<float>(i + 1), static_cast<float>(j + 1));
    }
    
    return 70.0f * (n0 + n1 + n2);
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
    return (sinf(x * frequency * 2.0f * static_cast<float>(M_PI) + phase) + 1.0f) * 0.5f;
}

float MathematicalColorEvaluator::CosineWave(float x, float frequency, float phase) {
    return (cosf(x * frequency * 2.0f * static_cast<float>(M_PI) + phase) + 1.0f) * 0.5f;
}

// Pattern helpers
float MathematicalColorEvaluator::GridPattern(float u, float v, float frequency, float lineWidth) {
    float gridU = fabsf(Fract(u * frequency) - 0.5f) * 2.0f;
    float gridV = fabsf(Fract(v * frequency) - 0.5f) * 2.0f;
    
    bool lineU = gridU > (1.0f - lineWidth);
    bool lineV = gridV > (1.0f - lineWidth);
    
    return (lineU || lineV) ? 1.0f : 0.0f;
}

float MathematicalColorEvaluator::HexagonPattern(float u, float v, float scale) {
    float hexU = u * scale;
    float hexV = v * scale * 0.866f;
    
    int gridU = static_cast<int>(hexU);
    int gridV = static_cast<int>(hexV);
    
    if (gridV % 2 == 1) {
        hexU += 0.5f;
        gridU = static_cast<int>(hexU);
    }
    
    float localU = Fract(hexU);
    float localV = Fract(hexV);
    
    float dist = std::max(fabsf(localU - 0.5f), fabsf(localV - 0.5f) * 0.866f);
    return (dist < 0.4f) ? 1.0f : 0.0f;
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

// Get equation names
const char* MathematicalColorEvaluator::GetColorEquationName(ColorEquationType equation) {
    static const char* names[] = {
        "Constant", "Linear U", "Linear V", "Quadratic U", "Quadratic V",
        "Bilinear", "Radial", "Inverse Radial", "Manhattan", "Chebyshev",
        "Diagonal", "Anti-diagonal", "Cross", "Diamond", "Square", "Circle",
        "Checkerboard", "Stripes U", "Stripes V", "Stripes Diagonal", "Grid",
        "Dots", "Hexagon", "Triangle", "Brick", "Herringbone", "Weave",
        "Scales", "Honeycomb", "Voronoi", "Delaunay", "Mosaic"
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
        default: return "Mathematical color pattern";
    }
}

// Parameter encoder/decoder implementations
uint8_t ColorParameterEncoder::EncodeFrequency(float frequency, float maxFreq) {
    float clamped = std::max(0.0f, std::min(maxFreq, frequency));
    return static_cast<uint8_t>((clamped / maxFreq) * 255.0f);
}

uint8_t ColorParameterEncoder::EncodeAngle(float angleRadians) {
    float normalized = fmodf(angleRadians + 2.0f * static_cast<float>(M_PI), 2.0f * static_cast<float>(M_PI)) / (2.0f * static_cast<float>(M_PI));
    return static_cast<uint8_t>(normalized * 255.0f);
}

uint8_t ColorParameterEncoder::EncodeNormalized(float value) {
    float clamped = std::max(0.0f, std::min(1.0f, value));
    return static_cast<uint8_t>(clamped * 255.0f);
}

float ColorParameterEncoder::DecodeFrequency(uint8_t encoded, float maxFreq) {
    return (static_cast<float>(encoded) / 255.0f) * maxFreq;
}

float ColorParameterEncoder::DecodeAngle(uint8_t encoded) {
    return (static_cast<float>(encoded) / 255.0f) * 2.0f * static_cast<float>(M_PI);
}

float ColorParameterEncoder::DecodeNormalized(uint8_t encoded) {
    return static_cast<float>(encoded) / 255.0f;
}