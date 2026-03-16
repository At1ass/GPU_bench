#pragma once
#include "geometry/math_types.h"

struct CameraKeypoint {
    Vec3 position;
    Vec3 target;
};

// Catmull-Rom spline camera path.
// Interpolates position and target independently.
class CameraPath {
public:
    CameraPath();

    // Evaluate camera at parameter t in [0, 1].
    // Returns lookAt matrix.
    Mat4 evaluate(float t) const;

    // Get interpolated position/target
    Vec3 getPosition(float t) const;
    Vec3 getTarget(float t) const;

    static const int NUM_KEYPOINTS = 10;

private:
    CameraKeypoint keypoints_[NUM_KEYPOINTS];

    static Vec3 catmullRom(const Vec3& p0, const Vec3& p1,
                           const Vec3& p2, const Vec3& p3, float t);
};
