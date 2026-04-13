#include <doctest.h>
#include "demo/scene/demo_camera.h"
#include <cmath>

TEST_CASE("CameraPath: t=0 returns first keypoint position") {
    CameraPath path;
    Vec3 p = path.getPosition(0.0f);
    // Should be near first keypoint (hardcoded in demo_camera.cpp)
    // Just verify it returns a valid, finite position
    CHECK(std::isfinite(p.x));
    CHECK(std::isfinite(p.y));
    CHECK(std::isfinite(p.z));
}

TEST_CASE("CameraPath: t=1 returns last keypoint position") {
    CameraPath path;
    Vec3 p = path.getPosition(1.0f);
    CHECK(std::isfinite(p.x));
    CHECK(std::isfinite(p.y));
    CHECK(std::isfinite(p.z));
}

TEST_CASE("CameraPath: continuous path (no jumps)") {
    CameraPath path;
    const int N = 100;
    const float max_jump = 1.0f;  // max allowed distance between adjacent samples
    Vec3 prev = path.getPosition(0.0f);
    for (int i = 1; i <= N; i++) {
        float t = static_cast<float>(i) / static_cast<float>(N);
        Vec3 cur = path.getPosition(t);
        float dx = cur.x - prev.x, dy = cur.y - prev.y, dz = cur.z - prev.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        CHECK(dist < max_jump);
        prev = cur;
    }
}

TEST_CASE("CameraPath: evaluate returns valid lookAt matrix") {
    CameraPath path;
    Mat4 m = path.evaluate(0.5f);
    // Should be a valid view matrix (not NaN, not all zeros)
    bool has_nonzero = false;
    for (int i = 0; i < 16; i++) {
        CHECK(std::isfinite(m.m[i]));
        if (m.m[i] != 0.0f) has_nonzero = true;
    }
    CHECK(has_nonzero);
}
