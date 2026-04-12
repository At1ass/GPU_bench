#pragma once
#include "engine/frame_data.h"
#include "engine/scene_data.h"
#include "geometry/math_types.h"
#include <cmath>

// Frustum extraction and culling utilities.
// Used by engine passes (GeometryPass) and demo scene construction.

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

inline bool sphereInFrustum(const FrustumPlanes& f, const Vec3& center, float radius) {
    for (int i = 0; i < 6; i++) {
        float dist = f.planes[i][0] * center.x + f.planes[i][1] * center.y +
                     f.planes[i][2] * center.z + f.planes[i][3];
        if (dist < -radius) return false;
    }
    return true;
}

inline void setBounds(SceneObject& obj, float mesh_radius) {
    obj.bounds_center = Vec3(obj.transform.m[12], obj.transform.m[13], obj.transform.m[14]);
    float sx = sqrtf(obj.transform.m[0]*obj.transform.m[0] + obj.transform.m[1]*obj.transform.m[1] + obj.transform.m[2]*obj.transform.m[2]);
    float sy = sqrtf(obj.transform.m[4]*obj.transform.m[4] + obj.transform.m[5]*obj.transform.m[5] + obj.transform.m[6]*obj.transform.m[6]);
    float sz = sqrtf(obj.transform.m[8]*obj.transform.m[8] + obj.transform.m[9]*obj.transform.m[9] + obj.transform.m[10]*obj.transform.m[10]);
    float max_scale = sx > sy ? (sx > sz ? sx : sz) : (sy > sz ? sy : sz);
    obj.bounds_radius = mesh_radius * max_scale;
}

inline Mat4 computeViewInverse(const Mat4& view) {
    Mat4 vi;
    vi.m[0]  = view.m[0]; vi.m[1]  = view.m[4]; vi.m[2]  = view.m[8];
    vi.m[4]  = view.m[1]; vi.m[5]  = view.m[5]; vi.m[6]  = view.m[9];
    vi.m[8]  = view.m[2]; vi.m[9]  = view.m[6]; vi.m[10] = view.m[10];
    vi.m[12] = -(vi.m[0]*view.m[12] + vi.m[4]*view.m[13] + vi.m[8]*view.m[14]);
    vi.m[13] = -(vi.m[1]*view.m[12] + vi.m[5]*view.m[13] + vi.m[9]*view.m[14]);
    vi.m[14] = -(vi.m[2]*view.m[12] + vi.m[6]*view.m[13] + vi.m[10]*view.m[14]);
    vi.m[3] = vi.m[7] = vi.m[11] = 0.0f;
    vi.m[15] = 1.0f;
    return vi;
}
