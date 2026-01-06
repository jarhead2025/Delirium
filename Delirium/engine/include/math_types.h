#pragma once

#include <cmath>



#ifndef M_PI

#define M_PI 3.14159265358979323846

#endif

struct SharedEdge {

    int vertices[2];    // Two vertex indices

    int faces[2];       // Two face indices 

    int edgeIndices[2]; // Edge indices on each face

};

struct Vec3 {

    float x = 0.0f, y = 0.0f, z = 0.0f;

    

    Vec3() = default;

    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

};



struct Quaternion {

    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    

    Quaternion() = default;

    Quaternion(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

};



struct Matrix4x4 {

    float m[16] = {

        1,0,0,0,

        0,1,0,0,

        0,0,1,0,

        0,0,0,1

    };

    

    Matrix4x4() = default;

};



// Math utility functions

inline Vec3 Lerp(const Vec3& a, const Vec3& b, float t) {

    return Vec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);

}



inline void NormalizeVector(Vec3& v) {

    float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

    if (len > 1e-6f) {

        v.x /= len;

        v.y /= len;

        v.z /= len;

    }

}



inline Vec3 Cross(const Vec3& a, const Vec3& b) {

    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);

}



inline float Dot(const Vec3& a, const Vec3& b) {

    return a.x * b.x + a.y * b.y + a.z * b.z;

}



inline Matrix4x4 MultiplyMatrices(const Matrix4x4& matA, const Matrix4x4& matB) {

    Matrix4x4 result;

    const float* a = matA.m;

    const float* b = matB.m;

    float* r = result.m;

    

    for (int i = 0; i < 4; ++i) {

        for (int j = 0; j < 4; ++j) {

            r[i*4 + j] = a[i*4 + 0] * b[0*4 + j] +

                         a[i*4 + 1] * b[1*4 + j] +

                         a[i*4 + 2] * b[2*4 + j] +

                         a[i*4 + 3] * b[3*4 + j];

        }

    }

    return result;

}



inline Vec3 Transform(const Vec3& point, const Matrix4x4& matrix, bool isVector = false) {

    const float* m = matrix.m;

    Vec3 result;

    result.x = point.x * m[0] + point.y * m[4] + point.z * m[8];

    result.y = point.x * m[1] + point.y * m[5] + point.z * m[9];

    result.z = point.x * m[2] + point.y * m[6] + point.z * m[10];

    if (!isVector) {

        result.x += m[12];

        result.y += m[13];

        result.z += m[14];

    }

    return result;

}