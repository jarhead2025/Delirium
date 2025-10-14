#ifndef PSF_AGGRESSIVE_COMPRESSOR_H
#define PSF_AGGRESSIVE_COMPRESSOR_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <limits>
#include <iostream>
#include <fstream>
#include <map>
#include <set>

// Include your existing headers
#include "math_types.h"
#include "object_descriptor_buffers.h"
#include "surface_buffers.h"
#include "anchorpoint_evaluator.h"

// Extended equation types for aggressive compression
enum ExtendedEquationType {
    EQ_PLANE = 0,
    EQ_BILINEAR = 1,
    EQ_QUADRATIC = 2,           // z = a + bx + cy + dx² + exy + fy²
    EQ_CUBIC = 3,               // z = ... + gx³ + hx²y + ixy² + jy³
    EQ_QUARTIC = 4,             // 4th order polynomial
    EQ_SINE = 5,                // z = a + b*sin(cx + d) + e*sin(fy + g)
    EQ_COSINE = 6,              // z = a + b*cos(cx + d) + e*cos(fy + g)
    EQ_SINE_COSINE = 7,         // Combined sine/cosine
    EQ_EXPONENTIAL = 8,         // z = a + b*exp(cx) + d*exp(ey)
    EQ_LOGARITHMIC = 9,         // z = a + b*ln(cx+1) + d*ln(ey+1)
    EQ_GAUSSIAN = 10,           // z = a*exp(-((x-b)²+(y-c)²)/(2d²))
    EQ_HYPERBOLIC = 11,         // z = a + b*sinh(cx) + d*cosh(ey)
    EQ_COMBINED_TRIG = 12,      // Multiple harmonics
    EQ_BEZIER_CUBIC = 13,       // Bicubic Bezier surface
    EQ_BSPLINE = 14,            // B-spline surface
    EQ_NURBS = 15,              // NURBS surface
    EQ_CUSTOM_COMPOSITE = 16    // Weighted combination of multiple equations
};

// Surface patch with fitted equation
struct FittedPatch {
    std::vector<int> vertex_indices;
    ExtendedEquationType equation_type;
    std::vector<double> equation_params;
    double fitting_error;
    Vec3 bbox_min, bbox_max;
    int anchor_indices[4];
    
    // For composite equations
    std::vector<ExtendedEquationType> component_types;
    std::vector<std::vector<double>> component_params;
    std::vector<double> component_weights;
};

// Configuration for aggressive compression
struct CompressionConfig {
    double error_threshold = 0.001;        // Maximum allowed fitting error
    int min_patch_size = 4;                // Minimum vertices per patch
    int max_patch_size = 256;              // Maximum vertices per patch
    int max_polynomial_order = 4;          // Max polynomial degree
    bool enable_trigonometric = true;      // Enable sin/cos/tan fitting
    bool enable_exponential = true;        // Enable exp/log fitting
    bool enable_hyperbolic = true;         // Enable sinh/cosh fitting
    bool enable_composite = true;          // Enable multi-equation combinations
    int max_harmonics = 3;                 // Max harmonics for trig functions
    bool verbose = true;
    
    // Adaptive thresholds based on curvature
    double flat_threshold = 0.0001;        // Use plane for nearly flat
    double smooth_threshold = 0.01;        // Use low-order for smooth
    double complex_threshold = 0.1;        // Use high-order for complex
};

class AggressivePSFCompressor {
private:
    CompressionConfig config;
    
    // Equation evaluation functions
    static double EvaluateEquation(ExtendedEquationType type, 
                                   const std::vector<double>& params,
                                   double x, double y) {
        switch(type) {
            case EQ_PLANE:
                return params[0] + params[1]*x + params[2]*y;
                
            case EQ_BILINEAR:
                return params[0] + params[1]*x + params[2]*y + params[3]*x*y;
                
            case EQ_QUADRATIC:
                return params[0] + params[1]*x + params[2]*y + 
                       params[3]*x*x + params[4]*x*y + params[5]*y*y;
                
            case EQ_CUBIC:
                return params[0] + params[1]*x + params[2]*y + 
                       params[3]*x*x + params[4]*x*y + params[5]*y*y +
                       params[6]*x*x*x + params[7]*x*x*y + 
                       params[8]*x*y*y + params[9]*y*y*y;
                
            case EQ_QUARTIC: {
                double z = params[0];
                int idx = 1;
                for (int i = 0; i <= 4; i++) {
                    for (int j = 0; j <= 4-i; j++) {
                        z += params[idx++] * pow(x, i) * pow(y, j);
                    }
                }
                return z;
            }
                
            case EQ_SINE:
                return params[0] + 
                       params[1] * sin(params[2]*x + params[3]) +
                       params[4] * sin(params[5]*y + params[6]);
                
            case EQ_COSINE:
                return params[0] + 
                       params[1] * cos(params[2]*x + params[3]) +
                       params[4] * cos(params[5]*y + params[6]);
                
            case EQ_SINE_COSINE:
                return params[0] + 
                       params[1] * sin(params[2]*x + params[3]) +
                       params[4] * cos(params[5]*x + params[6]) +
                       params[7] * sin(params[8]*y + params[9]) +
                       params[10] * cos(params[11]*y + params[12]);
                
            case EQ_EXPONENTIAL:
                return params[0] + 
                       params[1] * exp(params[2]*x) +
                       params[3] * exp(params[4]*y);
                
            case EQ_LOGARITHMIC:
                return params[0] + 
                       params[1] * log(fabs(params[2]*x) + 1.0) +
                       params[3] * log(fabs(params[4]*y) + 1.0);
                
            case EQ_GAUSSIAN:
                return params[0] * exp(-((x-params[1])*(x-params[1]) + 
                                        (y-params[2])*(y-params[2])) / 
                                       (2.0*params[3]*params[3]));
                
            case EQ_HYPERBOLIC:
                return params[0] + 
                       params[1] * sinh(params[2]*x) +
                       params[3] * cosh(params[4]*y);
                
            case EQ_COMBINED_TRIG: {
                double z = params[0];
                int idx = 1;
                int harmonics = (params.size() - 1) / 4;
                for (int h = 1; h <= harmonics; h++) {
                    z += params[idx++] * sin(h * params[idx++] * x);
                    z += params[idx++] * sin(h * params[idx++] * y);
                }
                return z;
            }
                
            default:
                return 0.0;
        }
    }
    
    // Calculate RMS error for a fit
    static double CalculateFittingError(const std::vector<Vec3>& vertices,
                                       ExtendedEquationType type,
                                       const std::vector<double>& params) {
        double sum_squared_error = 0.0;
        for (const auto& v : vertices) {
            double predicted = EvaluateEquation(type, params, v.x, v.y);
            double error = predicted - v.z;
            sum_squared_error += error * error;
        }
        return sqrt(sum_squared_error / vertices.size());
    }
    
    // Least squares fitting for polynomial equations
    std::vector<double> FitPolynomial(const std::vector<Vec3>& vertices, int order) {
        int num_terms = 0;
        for (int i = 0; i <= order; i++) {
            num_terms += (order - i + 1);
        }
        
        // Build design matrix and target vector
        std::vector<std::vector<double>> A(vertices.size(), std::vector<double>(num_terms, 0));
        std::vector<double> b(vertices.size());
        
        for (size_t i = 0; i < vertices.size(); i++) {
            const Vec3& v = vertices[i];
            b[i] = v.z;
            
            int col = 0;
            for (int p = 0; p <= order; p++) {
                for (int q = 0; q <= order - p; q++) {
                    A[i][col++] = pow(v.x, p) * pow(v.y, q);
                }
            }
        }
        
        // Solve least squares (simplified - use proper linear algebra in production)
        return SolveLeastSquares(A, b);
    }
    
    // Fit trigonometric functions using Fourier analysis
    std::vector<double> FitTrigonometric(const std::vector<Vec3>& vertices, 
                                        ExtendedEquationType type) {
        std::vector<double> params;
        
        // Estimate fundamental frequencies using FFT-like approach
        double mean_z = 0;
        for (const auto& v : vertices) mean_z += v.z;
        mean_z /= vertices.size();
        params.push_back(mean_z); // DC offset
        
        // Find dominant frequencies in x and y directions
        auto [freq_x, amp_x, phase_x] = EstimateDominantFrequency(vertices, true);
        auto [freq_y, amp_y, phase_y] = EstimateDominantFrequency(vertices, false);
        
        if (type == EQ_SINE) {
            params.push_back(amp_x);
            params.push_back(freq_x);
            params.push_back(phase_x);
            params.push_back(amp_y);
            params.push_back(freq_y);
            params.push_back(phase_y);
        } else if (type == EQ_SINE_COSINE) {
            // Full Fourier series with sin and cos
            params.push_back(amp_x);
            params.push_back(freq_x);
            params.push_back(phase_x);
            params.push_back(amp_x * 0.5);
            params.push_back(freq_x);
            params.push_back(phase_x + M_PI/2);
            params.push_back(amp_y);
            params.push_back(freq_y);
            params.push_back(phase_y);
            params.push_back(amp_y * 0.5);
            params.push_back(freq_y);
            params.push_back(phase_y + M_PI/2);
        }
        
        return params;
    }
    
    // Estimate dominant frequency in data
    std::tuple<double, double, double> EstimateDominantFrequency(
        const std::vector<Vec3>& vertices, bool use_x_axis) {
        
        if (vertices.size() < 4) return {1.0, 0.0, 0.0};
        
        // Sort by x or y coordinate
        std::vector<Vec3> sorted = vertices;
        std::sort(sorted.begin(), sorted.end(), [use_x_axis](const Vec3& a, const Vec3& b) {
            return use_x_axis ? (a.x < b.x) : (a.y < b.y);
        });
        
        // Find zero crossings to estimate frequency
        double mean_z = 0;
        for (const auto& v : sorted) mean_z += v.z;
        mean_z /= sorted.size();
        
        int zero_crossings = 0;
        for (size_t i = 1; i < sorted.size(); i++) {
            if ((sorted[i-1].z - mean_z) * (sorted[i].z - mean_z) < 0) {
                zero_crossings++;
            }
        }
        
        double coord_range = use_x_axis ? 
            (sorted.back().x - sorted.front().x) : 
            (sorted.back().y - sorted.front().y);
        
        double frequency = (zero_crossings / 2.0) * (2.0 * M_PI) / coord_range;
        
        // Estimate amplitude
        double amplitude = 0;
        for (const auto& v : sorted) {
            amplitude = std::max(amplitude, fabs(v.z - mean_z));
        }
        
        return {frequency, amplitude, 0.0};
    }
    
    // Fit exponential/logarithmic functions
    std::vector<double> FitNonlinear(const std::vector<Vec3>& vertices,
                                    ExtendedEquationType type) {
        std::vector<double> params;
        
        // Use iterative optimization (simplified Levenberg-Marquardt)
        if (type == EQ_EXPONENTIAL) {
            params = {0.0, 1.0, 0.1, 1.0, 0.1}; // Initial guess
            OptimizeParameters(vertices, type, params, 100);
        } else if (type == EQ_LOGARITHMIC) {
            params = {0.0, 1.0, 1.0, 1.0, 1.0};
            OptimizeParameters(vertices, type, params, 100);
        } else if (type == EQ_GAUSSIAN) {
            // Estimate Gaussian parameters
            Vec3 center = {0, 0, 0};
            double max_z = -std::numeric_limits<double>::max();
            for (const auto& v : vertices) {
                if (v.z > max_z) {
                    max_z = v.z;
                    center = v;
                }
            }
            double sigma = ComputeStdDev(vertices, center);
            params = {max_z, center.x, center.y, sigma};
            OptimizeParameters(vertices, type, params, 50);
        }
        
        return params;
    }
    
    // Simple gradient descent optimization
    void OptimizeParameters(const std::vector<Vec3>& vertices,
                           ExtendedEquationType type,
                           std::vector<double>& params,
                           int max_iterations) {
        double learning_rate = 0.001;
        double epsilon = 1e-6;
        
        for (int iter = 0; iter < max_iterations; iter++) {
            std::vector<double> gradients(params.size(), 0.0);
            double current_error = CalculateFittingError(vertices, type, params);
            
            // Numerical gradient
            for (size_t p = 0; p < params.size(); p++) {
                params[p] += epsilon;
                double error_plus = CalculateFittingError(vertices, type, params);
                params[p] -= epsilon;
                gradients[p] = (error_plus - current_error) / epsilon;
            }
            
            // Update parameters
            for (size_t p = 0; p < params.size(); p++) {
                params[p] -= learning_rate * gradients[p];
            }
            
            // Early stopping
            if (current_error < config.error_threshold) break;
        }
    }
    
    double ComputeStdDev(const std::vector<Vec3>& vertices, const Vec3& center) {
        double sum = 0;
        for (const auto& v : vertices) {
            double dx = v.x - center.x;
            double dy = v.y - center.y;
            sum += dx*dx + dy*dy;
        }
        return sqrt(sum / vertices.size());
    }
    
    // Simplified least squares solver
    std::vector<double> SolveLeastSquares(const std::vector<std::vector<double>>& A,
                                         const std::vector<double>& b) {
        int m = A.size();
        int n = A[0].size();
        
        // Compute A^T * A and A^T * b
        std::vector<std::vector<double>> AtA(n, std::vector<double>(n, 0));
        std::vector<double> Atb(n, 0);
        
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                for (int k = 0; k < m; k++) {
                    AtA[i][j] += A[k][i] * A[k][j];
                }
            }
            for (int k = 0; k < m; k++) {
                Atb[i] += A[k][i] * b[k];
            }
        }
        
        // Solve using Gaussian elimination (simplified)
        return GaussianElimination(AtA, Atb);
    }
    
    std::vector<double> GaussianElimination(std::vector<std::vector<double>> A,
                                           std::vector<double> b) {
        int n = A.size();
        
        // Forward elimination
        for (int i = 0; i < n; i++) {
            // Find pivot
            int max_row = i;
            for (int k = i + 1; k < n; k++) {
                if (fabs(A[k][i]) > fabs(A[max_row][i])) {
                    max_row = k;
                }
            }
            std::swap(A[i], A[max_row]);
            std::swap(b[i], b[max_row]);
            
            // Eliminate column
            for (int k = i + 1; k < n; k++) {
                double factor = A[k][i] / (A[i][i] + 1e-10);
                for (int j = i; j < n; j++) {
                    A[k][j] -= factor * A[i][j];
                }
                b[k] -= factor * b[i];
            }
        }
        
        // Back substitution
        std::vector<double> x(n);
        for (int i = n - 1; i >= 0; i--) {
            x[i] = b[i];
            for (int j = i + 1; j < n; j++) {
                x[i] -= A[i][j] * x[j];
            }
            x[i] /= (A[i][i] + 1e-10);
        }
        
        return x;
    }
    
    // Try all equation types and find best fit
    FittedPatch FitBestEquation(const std::vector<Vec3>& patch_vertices) {
        FittedPatch best_fit;
        best_fit.fitting_error = std::numeric_limits<double>::max();
        
        std::vector<std::pair<ExtendedEquationType, int>> candidates = {
            {EQ_PLANE, 3},
            {EQ_BILINEAR, 4},
            {EQ_QUADRATIC, 6},
        };
        
        if (config.max_polynomial_order >= 3) {
            candidates.push_back({EQ_CUBIC, 10});
        }
        if (config.max_polynomial_order >= 4) {
            candidates.push_back({EQ_QUARTIC, 15});
        }
        
        if (config.enable_trigonometric) {
            candidates.push_back({EQ_SINE, 7});
            candidates.push_back({EQ_COSINE, 7});
            candidates.push_back({EQ_SINE_COSINE, 13});
        }
        
        if (config.enable_exponential) {
            candidates.push_back({EQ_EXPONENTIAL, 5});
            candidates.push_back({EQ_LOGARITHMIC, 5});
            candidates.push_back({EQ_GAUSSIAN, 4});
        }
        
        if (config.enable_hyperbolic) {
            candidates.push_back({EQ_HYPERBOLIC, 5});
        }
        
        // Try each equation type
        for (const auto& [type, param_count] : candidates) {
            std::vector<double> params;
            
            // Fit parameters based on equation type
            if (type <= EQ_QUARTIC) {
                params = FitPolynomial(patch_vertices, type);
            } else if (type >= EQ_SINE && type <= EQ_COMBINED_TRIG) {
                params = FitTrigonometric(patch_vertices, type);
            } else {
                params = FitNonlinear(patch_vertices, type);
            }
            
            double error = CalculateFittingError(patch_vertices, type, params);
            
            if (error < best_fit.fitting_error) {
                best_fit.equation_type = type;
                best_fit.equation_params = params;
                best_fit.fitting_error = error;
            }
        }
        
        return best_fit;
    }
    
public:
    AggressivePSFCompressor(const CompressionConfig& cfg = CompressionConfig()) 
        : config(cfg) {}
    
    // Main compression function
    ProceduralObjectCollection CompressMesh(const EnhancedMeshData& mesh) {
        if (config.verbose) {
            std::cout << "\n=== AGGRESSIVE PSF COMPRESSION ===\n";
            std::cout << "Input: " << mesh.vertices.size() << " vertices, " 
                      << mesh.faces.size() << " faces\n";
            std::cout << "Target error threshold: " << config.error_threshold << "\n";
        }
        
        // Segment mesh into patches using region growing
        std::vector<FittedPatch> patches = SegmentAndFit(mesh);
        
        if (config.verbose) {
            std::cout << "Segmented into " << patches.size() << " patches\n";
            
            // Statistics
            std::map<ExtendedEquationType, int> eq_counts;
            double total_error = 0;
            for (const auto& patch : patches) {
                eq_counts[patch.equation_type]++;
                total_error += patch.fitting_error;
            }
            
            std::cout << "\nEquation type distribution:\n";
            for (const auto& [type, count] : eq_counts) {
                std::cout << "  Type " << type << ": " << count << " patches\n";
            }
            std::cout << "Average fitting error: " << (total_error / patches.size()) << "\n";
        }
        
        // Convert to PSF format
        return ConvertToPSF(patches, mesh);
    }
    
    // Segment mesh and fit equations
    std::vector<FittedPatch> SegmentAndFit(const EnhancedMeshData& mesh) {
        std::vector<FittedPatch> patches;
        std::set<int> unassigned;
        for (size_t i = 0; i < mesh.vertices.size(); i++) {
            unassigned.insert(i);
        }
        
        // Build adjacency graph
        std::map<int, std::set<int>> adjacency;
        for (const auto& face : mesh.faces) {
            for (size_t i = 0; i < face.size(); i++) {
                int v1 = face[i];
                int v2 = face[(i + 1) % face.size()];
                adjacency[v1].insert(v2);
                adjacency[v2].insert(v1);
            }
        }
        
        // Region growing
        while (!unassigned.empty()) {
            int seed = *unassigned.begin();
            std::vector<int> patch_indices = {seed};
            std::set<int> visited = {seed};
            std::vector<int> frontier = {seed};
            
            while (!frontier.empty() && patch_indices.size() < config.max_patch_size) {
                int current = frontier.back();
                frontier.pop_back();
                
                for (int neighbor : adjacency[current]) {
                    if (visited.count(neighbor) || !unassigned.count(neighbor)) continue;
                    
                    // Test if adding this vertex maintains low error
                    std::vector<Vec3> test_patch;
                    for (int idx : patch_indices) {
                        test_patch.push_back(mesh.vertices[idx]);
                    }
                    test_patch.push_back(mesh.vertices[neighbor]);
                    
                    FittedPatch test_fit = FitBestEquation(test_patch);
                    
                    if (test_fit.fitting_error < config.error_threshold) {
                        patch_indices.push_back(neighbor);
                        visited.insert(neighbor);
                        frontier.push_back(neighbor);
                    }
                }
            }
            
            // Finalize patch
            if (patch_indices.size() >= config.min_patch_size) {
                std::vector<Vec3> patch_verts;
                for (int idx : patch_indices) {
                    patch_verts.push_back(mesh.vertices[idx]);
                    unassigned.erase(idx);
                }
                
                FittedPatch fitted = FitBestEquation(patch_verts);
                fitted.vertex_indices = patch_indices;
                patches.push_back(fitted);
            } else {
                unassigned.erase(seed);
            }
        }
        
        return patches;
    }
    
    // Convert fitted patches to PSF format
    ProceduralObjectCollection ConvertToPSF(const std::vector<FittedPatch>& patches,
                                           const EnhancedMeshData& mesh) {
        ProceduralObjectCollection collection;
        collection.object_name = mesh.name + "_compressed";
        collection.creator_info = "Aggressive PSF Compressor";
        
        // Create anchor points from unique vertices
        std::set<int> unique_vertices;
        for (const auto& patch : patches) {
            for (int idx : patch.vertex_indices) {
                unique_vertices.insert(idx);
            }
        }
        
        std::map<int, uint16_t> vertex_to_anchor;
        auto bbox = FindBoundingBox(mesh.vertices);
        
        uint16_t anchor_idx = 0;
        for (int v_idx : unique_vertices) {
            vertex_to_anchor[v_idx] = anchor_idx++;
            
            AnchorPointBuffer anchor;
            memset(&anchor, 0, sizeof(anchor));
            
            Vec3 psf_coords = WorldToPSF(mesh.vertices[v_idx], bbox.first, bbox.second);
            anchor.pos_xyz[0] = static_cast<uint16_t>((psf_coords.x + 2.0f) / 4.0f * 4095.0f);
            anchor.pos_xyz[1] = static_cast<uint16_t>((psf_coords.y + 2.0f) / 4.0f * 4095.0f);
            anchor.pos_xyz[2] = static_cast<uint16_t>((psf_coords.z + 2.0f) / 4.0f * 4095.0f);
            
            if (v_idx < mesh.normals.size()) {
                const Vec3& n = mesh.normals[v_idx];
                anchor.normal[0] = static_cast<uint8_t>((n.x * 0.5f + 0.5f) * 255.0f);
                anchor.normal[1] = static_cast<uint8_t>((n.y * 0.5f + 0.5f) * 255.0f);
                anchor.normal[2] = static_cast<uint8_t>((n.z * 0.5f + 0.5f) * 255.0f);
            }
            
            anchor.color[0] = anchor.color[1] = anchor.color[2] = 255;
            anchor.roughness = 128;
            anchor.metallic = 0;
            
            collection.AddAnchor(anchor);
        }
        
        // Create surfaces from patches
        for (const auto& patch : patches) {
            // Find corner anchors
            uint16_t corner_anchors[4] = {0, 0, 0, 0};
            
            // Simple corner selection (can be improved)
            if (patch.vertex_indices.size() >= 4) {
                corner_anchors[0] = vertex_to_anchor[patch.vertex_indices[0]];
                corner_anchors[1] = vertex_to_anchor[patch.vertex_indices[patch.vertex_indices.size()/3]];
                corner_anchors[2] = vertex_to_anchor[patch.vertex_indices[2*patch.vertex_indices.size()/3]];
                corner_anchors[3] = vertex_to_anchor[patch.vertex_indices.back()];
            }
            
            // Pack equation parameters into surface buffer
            float params[8] = {0};
            for (size_t i = 0; i < std::min(size_t(8), patch.equation_params.size()); i++) {
                params[i] = static_cast<float>(patch.equation_params[i]);
            }
            
            // Map extended equation type to standard type (or use custom storage)
            SurfaceEquationType standard_type = EQUATION_BILINEAR;
            if (patch.equation_type <= EQ_BILINEAR) {
                standard_type = static_cast<SurfaceEquationType>(patch.equation_type);
            }
            
            ProceduralSurfaceBuffer surface = SurfaceBufferUtils::CreateWithAnchors(
                standard_type, corner_anchors, params, MOD_NONE);
            
            // Store extended type in secondary field if needed
            surface.secondary_surface_type = static_cast<uint8_t>(patch.equation_type);
            
            collection.AddSurface(surface);
        }
        
        collection.UpdateHeader();
        
        if (config.verbose) {
            size_t original_size = mesh.vertices.size() * 32 + mesh.faces.size() * 12;
            size_t compressed_size = collection.CalculateFileSize();
            double compression_ratio = static_cast<double>(original_size) / compressed_size;
            
            std::cout << "\n=== COMPRESSION RESULTS ===\n";
            std::cout << "Original size: " << original_size << " bytes\n";
            std::cout << "Compressed size: " << compressed_size << " bytes\n";
            std::cout << "Compression ratio: " << compression_ratio << "x\n";
            std::cout << "Space saved: " << (100.0 - 100.0/compression_ratio) << "%\n";
        }
        
        return collection;
    }
    
    Vec3 WorldToPSF(const Vec3& world_pos, const Vec3& min_pos, const Vec3& max_pos) {
        Vec3 size = {max_pos.x - min_pos.x, max_pos.y - min_pos.y, max_pos.z - min_pos.z};
        float max_dimension = std::max({size.x, size.y, size.z});
        
        if (max_dimension < 1e-6f) return {0, 0, 0};
        
        Vec3 center = {
            (min_pos.x + max_pos.x) * 0.5f,
            (min_pos.y + max_pos.y) * 0.5f,
            (min_pos.z + max_pos.z) * 0.5f
        };
        
        return {
            ((world_pos.x - center.x) / max_dimension) * 3.0f,
            ((world_pos.y - center.y) / max_dimension) * 3.0f,
            ((world_pos.z - center.z) / max_dimension) * 3.0f
        };
    }
};

#endif // PSF_AGGRESSIVE_COMPRESSOR_H
