#pragma once
#include "engine/frustum.h"
#include "engine/shader_program.h"
#include <cmath>

// Demo-specific scene helpers. Engine-level math (frustum, bounds, view inverse)
// is in engine/frustum.h — re-exported here for backward compatibility.

static const Vec3 SUN_DIR_RAW(0.4f, 0.8f, 0.3f);
static const Vec3 FOG_COLOR(0.62f, 0.67f, 0.76f);

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
    static const float centers[][2] = { {-3.5f, -2.0f}, {0.0f, -2.5f}, {-3.2f, 1.0f} };
    static const float orbit_r[] = { 1.5f, 2.2f, 1.8f };
    static const float heights[] = { 0.6f, 1.2f, 0.4f };
    static const float speeds[] = { 0.8f, 1.1f, 1.4f };
    static const float colors[][3] = {
        { 1.5f, 1.0f, 0.4f }, { 0.4f, 1.0f, 1.5f }, { 1.0f, 1.5f, 0.4f }
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

// Compute wind direction from time
inline Vec3 computeWindDir(float time) {
    float wx = sinf(time * 0.7f) * 1.8f;
    float wz = cosf(time * 0.5f) * 1.2f;
    return Vec3(wx, 0.0f, wz);
}
