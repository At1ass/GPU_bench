#pragma once
#include "demo/demo_frame_data.h"
#include "demo/scene_data.h"
#include "geometry/math_types.h"
#include <cmath>

// Shared constants and utilities for demo render passes.

static const Vec3 SUN_DIR_RAW(0.4f, 0.8f, 0.3f);
static const Vec3 FOG_COLOR(0.62f, 0.67f, 0.76f);

// Extract frustum planes from view-projection matrix
inline FrustumPlanes extractFrustum(const Mat4& vp) {
    FrustumPlanes f;
    const float* m = vp.m;
    f.planes[0][0] = m[3]  + m[0];  f.planes[0][1] = m[7]  + m[4];
    f.planes[0][2] = m[11] + m[8];  f.planes[0][3] = m[15] + m[12];
    f.planes[1][0] = m[3]  - m[0];  f.planes[1][1] = m[7]  - m[4];
    f.planes[1][2] = m[11] - m[8];  f.planes[1][3] = m[15] - m[12];
    f.planes[2][0] = m[3]  + m[1];  f.planes[2][1] = m[7]  + m[5];
    f.planes[2][2] = m[11] + m[9];  f.planes[2][3] = m[15] + m[13];
    f.planes[3][0] = m[3]  - m[1];  f.planes[3][1] = m[7]  - m[5];
    f.planes[3][2] = m[11] - m[9];  f.planes[3][3] = m[15] - m[13];
    f.planes[4][0] = m[3]  + m[2];  f.planes[4][1] = m[7]  + m[6];
    f.planes[4][2] = m[11] + m[10]; f.planes[4][3] = m[15] + m[14];
    f.planes[5][0] = m[3]  - m[2];  f.planes[5][1] = m[7]  - m[6];
    f.planes[5][2] = m[11] - m[10]; f.planes[5][3] = m[15] - m[14];
    for (int i = 0; i < 6; i++) {
        float len = sqrtf(f.planes[i][0] * f.planes[i][0] +
                          f.planes[i][1] * f.planes[i][1] +
                          f.planes[i][2] * f.planes[i][2]);
        if (len > 1e-8f) {
            float inv = 1.0f / len;
            f.planes[i][0] *= inv; f.planes[i][1] *= inv;
            f.planes[i][2] *= inv; f.planes[i][3] *= inv;
        }
    }
    return f;
}

// Test sphere against frustum (returns true if visible)
inline bool sphereInFrustum(const FrustumPlanes& f, const Vec3& center, float radius) {
    for (int i = 0; i < 6; i++) {
        float dist = f.planes[i][0] * center.x + f.planes[i][1] * center.y +
                     f.planes[i][2] * center.z + f.planes[i][3];
        if (dist < -radius) return false;
    }
    return true;
}

// Set bounding sphere from transform (assumes mesh is centered at origin)
inline void setBounds(SceneObject& obj, float mesh_radius) {
    obj.bounds_center = Vec3(obj.transform.m[12], obj.transform.m[13], obj.transform.m[14]);
    float sx = sqrtf(obj.transform.m[0]*obj.transform.m[0] + obj.transform.m[1]*obj.transform.m[1] + obj.transform.m[2]*obj.transform.m[2]);
    float sy = sqrtf(obj.transform.m[4]*obj.transform.m[4] + obj.transform.m[5]*obj.transform.m[5] + obj.transform.m[6]*obj.transform.m[6]);
    float sz = sqrtf(obj.transform.m[8]*obj.transform.m[8] + obj.transform.m[9]*obj.transform.m[9] + obj.transform.m[10]*obj.transform.m[10]);
    float max_scale = sx > sy ? (sx > sz ? sx : sz) : (sy > sz ? sy : sz);
    obj.bounds_radius = mesh_radius * max_scale;
}

// Compute orthographic light matrix from sun direction
inline void computeLightMatrix(FrameData& fd) {
    Vec3 light_pos = fd.sun_dir * 15.0f;
    Vec3 up(0.0f, 0.0f, 1.0f);
    if (fabsf(Vec3::dot(fd.sun_dir, up)) > 0.99f)
        up = Vec3(1.0f, 0.0f, 0.0f);
    Mat4 light_view = Mat4::lookAt(light_pos, Vec3(0.0f, 0.0f, 0.0f), up);
    Mat4 light_proj = Mat4::ortho(-8.0f, 8.0f, -8.0f, 8.0f, 0.1f, 30.0f);
    fd.light_vp = light_proj * light_view;
}

// Terrain height sampling (matches heightmap in demo_resources.cpp)
inline float sampleTerrainHeight(float x, float z) {
    float h = 0.0f;
    h += sinf(x * 0.4f) * cosf(z * 0.3f) * 0.6f;
    h += sinf(x * 0.7f + 1.3f) * sinf(z * 0.5f + 0.7f) * 0.3f;
    float cd = sqrtf(x * x + z * z);
    float ft = cd < 2.5f ? (cd < 1.0f ? 1.0f : (2.5f - cd) / 1.5f) : 0.0f;
    ft = ft * ft * (3.0f - 2.0f * ft);
    h *= (1.0f - ft);
    // Pond depression behind arch at (0.0, -3.5), radius ~3.0
    // Applied AFTER center flattening so the depression isn't attenuated
    float pdx = x - 0.0f, pdz = z - (-3.5f);
    float pd = sqrtf(pdx * pdx + pdz * pdz);
    float pt = pd < 3.0f ? (3.0f - pd) / 3.0f : 0.0f;
    pt = pt * pt * (3.0f - 2.0f * pt);
    h -= pt * 0.5f;
    return h - 1.0f;
}

// Set point light uniforms (array uniforms, string-based)
inline void setPointLightUniforms(ShaderProgram* shader, const FrameData& fd, int point_light_count) {
    if (point_light_count <= 0) {
        shader->set1i("u_point_light_count", 0);
        return;
    }
    shader->set1i("u_point_light_count", point_light_count);
    // 3 animated point lights orbiting around scene landmarks:
    //   0 (warm orange): obelisk at (-3.5, -2.0) — torch glow
    //   1 (cold blue):   arch gateway at (0, -2.5) — mystical light
    //   2 (green):       fallen column area at (-3.2, 1.0) — nature wisp
    static const float centers[][2] = { {-3.5f, -2.0f}, {0.0f, -2.5f}, {-3.2f, 1.0f} };
    static const float orbit_r[] = { 1.5f, 2.2f, 1.8f };
    static const float heights[] = { 0.6f, 1.2f, 0.4f };
    static const float speeds[] = { 0.8f, 1.1f, 1.4f };
    static const float colors[][3] = {
        { 1.5f, 1.0f, 0.4f },   // warm orange (T4 PBR multiplies by 3x internally)
        { 0.4f, 1.0f, 1.5f },   // cold blue
        { 1.0f, 1.5f, 0.4f }    // green
    };
    for (int i = 0; i < point_light_count && i < 3; i++) {
        float angle = fd.time * speeds[i] + static_cast<float>(i) * 2.094f;
        float px = centers[i][0] + cosf(angle) * orbit_r[i];
        float py = heights[i];
        float pz = centers[i][1] + sinf(angle) * orbit_r[i];
        char name[32];
        snprintf(name, sizeof(name), "u_point_lights[%d]", i);
        shader->set3f(name, px, py, pz);
        snprintf(name, sizeof(name), "u_point_colors[%d]", i);
        shader->set3f(name, colors[i][0], colors[i][1], colors[i][2]);
    }
}

// Compute wind direction from time — shared across OpaquePass, FurPass, GrassInstancedPass.
// Returns (wind_x, 0, wind_z) vector.
inline Vec3 computeWindDir(float time) {
    float wx = sinf(time * 0.7f) * 1.8f;
    float wz = cosf(time * 0.5f) * 1.2f;
    return Vec3(wx, 0.0f, wz);
}

// Compute view inverse matrix (transpose of rotation + recalculated translation).
// Used by SSR and VolumetricFog passes for world-space reconstruction from depth.
inline Mat4 computeViewInverse(const Mat4& view) {
    Mat4 vi;
    // Transpose 3x3 rotation block (column-major)
    vi.m[0]  = view.m[0]; vi.m[1]  = view.m[4]; vi.m[2]  = view.m[8];
    vi.m[4]  = view.m[1]; vi.m[5]  = view.m[5]; vi.m[6]  = view.m[9];
    vi.m[8]  = view.m[2]; vi.m[9]  = view.m[6]; vi.m[10] = view.m[10];
    // Recalculate translation: -R^T * t
    vi.m[12] = -(vi.m[0]*view.m[12] + vi.m[4]*view.m[13] + vi.m[8]*view.m[14]);
    vi.m[13] = -(vi.m[1]*view.m[12] + vi.m[5]*view.m[13] + vi.m[9]*view.m[14]);
    vi.m[14] = -(vi.m[2]*view.m[12] + vi.m[6]*view.m[13] + vi.m[10]*view.m[14]);
    vi.m[3] = vi.m[7] = vi.m[11] = 0.0f;
    vi.m[15] = 1.0f;
    return vi;
}
