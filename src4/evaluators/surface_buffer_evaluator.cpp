#include "surface_buffer_evaluator.h"
#include <cstring>
#include <algorithm>
#include <cmath>

Vec3 SurfaceBufferEvaluator::GetClosestPointOnSurface(const ProceduralSurfaceBuffer* surface, const Vec3& point,
                                                     const std::vector<AnchorPointBuffer>& anchor_pool,
                                                     float& u, float& v) {
    if (!surface || anchor_pool.empty()) {
        u = v = 0.0f;
        return Vec3{0.0f, 0.0f, 0.0f};
    }

    SurfaceEquationType eq_type = SurfaceBufferUtils::ExtractBaseType(surface);
    Vec3 result = {0.0f, 0.0f, 0.0f};
    u = v = 0.5f; // Initial UV parameters

    if (eq_type == EQUATION_PLANE) {
        auto params = SurfaceBufferUtils::ExtractParameters(surface);
        float a = params[0], b = params[1], c = params[2], d = params[3];
        float denom = a * a + b * b + c * c;
        if (denom > 1e-6f) {
            float t = -(a * point.x + b * point.y + c * point.z + d) / denom;
            result = {point.x + a * t, point.y + b * t, point.z + c * t};
        }
    } else if (eq_type == EQUATION_BILINEAR) {
        // Check anchor indices bounds
        if (surface->anchor_point_indices[0] >= anchor_pool.size() ||
            surface->anchor_point_indices[1] >= anchor_pool.size() ||
            surface->anchor_point_indices[2] >= anchor_pool.size() ||
            surface->anchor_point_indices[3] >= anchor_pool.size()) {
            return Vec3{0.0f, 0.0f, 0.0f};
        }
        
        // Get anchor positions
        Vec3 p0 = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[0]].pos_xyz);
        Vec3 p1 = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[1]].pos_xyz);
        Vec3 p2 = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[2]].pos_xyz);
        Vec3 p3 = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[3]].pos_xyz);
        // Simple bilinear interpolation
        result.x = (1-u)*(1-v)*p0.x + u*(1-v)*p1.x + u*v*p2.x + (1-u)*v*p3.x;
        result.y = (1-u)*(1-v)*p0.y + u*(1-v)*p1.y + u*v*p2.y + (1-u)*v*p3.y;
        result.z = (1-u)*(1-v)*p0.z + u*(1-v)*p1.z + u*v*p2.z + (1-u)*v*p3.z;
    } else {
        // Fallback: return first anchor position if available
        if (surface->anchor_point_indices[0] < anchor_pool.size()) {
            result = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[0]].pos_xyz);
        }
    }

    return result;
}

void SurfaceBufferEvaluator::EvaluateSurfaceMesh(const ProceduralSurfaceBuffer* surface,
                                                const std::vector<AnchorPointBuffer>& anchor_pool,
                                                const std::vector<BoneBuffer>& bone_pool,
                                                int resolution,
                                                std::vector<float>& vertices,
                                                std::vector<uint32_t>& indices,
                                                std::vector<float>& normals,
                                                float time) {
    if (!surface || anchor_pool.empty()) return;

    SurfaceEquationType eq_type = SurfaceBufferUtils::ExtractBaseType(surface);
    uint32_t vertex_offset = static_cast<uint32_t>(vertices.size() / 3);
    float u_step = 1.0f / resolution;
    float v_step = 1.0f / resolution;

    for (int v = 0; v < resolution; ++v) {
        for (int u = 0; u < resolution; ++u) {
            float u0 = u * u_step;
            float v0 = v * v_step;
            float u1 = (u + 1) * u_step;
            float v1 = (v + 1) * v_step;

            Vec3 p00, p10, p11, p01;
            float u_out, v_out;

            if (eq_type == EQUATION_PLANE) {
                p00 = GetClosestPointOnSurface(surface, {u0, v0, 0.0f}, anchor_pool, u_out, v_out);
                p10 = GetClosestPointOnSurface(surface, {u1, v0, 0.0f}, anchor_pool, u_out, v_out);
                p11 = GetClosestPointOnSurface(surface, {u1, v1, 0.0f}, anchor_pool, u_out, v_out);
                p01 = GetClosestPointOnSurface(surface, {u0, v1, 0.0f}, anchor_pool, u_out, v_out);
            } else if (eq_type == EQUATION_BILINEAR) {
                // Check bounds before accessing anchor pool
                if (surface->anchor_point_indices[0] >= anchor_pool.size() ||
                    surface->anchor_point_indices[1] >= anchor_pool.size() ||
                    surface->anchor_point_indices[2] >= anchor_pool.size() ||
                    surface->anchor_point_indices[3] >= anchor_pool.size()) {
                    continue; // Skip this quad if invalid anchors
                }
                
                Vec3 p0 = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[0]].pos_xyz);
                Vec3 p1 = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[1]].pos_xyz);
                Vec3 p2 = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[2]].pos_xyz);
                Vec3 p3 = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[3]].pos_xyz);
                p00 = {(1-u0)*(1-v0)*p0.x + u0*(1-v0)*p1.x + u0*v0*p2.x + (1-u0)*v0*p3.x,
                       (1-u0)*(1-v0)*p0.y + u0*(1-v0)*p1.y + u0*v0*p2.y + (1-u0)*v0*p3.y,
                       (1-u0)*(1-v0)*p0.z + u0*(1-v0)*p1.z + u0*v0*p2.z + (1-u0)*v0*p3.z};
                p10 = {(1-u1)*(1-v0)*p0.x + u1*(1-v0)*p1.x + u1*v0*p2.x + (1-u1)*v0*p3.x,
                       (1-u1)*(1-v0)*p0.y + u1*(1-v0)*p1.y + u1*v0*p2.y + (1-u1)*v0*p3.y,
                       (1-u1)*(1-v0)*p0.z + u1*(1-v0)*p1.z + u1*v0*p2.z + (1-u1)*v0*p3.z};
                p11 = {(1-u1)*(1-v1)*p0.x + u1*(1-v1)*p1.x + u1*v1*p2.x + (1-u1)*v1*p3.x,
                       (1-u1)*(1-v1)*p0.y + u1*(1-v1)*p1.y + u1*v1*p2.y + (1-u1)*v1*p3.y,
                       (1-u1)*(1-v1)*p0.z + u1*(1-v1)*p1.z + u1*v1*p2.z + (1-u1)*v1*p3.z};
                p01 = {(1-u0)*(1-v1)*p0.x + u0*(1-v1)*p1.x + u0*v1*p2.x + (1-u0)*v1*p3.x,
                       (1-u0)*(1-v1)*p0.y + u0*(1-v1)*p1.y + u0*v1*p2.y + (1-u0)*v1*p3.y,
                       (1-u0)*(1-v1)*p0.z + u0*(1-v1)*p1.z + u0*v1*p2.z + (1-u0)*v1*p3.z};
            } else {
                // Fallback: use first anchor if available
                if (surface->anchor_point_indices[0] < anchor_pool.size()) {
                    p00 = p10 = p11 = p01 = AnchorPointEvaluator::DecodePosition(anchor_pool[surface->anchor_point_indices[0]].pos_xyz);
                } else {
                    p00 = p10 = p11 = p01 = Vec3{0.0f, 0.0f, 0.0f};
                }
            }

            // Calculate normal (simplified)
            Vec3 normal = {0.0f, 0.0f, 1.0f};
            if (eq_type == EQUATION_PLANE) {
                auto params = SurfaceBufferUtils::ExtractParameters(surface);
                normal = {params[0], params[1], params[2]};
                NormalizeVector(normal);
            }

            // Add vertices
            vertices.push_back(p00.x); vertices.push_back(p00.y); vertices.push_back(p00.z);
            vertices.push_back(p10.x); vertices.push_back(p10.y); vertices.push_back(p10.z);
            vertices.push_back(p11.x); vertices.push_back(p11.y); vertices.push_back(p11.z);
            vertices.push_back(p01.x); vertices.push_back(p01.y); vertices.push_back(p01.z);

            // Add normals
            normals.push_back(normal.x); normals.push_back(normal.y); normals.push_back(normal.z);
            normals.push_back(normal.x); normals.push_back(normal.y); normals.push_back(normal.z);
            normals.push_back(normal.x); normals.push_back(normal.y); normals.push_back(normal.z);
            normals.push_back(normal.x); normals.push_back(normal.y); normals.push_back(normal.z);

            // Add indices for two triangles (quad)
            uint32_t base_vertex = vertex_offset + (v * resolution + u) * 4;
            indices.push_back(base_vertex);
            indices.push_back(base_vertex + 1);
            indices.push_back(base_vertex + 2);
            indices.push_back(base_vertex);
            indices.push_back(base_vertex + 2);
            indices.push_back(base_vertex + 3);
        }
    }
}