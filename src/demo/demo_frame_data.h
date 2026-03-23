#pragma once
#include "renderer/renderer.h"
#include "geometry/math_types.h"

// Frustum culling planes (6 planes: left, right, bottom, top, near, far)
struct FrustumPlanes {
    float planes[6][4]; // A,B,C,D for each plane
};

// Camera/projection constants shared across all render passes
static const float kDemoFovDeg = 60.0f;
static const float kDemoNear   = 0.1f;
static const float kDemoFar    = 50.0f;

// Per-frame data shared between render passes.
// Built once at the start of renderFrame(), passed to all passes.
struct FrameData {
    // Camera & projection
    Mat4 proj, view;
    Vec3 cam_pos, sun_dir;
    FrustumPlanes frustum;
    float time;
    float aspect;

    // Light (computed by computeLightMatrix)
    Mat4 light_vp;

    // Tier capability flags
    int tier_int;
    bool has_shadows;
    bool has_bloom;
    bool has_ssao;
    bool has_pbr;
    bool has_tessellation;
    bool has_compute_particles;
    bool has_volumetric_fog;
    bool has_hdr;

    // Viewport
    int viewport_w, viewport_h;
    RenderTargetHandle dest_rt;
};
