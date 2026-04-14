#include "demo/passes/torch_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_debug.h"
#include "platform/logger.h"

void TorchPass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                     const DemoDebugOverrides& dbg) {
    (void)dbg;
    res_ = &res;
    cfg_ = &cfg;
    ShaderProgram* s = res.shader(ShaderBank::Torch);
    if (s)
        ub_.init(s);
}

bool TorchPass::isEnabled() const {
    return cfg_ && cfg_->point_light_count >= 3;
}

// Torch positions: on top of column stumps C, D (standalone ruins, no arch above).
// Stump mesh: cylinder(16, 1.2, 0.30) — height 1.2, centered -> top at +0.6 from base.
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

void TorchPass::execute(PassContext& ctx, FrameData& fd, const SceneData& scene) {
    Renderer* r = ctx.renderer();
    const TierResourceView& res = *res_;
    (void)scene;
    ShaderProgram* torch = res.shader(ShaderBank::Torch);
    if (!torch || res.core.torch_mesh == MeshHandle()) return;

    ub_.use();
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::CamPos, fd.cam_pos);
    ub_.set(U::Time, fd.time);

    float tp[2][3];
    getTorchPositions(tp);
    ub_.set(U::TorchPos0, tp[0][0], tp[0][1], tp[0][2]);
    ub_.set(U::TorchPos1, tp[1][0], tp[1][1], tp[1][2]);

    r->setDepthTest(true);
    r->setDepthMask(false);
    r->setBlendingAdditive(true);
    r->setCullFace(false);

    ctx.drawMesh(res.core.torch_mesh);

    r->setDepthMask(true);
    r->setBlendingAdditive(false);
    r->setCullFace(true);
}
