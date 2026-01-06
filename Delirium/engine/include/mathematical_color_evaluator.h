#pragma once



#ifndef MATHEMATICAL_COLOR_EVALUATOR_H

#define MATHEMATICAL_COLOR_EVALUATOR_H



#include <stdint.h>

#include <cmath>



#ifndef M_PI

#define M_PI 3.14159265358979323846

#endif

// =============================================================================

// MATHEMATICAL COLOR EVALUATOR SYSTEM

// =============================================================================



// Color equation types (256 possible formulas per channel)

enum ColorEquationType : uint8_t {

    // Basic patterns (0-15)

    COLOR_CONSTANT = 0,          // Solid color: param = color_value

    COLOR_LINEAR_U = 1,          // Linear in U: color = param * u

    COLOR_LINEAR_V = 2,          // Linear in V: color = param * v  

    COLOR_QUADRATIC_U = 3,       // Quadratic U: color = param * u²

    COLOR_QUADRATIC_V = 4,       // Quadratic V: color = param * v²

    COLOR_BILINEAR = 5,          // Bilinear: color = param * u * v

    COLOR_RADIAL = 6,            // Radial: color = param * sqrt(u² + v²)

    COLOR_INVERSE_RADIAL = 7,    // Inverse radial: color = param / sqrt(u² + v²)

    COLOR_MANHATTAN = 8,         // Manhattan distance: color = param * (|u-0.5| + |v-0.5|)

    COLOR_CHEBYSHEV = 9,         // Chebyshev distance: color = param * max(|u-0.5|, |v-0.5|)

    COLOR_DIAGONAL = 10,         // Diagonal: color = param * (u + v)

    COLOR_ANTIDIAGONAL = 11,     // Anti-diagonal: color = param * (u - v)

    COLOR_CROSS = 12,            // Cross pattern: color = param * min(u, v, 1-u, 1-v)

    COLOR_DIAMOND = 13,          // Diamond: color = param * (1 - |u-0.5| - |v-0.5|)

    COLOR_SQUARE = 14,           // Square: color = param * (1 - max(|u-0.5|, |v-0.5|))

    COLOR_CIRCLE = 15,           // Circle: color = param * (1 - sqrt((u-0.5)² + (v-0.5)²))

    

    // Geometric patterns (16-31)

    COLOR_CHECKERBOARD = 16,     // Checkerboard: param = frequency

    COLOR_STRIPES_U = 17,        // Horizontal stripes: param = frequency

    COLOR_STRIPES_V = 18,        // Vertical stripes: param = frequency

    COLOR_STRIPES_DIAGONAL = 19, // Diagonal stripes: param = frequency

    COLOR_GRID = 20,             // Grid pattern: param = line_width

    COLOR_DOTS = 21,             // Dot pattern: param = dot_density

    COLOR_HEXAGON = 22,          // Hexagonal tiles: param = hex_size

    COLOR_TRIANGLE = 23,         // Triangle pattern: param = tri_size

    COLOR_BRICK = 24,            // Brick pattern: param = brick_ratio

    COLOR_HERRINGBONE = 25,      // Herringbone pattern: param = tile_size

    COLOR_WEAVE = 26,            // Basket weave: param = weave_density

    COLOR_SCALES = 27,           // Fish scale pattern: param = scale_size

    COLOR_HONEYCOMB = 28,        // Honeycomb: param = cell_size

    COLOR_VORONOI = 29,          // Voronoi cells: param = cell_count

    COLOR_DELAUNAY = 30,         // Delaunay triangulation: param = point_density

    COLOR_MOSAIC = 31,           // Mosaic tiles: param = tile_irregularity

    

    // Wave patterns (32-47)

    COLOR_SINE_U = 32,           // Sine wave U: color = param * sin(u * 2π)

    COLOR_SINE_V = 33,           // Sine wave V: color = param * sin(v * 2π)

    COLOR_SINE_RADIAL = 34,      // Radial sine: color = param * sin(r * 2π)

    COLOR_COSINE_U = 35,         // Cosine wave U: param = frequency

    COLOR_COSINE_V = 36,         // Cosine wave V: param = frequency

    COLOR_COSINE_RADIAL = 37,    // Radial cosine: param = frequency

    COLOR_WAVE_INTERFERENCE = 38, // Wave interference: param = frequency_ratio

    COLOR_RIPPLES = 39,          // Water ripples: param = ripple_frequency

    COLOR_STANDING_WAVE = 40,    // Standing wave: param = node_spacing

    COLOR_SPIRAL_WAVE = 41,      // Spiral wave: param = spiral_tightness

    COLOR_SAWTOOTH_U = 42,       // Sawtooth wave U: param = frequency

    COLOR_SAWTOOTH_V = 43,       // Sawtooth wave V: param = frequency

    COLOR_TRIANGLE_WAVE_U = 44,  // Triangle wave U: param = frequency

    COLOR_TRIANGLE_WAVE_V = 45,  // Triangle wave V: param = frequency

    COLOR_SQUARE_WAVE_U = 46,    // Square wave U: param = duty_cycle

    COLOR_SQUARE_WAVE_V = 47,    // Square wave V: param = duty_cycle

    

    // Noise and organic patterns (48-63)

    COLOR_PERLIN_NOISE = 48,     // Perlin noise: param = frequency

    COLOR_SIMPLEX_NOISE = 49,    // Simplex noise: param = frequency

    COLOR_FRACTAL_NOISE = 50,    // Fractal noise: param = octaves

    COLOR_TURBULENCE = 51,       // Turbulence: param = intensity

    COLOR_RIDGED_NOISE = 52,     // Ridged multifractal: param = ridge_sharpness

    COLOR_CELLULAR_NOISE = 53,   // Cellular/Worley noise: param = cell_density

    COLOR_CLOUD = 54,            // Cloud pattern: param = density_variation

    COLOR_MARBLE = 55,           // Marble veining: param = vein_intensity

    COLOR_WOOD = 56,             // Wood grain: param = ring_frequency

    COLOR_BARK = 57,             // Tree bark: param = roughness

    COLOR_FABRIC = 58,           // Fabric weave: param = thread_density

    COLOR_LEATHER = 59,          // Leather texture: param = grain_size

    COLOR_ROCK = 60,             // Rock surface: param = crystal_size

    COLOR_SAND = 61,             // Sand grains: param = grain_density

    COLOR_RUST = 62,             // Rust pattern: param = corrosion_level

    COLOR_CRACKED = 63,          // Cracked surface: param = crack_density

    

    // Gradients and blends (64-79)

    COLOR_GRADIENT_LINEAR = 64,  // Linear gradient: param = direction_angle

    COLOR_GRADIENT_RADIAL = 65,  // Radial gradient: param = center_offset

    COLOR_GRADIENT_ANGULAR = 66, // Angular gradient: param = start_angle

    COLOR_GRADIENT_DIAMOND = 67, // Diamond gradient: param = center_bias

    COLOR_GRADIENT_SPIRAL = 68,  // Spiral gradient: param = spiral_rate

    COLOR_GRADIENT_CONIC = 69,   // Conic gradient: param = cone_angle

    COLOR_BLEND_MULTIPLY = 70,   // Multiply blend: param = blend_strength

    COLOR_BLEND_SCREEN = 71,     // Screen blend: param = blend_strength

    COLOR_BLEND_OVERLAY = 72,    // Overlay blend: param = blend_strength

    COLOR_BLEND_SOFT_LIGHT = 73, // Soft light blend: param = blend_strength

    COLOR_BLEND_HARD_LIGHT = 74, // Hard light blend: param = blend_strength

    COLOR_BLEND_COLOR_DODGE = 75, // Color dodge: param = dodge_amount

    COLOR_BLEND_COLOR_BURN = 76, // Color burn: param = burn_amount

    COLOR_BLEND_DIFFERENCE = 77, // Difference: param = difference_amount

    COLOR_BLEND_EXCLUSION = 78,  // Exclusion: param = exclusion_amount

    COLOR_HSV_HUE_SHIFT = 79,    // HSV hue shift: param = hue_rotation

    

    // Mathematical functions (80-95)

    COLOR_EXPONENTIAL = 80,      // Exponential: param = exponent

    COLOR_LOGARITHMIC = 81,      // Logarithmic: param = base

    COLOR_POWER = 82,            // Power function: param = power

    COLOR_SIGMOID = 83,          // Sigmoid: param = steepness

    COLOR_GAUSSIAN = 84,         // Gaussian: param = standard_deviation

    COLOR_POLYNOMIAL = 85,       // Polynomial: param = degree

    COLOR_RATIONAL = 86,         // Rational function: param = denominator_power

    COLOR_HYPERBOLIC = 87,       // Hyperbolic: param = hyperbola_shape

    COLOR_ELLIPTIC = 88,         // Elliptic: param = eccentricity

    COLOR_BESSEL = 89,           // Bessel function: param = order

    COLOR_LEGENDRE = 90,         // Legendre polynomial: param = degree

    COLOR_CHEBYSHEV_POLY = 91,   // Chebyshev polynomial: param = kind

    COLOR_FOURIER = 92,          // Fourier series: param = harmonic_count

    COLOR_ZERNIKE = 93,          // Zernike polynomial: param = radial_order

    COLOR_SPHERICAL_HARMONIC = 94, // Spherical harmonic: param = degree

    COLOR_FRESNEL = 95,          // Fresnel integral: param = scale_factor

    

    // Specialized patterns (96-111)

    COLOR_MANDELBROT = 96,       // Mandelbrot set: param = iteration_count

    COLOR_JULIA = 97,            // Julia set: param = julia_parameter

    COLOR_BARNSLEY_FERN = 98,    // Barnsley fern: param = scale

    COLOR_SIERPINSKI = 99,       // Sierpinski triangle: param = iteration_depth

    COLOR_DRAGON_CURVE = 100,    // Dragon curve: param = curve_order

    COLOR_HILBERT_CURVE = 101,   // Hilbert curve: param = curve_order

    COLOR_CANTOR_SET = 102,      // Cantor set: param = iteration_depth

    COLOR_KOCH_SNOWFLAKE = 103,  // Koch snowflake: param = iteration_depth

    COLOR_APOLLONIAN = 104,      // Apollonian gasket: param = circle_density

    COLOR_PENROSE = 105,         // Penrose tiling: param = tile_scale

    COLOR_QUASICRYSTAL = 106,    // Quasicrystal: param = symmetry_order

    COLOR_APERIODIC = 107,       // Aperiodic tiling: param = tile_complexity

    COLOR_FIBONACCI_SPIRAL = 108, // Fibonacci spiral: param = growth_rate

    COLOR_GOLDEN_RATIO = 109,    // Golden ratio spiral: param = spiral_tightness

    COLOR_PHYLLOTAXIS = 110,     // Phyllotaxis pattern: param = divergence_angle

    COLOR_CRYSTAL_LATTICE = 111, // Crystal lattice: param = lattice_type

    

    // Advanced textures (112-127)

    COLOR_DENIM = 112,           // Denim texture: param = weave_tightness

    COLOR_VELVET = 113,          // Velvet: param = fiber_density

    COLOR_SILK = 114,            // Silk: param = thread_smoothness

    COLOR_COTTON = 115,          // Cotton: param = fiber_irregularity

    COLOR_LINEN = 116,           // Linen: param = weave_looseness

    COLOR_WOOL = 117,            // Wool: param = fiber_curl

    COLOR_FUR = 118,             // Fur texture: param = hair_density

    COLOR_FEATHERS = 119,        // Feather pattern: param = barb_density

    COLOR_SCALES_FISH = 120,     // Fish scales: param = scale_overlap

    COLOR_SCALES_REPTILE = 121,  // Reptile scales: param = scale_size

    COLOR_CORAL = 122,           // Coral texture: param = branch_density

    COLOR_SPONGE = 123,          // Sponge: param = pore_size

    COLOR_FOAM = 124,            // Foam bubbles: param = bubble_size

    COLOR_CRYSTALLINE = 125,     // Crystalline: param = crystal_facets

    COLOR_METALLIC = 126,        // Metallic: param = reflection_sharpness

    COLOR_IRIDESCENT = 127,      // Iridescent: param = color_shift_rate

    

    // Reserved for future expansion (128-255)

    COLOR_CUSTOM_128 = 128,

    COLOR_CUSTOM_129 = 129,

    // ... up to COLOR_CUSTOM_255 = 255

};



// Color evaluator class

class MathematicalColorEvaluator {

public:

    // Main evaluation function - evaluates a single color channel

    static float EvaluateColorChannel(ColorEquationType equation, uint8_t param, float u, float v);

    static void EvaluateColor(ColorEquationType eqnR, uint8_t paramR,

                             ColorEquationType eqnG, uint8_t paramG,

                             ColorEquationType eqnB, uint8_t paramB,

                             float u, float v, float* outRGB);

    static void EvaluateColorFromBuffer(const void* buffer, float u, float v, float* outRGB);

    

    

    // Utility functions for common color operations

    static float ClampColor(float value);

    static void RGBToHSV(const float* rgb, float* hsv);

    static void HSVToRGB(const float* hsv, float* rgb);

    static float GammaCorrect(float value, float gamma = 2.2f);

    static float InverseGammaCorrect(float value, float gamma = 2.2f);

    

    // Get description of color equation

    static const char* GetColorEquationName(ColorEquationType equation);

    static const char* GetColorEquationDescription(ColorEquationType equation);

    

private:

    // Basic pattern evaluators

    static float EvaluateBasicPattern(ColorEquationType equation, float param, float u, float v);

    

    // Geometric pattern evaluators

    static float EvaluateGeometricPattern(ColorEquationType equation, float param, float u, float v);

    

    // Wave pattern evaluators

    static float EvaluateWavePattern(ColorEquationType equation, float param, float u, float v);

    

    // Noise and organic pattern evaluators

    static float EvaluateNoisePattern(ColorEquationType equation, float param, float u, float v);

    

    // Gradient and blend evaluators

    static float EvaluateGradientPattern(ColorEquationType equation, float param, float u, float v);

    

    // Mathematical function evaluators

    static float EvaluateMathematicalFunction(ColorEquationType equation, float param, float u, float v);

    

    // Specialized pattern evaluators

    static float EvaluateSpecializedPattern(ColorEquationType equation, float param, float u, float v);

    

    // Advanced texture evaluators

    static float EvaluateAdvancedTexture(ColorEquationType equation, float param, float u, float v);

    

    // Noise generation functions

    static float PerlinNoise2D(float x, float y);

    static float SimplexNoise2D(float x, float y);

    static float CellularNoise2D(float x, float y);

    static float RidgedNoise2D(float x, float y);

    static float FractalNoise2D(float x, float y, int octaves, float persistence);

    

    // Utility math functions

    static float SmoothStep(float edge0, float edge1, float x);

    static float SmootherStep(float edge0, float edge1, float x);

    static float Mix(float a, float b, float t);

    static float Fract(float x);

    static float Hash(float x, float y);

    static float Random(float x, float y);

    

    // Pattern generation helpers

    static float GridPattern(float u, float v, float frequency, float lineWidth);

    static float BrickPattern(float u, float v, float brickWidth, float brickHeight);

    static float HexagonPattern(float u, float v, float scale);

    static float VoronoiPattern(float u, float v, float cellCount);

    static float CheckerboardPattern(float u, float v, float frequency);

    

    // Wave generation helpers

    static float SineWave(float x, float frequency, float phase = 0.0f);

    static float CosineWave(float x, float frequency, float phase = 0.0f);

    static float SawtoothWave(float x, float frequency);

    static float TriangleWave(float x, float frequency);

    static float SquareWave(float x, float frequency, float dutyCycle = 0.5f);

    

    // Fractal and mathematical helpers

    static int MandelbrotIteration(float x, float y, int maxIterations);

    static int JuliaIteration(float x, float y, float cx, float cy, int maxIterations);

    static float FibonacciSpiral(float u, float v, float growthRate);

    static float GoldenRatioSpiral(float u, float v, float tightness);

};



// Color equation parameter encoder/decoder

class ColorParameterEncoder {

public:

    // Encode common parameter types to 0-255 range

    static uint8_t EncodeFrequency(float frequency, float maxFreq = 32.0f);

    static uint8_t EncodeAngle(float angleRadians);

    static uint8_t EncodeNormalized(float value); // 0.0-1.0 to 0-255

    static uint8_t EncodeSignedNormalized(float value); // -1.0-1.0 to 0-255

    static uint8_t EncodeIntegerCount(int count, int maxCount = 255);

    

    // Decode parameter types from 0-255 range

    static float DecodeFrequency(uint8_t encoded, float maxFreq = 32.0f);

    static float DecodeAngle(uint8_t encoded);

    static float DecodeNormalized(uint8_t encoded); // 0-255 to 0.0-1.0

    static float DecodeSignedNormalized(uint8_t encoded); // 0-255 to -1.0-1.0

    static int DecodeIntegerCount(uint8_t encoded, int maxCount = 255);

};

#endif