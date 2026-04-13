#include <doctest.h>
#include "engine/draw_list.h"
#include "engine/scene_data.h"

static SceneObject dummyObj() {
    SceneObject obj;
    obj.mesh = MeshHandle(1);
    obj.transform = Mat4();
    return obj;
}

TEST_SUITE("DrawList") {

TEST_CASE("DrawList: sort key encoding") {
    DrawList dl;
    SceneObject obj = dummyObj();
    dl.push(obj, 1, 2, 0.5f);

    dl.sort();
    CHECK(dl.size() == 1);
    // shader=1 (bits 24-31), material=2 (bits 16-23), depth≈0x7FFF (bits 0-15)
    uint32_t key = dl[0].sort_key;
    CHECK(((key >> 24) & 0xFF) == 1);
    CHECK(((key >> 16) & 0xFF) == 2);
    uint16_t depth = static_cast<uint16_t>(key & 0xFFFF);
    CHECK(depth >= 0x7FFE);  // ~0x7FFF, allow rounding
    CHECK(depth <= 0x8001);
}

TEST_CASE("DrawList: sort priority shader > material > depth") {
    DrawList dl;
    SceneObject obj = dummyObj();
    // shader=2, any material/depth should sort after shader=1
    dl.push(obj, 1, 0, 1.0f);  // low shader, max depth
    dl.push(obj, 2, 0, 0.0f);  // high shader, min depth

    dl.sort();
    CHECK(dl.size() == 2);
    CHECK(dl[0].sort_key < dl[1].sort_key);
    CHECK(((dl[0].sort_key >> 24) & 0xFF) == 1);
    CHECK(((dl[1].sort_key >> 24) & 0xFF) == 2);
}

TEST_CASE("DrawList: depth quantization boundaries") {
    DrawList dl;
    SceneObject obj = dummyObj();
    dl.push(obj, 0, 0, 0.0f);  // min depth
    dl.push(obj, 0, 0, 1.0f);  // max depth

    dl.sort();
    uint16_t d0 = static_cast<uint16_t>(dl[0].sort_key & 0xFFFF);
    uint16_t d1 = static_cast<uint16_t>(dl[1].sort_key & 0xFFFF);
    CHECK(d0 == 0x0000);
    CHECK(d1 == 0xFFFF);
}

TEST_CASE("DrawList: depth clamping") {
    DrawList dl;
    SceneObject obj = dummyObj();
    dl.push(obj, 0, 0, -0.5f);  // negative → clamp to 0
    dl.push(obj, 0, 0, 2.0f);   // > 1 → clamp to 1

    dl.sort();
    uint16_t d_neg = static_cast<uint16_t>(dl[0].sort_key & 0xFFFF);
    uint16_t d_over = static_cast<uint16_t>(dl[1].sort_key & 0xFFFF);
    CHECK(d_neg == 0x0000);
    CHECK(d_over == 0xFFFF);
}

} // TEST_SUITE("DrawList")
