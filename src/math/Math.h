#pragma once

#include <array>
#include <cmath>

namespace vr::math {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct Mat4 {
    std::array<float, 16> m{};
};

inline float radians(float degrees) {
    return degrees * 0.017453292519943295769f;
}

inline Vec3 add(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 subtract(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 scale(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline Vec3 normalize(const Vec3& v) {
    const float len = std::sqrt(dot(v, v));
    if (len <= 1e-6f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return scale(v, 1.0f / len);
}

inline Mat4 identity() {
    Mat4 out{};
    out.m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    return out;
}

inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            out.m[col * 4 + row] = sum;
        }
    }
    return out;
}

inline Mat4 rotateY(float radiansAngle) {
    Mat4 out = identity();
    const float c = std::cos(radiansAngle);
    const float s = std::sin(radiansAngle);
    out.m[0] = c;
    out.m[2] = -s;
    out.m[8] = s;
    out.m[10] = c;
    return out;
}

inline Mat4 translation(const Vec3& t) {
    Mat4 out = identity();
    out.m[12] = t.x;
    out.m[13] = t.y;
    out.m[14] = t.z;
    return out;
}

inline Mat4 scaleMatrix(const Vec3& s) {
    Mat4 out = identity();
    out.m[0] = s.x;
    out.m[5] = s.y;
    out.m[10] = s.z;
    return out;
}

inline Mat4 quaternionToMatrix(const Vec4& q) {
    const float x = q.x;
    const float y = q.y;
    const float z = q.z;
    const float w = q.w;

    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    Mat4 out = identity();
    out.m[0] = 1.0f - 2.0f * (yy + zz);
    out.m[1] = 2.0f * (xy + wz);
    out.m[2] = 2.0f * (xz - wy);

    out.m[4] = 2.0f * (xy - wz);
    out.m[5] = 1.0f - 2.0f * (xx + zz);
    out.m[6] = 2.0f * (yz + wx);

    out.m[8] = 2.0f * (xz + wy);
    out.m[9] = 2.0f * (yz - wx);
    out.m[10] = 1.0f - 2.0f * (xx + yy);
    return out;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 f = normalize(subtract(center, eye));
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 out = identity();
    out.m[0] = s.x;
    out.m[1] = u.x;
    out.m[2] = -f.x;

    out.m[4] = s.y;
    out.m[5] = u.y;
    out.m[6] = -f.y;

    out.m[8] = s.z;
    out.m[9] = u.z;
    out.m[10] = -f.z;

    out.m[12] = -dot(s, eye);
    out.m[13] = -dot(u, eye);
    out.m[14] = dot(f, eye);
    return out;
}

inline Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) {
    Mat4 out{};
    const float tanHalfFovy = std::tan(fovYRadians * 0.5f);
    out.m[0] = 1.0f / (aspect * tanHalfFovy);
    out.m[5] = -1.0f / tanHalfFovy;  // Vulkan clip space uses inverted Y in projection.
    out.m[10] = zFar / (zNear - zFar);
    out.m[11] = -1.0f;
    out.m[14] = (zNear * zFar) / (zNear - zFar);
    return out;
}

}  // namespace vr::math
