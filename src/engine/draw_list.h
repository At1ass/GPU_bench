#pragma once
#include "demo/scene_data.h"
#include <vector>
#include <cstdint>

// Sort-based draw call ordering for GeometryPass.
// Minimizes GPU state changes by grouping draws by shader → material → depth.
// Pattern from bgfx (64-bit sort keys) and Molecular Matters (command buckets).
//
// Sort key layout (32-bit, MSB first — most expensive state changes in high bits):
//   [shader_id : 8] [material_id : 8] [depth : 16]
//
// Cost hierarchy (DOOM 2016, Christer Ericson):
//   RT switch (~100x) > Program (~10x) > Texture (~3x) > Uniform (~1x) > VBO (~1x)
// RT switches are handled by pipeline; shader/material grouping is handled here.
//
// Usage:
//   DrawList dl;
//   for (auto& obj : objects) {
//       dl.push(obj, shader_id, material_id, depth_from_camera);
//   }
//   dl.sort();
//   for (size_t i = 0; i < dl.size(); i++) {
//       perObject(ub, ctx, *dl[i].obj);
//       ctx.renderer()->drawMesh(dl[i].obj->mesh);
//   }

struct DrawCmd {
    uint32_t sort_key;
    const SceneObject* obj;

    bool operator<(const DrawCmd& o) const { return sort_key < o.sort_key; }
};

class DrawList {
public:
    DrawList() = default;
    DrawList(const DrawList&) = delete;
    DrawList& operator=(const DrawList&) = delete;
    DrawList(DrawList&&) = default;
    DrawList& operator=(DrawList&&) = default;

    void clear() { cmds_.clear(); }

    // Add object with sort parameters. depth: distance from camera (0 = near).
    void push(const SceneObject& obj, uint8_t shader_id,
              uint8_t material_id, float depth);

    void sort();

    size_t size() const { return cmds_.size(); }
    const DrawCmd& operator[](size_t i) const { return cmds_[i]; }

private:
    std::vector<DrawCmd> cmds_;
};
