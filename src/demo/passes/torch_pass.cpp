#include "demo/passes/torch_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "platform/logger.h"

void TorchPass::init(const TierResourceView& res) {
    if (res.core.torch_shader)
        ub_.init(res.core.torch_shader);
}

// Torch positions: on top of column stumps C, D (standalone ruins, no arch above).
// Stump mesh: cylinder(16, 1.2, 0.30) — height 1.2, centered → top at +0.6 from base.
// Stump C: (-3.0, terrain, -0.5), Stump D: (3.0, terrain, -0.5)
static void getTorchPositions(float out[2][3]) {
    static const float stump_x[2] = { -3.0f, 3.0f };
    static const float stump_z = -0.5f;
    static const float stump_half_h = 0.6f;  // cylinder height 1.2 / 2
    static const float fire_offset = 0.15f;   // small gap above column top

    for (int i = 0; i < 2; i++) {
        float terrain_y = sampleTerrainHeight(stump_x[i], stump_z);
        out[i][0] = stump_x[i];
        out[i][1] = terrain_y + stump_half_h + fire_offset;
        out[i][2] = stump_z;
    }
}

void TorchPass::execute(Renderer* r, FrameData& fd,
                        const TierResourceView& res,
                        const DemoTierConfig& cfg,
                        const SceneData& scene) {
    (void)scene;
    if (!res.core.torch_shader || res.core.torch_mesh == MeshHandle()) return;

    ub_.use();
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::CamPos, fd.cam_pos);
    ub_.set(U::Time, fd.time);

    float tp[2][3];
    getTorchPositions(tp);
    res.core.torch_shader->set3f("u_torch_positions[0]", tp[0][0], tp[0][1], tp[0][2]);
    res.core.torch_shader->set3f("u_torch_positions[1]", tp[1][0], tp[1][1], tp[1][2]);

    r->setDepthTest(true);
    r->setDepthMask(false);
    r->setBlendingAdditive(true);
    r->setCullFace(false);

    r->drawMesh(res.core.torch_mesh);

    r->setDepthMask(true);
    r->setBlendingAdditive(false);
    r->setCullFace(true);
}
