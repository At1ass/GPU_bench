#pragma once
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static const float CB_PI = static_cast<float>(M_PI);

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    float length() const { return sqrtf(x * x + y * y + z * z); }
    Vec3 normalized() const {
        float l = length();
        return l > 1e-8f ? Vec3(x / l, y / l, z / l) : Vec3();
    }
    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    }
    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
};

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

struct Mat4 {
    float m[16]; // column-major

    Mat4() {
        memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    const float* ptr() const { return m; }

    static Mat4 perspective(float fov_deg, float aspect, float znear, float zfar) {
        Mat4 r;
        memset(r.m, 0, sizeof(r.m));
        float f = 1.0f / tanf(fov_deg * CB_PI / 360.0f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zfar + znear) / (znear - zfar);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = Vec3::cross(f, up).normalized();
        Vec3 u = Vec3::cross(s, f);
        Mat4 r;
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -Vec3::dot(s, eye);
        r.m[13] = -Vec3::dot(u, eye);
        r.m[14] = Vec3::dot(f, eye);
        return r;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 r;
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 r;
        r.m[0] = sx; r.m[5] = sy; r.m[10] = sz;
        return r;
    }

    static Mat4 rotateY(float angle_deg) {
        Mat4 r;
        float rad = angle_deg * CB_PI / 180.0f;
        float c = cosf(rad), s = sinf(rad);
        r.m[0] = c;  r.m[8] = s;
        r.m[2] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 rotateX(float angle_deg) {
        Mat4 r;
        float rad = angle_deg * CB_PI / 180.0f;
        float c = cosf(rad), s = sinf(rad);
        r.m[5] = c;  r.m[9] = -s;
        r.m[6] = s;  r.m[10] = c;
        return r;
    }

    static Mat4 ortho(float left, float right, float bottom, float top, float znear, float zfar) {
        Mat4 r;
        memset(r.m, 0, sizeof(r.m));
        r.m[0]  =  2.0f / (right - left);
        r.m[5]  =  2.0f / (top - bottom);
        r.m[10] = -2.0f / (zfar - znear);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(zfar + znear) / (zfar - znear);
        r.m[15] = 1.0f;
        return r;
    }

    static Mat4 rotateZ(float angle_deg) {
        Mat4 r;
        float rad = angle_deg * CB_PI / 180.0f;
        float c = cosf(rad), s = sinf(rad);
        r.m[0] = c;  r.m[4] = -s;
        r.m[1] = s;  r.m[5] = c;
        return r;
    }

    // Transform a point (w=1) by this matrix
    Vec3 transformPoint(const Vec3& p) const {
        float x = m[0]*p.x + m[4]*p.y + m[8]*p.z  + m[12];
        float y = m[1]*p.x + m[5]*p.y + m[9]*p.z  + m[13];
        float z = m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14];
        return Vec3(x, y, z);
    }

    // Transform a normal (w=0, upper-left 3x3 only)
    Vec3 transformNormal(const Vec3& n) const {
        float x = m[0]*n.x + m[4]*n.y + m[8]*n.z;
        float y = m[1]*n.x + m[5]*n.y + m[9]*n.z;
        float z = m[2]*n.x + m[6]*n.y + m[10]*n.z;
        return Vec3(x, y, z).normalized();
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        memset(r.m, 0, sizeof(r.m));
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 4; row++)
                for (int k = 0; k < 4; k++)
                    r.m[col * 4 + row] += m[k * 4 + row] * o.m[col * 4 + k];
        return r;
    }
};
