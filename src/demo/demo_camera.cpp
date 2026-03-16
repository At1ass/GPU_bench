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
    // Fixed camera path through the "abandoned city" scene.
    // Scene is roughly 100x100 units centered at origin, terrain Y from -3 to +5.
    // Route: high start -> sweep right -> low over water -> through buildings -> climb -> finish
    keypoints_[0] = { Vec3(-40, 25, -40), Vec3(0, 0, 0) };      // high overview
    keypoints_[1] = { Vec3(-30, 18, -25), Vec3(10, 2, 5) };      // descending
    keypoints_[2] = { Vec3(-15, 10, -10), Vec3(15, 3, 10) };     // sweeping right
    keypoints_[3] = { Vec3(  5,  4,  -5), Vec3(20, 1, 15) };     // low approach
    keypoints_[4] = { Vec3( 15,  1,   5), Vec3(25, 0, 20) };     // low over water (Y~0)
    keypoints_[5] = { Vec3( 20,  3,  15), Vec3(10, 5, 30) };     // rising from water
    keypoints_[6] = { Vec3( 10,  8,  25), Vec3(-5, 4, 35) };     // through buildings
    keypoints_[7] = { Vec3( -5, 12,  30), Vec3(-20, 6, 20) };    // climbing
    keypoints_[8] = { Vec3(-20, 20,  25), Vec3(-30, 8, 0) };     // high orbit
    keypoints_[9] = { Vec3(-35, 25, 10),  Vec3(0, 5, 0) };       // final overview
}

Vec3 CameraPath::getPosition(float t) const {
    if (t <= 0.0f) return keypoints_[0].position;
    if (t >= 1.0f) return keypoints_[NUM_KEYPOINTS - 1].position;

    float seg_f = t * (NUM_KEYPOINTS - 1);
    int seg = static_cast<int>(seg_f);
    float local_t = seg_f - seg;

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
    if (t <= 0.0f) return keypoints_[0].target;
    if (t >= 1.0f) return keypoints_[NUM_KEYPOINTS - 1].target;

    float seg_f = t * (NUM_KEYPOINTS - 1);
    int seg = static_cast<int>(seg_f);
    float local_t = seg_f - seg;

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
