#include "demo/demo_camera.h"
#include <cmath>

Vec3 CameraPath::catmullRom(const Vec3& p0, const Vec3& p1,
                             const Vec3& p2, const Vec3& p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return Vec3(
        0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
        0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
        0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t +
                (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
                (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3)
    );
}

CameraPath::CameraPath() {
    // Smooth orbit around a centered OBJ model.
    // Model fits in [-1,1] cube. Camera radius ~3.5.
    // All targets look at model center (0,0,0).
    // Height varies for interesting angles.

    float r = 3.5f;

    // 1. Start: high front-right
    keypoints_[0] = { Vec3( r*0.7f,  1.8f, -r*0.7f), Vec3(0, 0, 0) };
    // 2. Descend to eye level, front
    keypoints_[1] = { Vec3( 0.3f,    0.5f, -r),       Vec3(0, 0, 0) };
    // 3. Low angle, right side
    keypoints_[2] = { Vec3( r,       0.2f,  0.0f),    Vec3(0, 0, 0) };
    // 4. Rise, back-right
    keypoints_[3] = { Vec3( r*0.7f,  1.2f,  r*0.7f), Vec3(0, 0, 0) };
    // 5. High back
    keypoints_[4] = { Vec3( 0.0f,    2.5f,  r),       Vec3(0, 0, 0) };
    // 6. Descend, back-left
    keypoints_[5] = { Vec3(-r*0.7f,  0.8f,  r*0.7f), Vec3(0, 0, 0) };
    // 7. Low left
    keypoints_[6] = { Vec3(-r,       0.3f,  0.0f),    Vec3(0, 0, 0) };
    // 8. Rise, front-left
    keypoints_[7] = { Vec3(-r*0.7f,  1.5f, -r*0.7f), Vec3(0, 0, 0) };
    // 9. High overhead
    keypoints_[8] = { Vec3( 0.5f,    3.5f, -0.5f),    Vec3(0, 0, 0) };
    // 10. Return to start position
    keypoints_[9] = { Vec3( r*0.7f,  1.8f, -r*0.7f), Vec3(0, 0, 0) };
}

Vec3 CameraPath::getPosition(float t) const {
    if (t <= 0.0f) t = 0.0f;
    if (t >= 1.0f) t = 1.0f;

    float seg_f = t * static_cast<float>(NUM_KEYPOINTS - 1);
    int seg = static_cast<int>(seg_f);
    float local_t = seg_f - static_cast<float>(seg);

    if (seg >= NUM_KEYPOINTS - 1) {
        seg = NUM_KEYPOINTS - 2;
        local_t = 1.0f;
    }

    int i0 = (seg > 0) ? seg - 1 : 0;
    int i1 = seg;
    int i2 = seg + 1;
    int i3 = (seg + 2 < NUM_KEYPOINTS) ? seg + 2 : NUM_KEYPOINTS - 1;

    return catmullRom(keypoints_[i0].position, keypoints_[i1].position,
                      keypoints_[i2].position, keypoints_[i3].position, local_t);
}

Vec3 CameraPath::getTarget(float t) const {
    if (t <= 0.0f) t = 0.0f;
    if (t >= 1.0f) t = 1.0f;

    float seg_f = t * static_cast<float>(NUM_KEYPOINTS - 1);
    int seg = static_cast<int>(seg_f);
    float local_t = seg_f - static_cast<float>(seg);

    if (seg >= NUM_KEYPOINTS - 1) {
        seg = NUM_KEYPOINTS - 2;
        local_t = 1.0f;
    }

    int i0 = (seg > 0) ? seg - 1 : 0;
    int i1 = seg;
    int i2 = seg + 1;
    int i3 = (seg + 2 < NUM_KEYPOINTS) ? seg + 2 : NUM_KEYPOINTS - 1;

    return catmullRom(keypoints_[i0].target, keypoints_[i1].target,
                      keypoints_[i2].target, keypoints_[i3].target, local_t);
}

Mat4 CameraPath::evaluate(float t) const {
    Vec3 pos = getPosition(t);
    Vec3 tgt = getTarget(t);
    return Mat4::lookAt(pos, tgt, Vec3(0, 1, 0));
}
