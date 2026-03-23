#include "demo/passes/dof_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"

void DoFPass::init(const TierResourceView& res) {
    ub_.init(res.t4.dof_shader);
}

void DoFPass::execute(Renderer* r, FrameData& fd,
                      const TierResourceView& res,
                      const DemoTierConfig& cfg,
                      const SceneData& scene) {
    (void)scene;

    if (!res.t4.dof_shader || res.t4.dof_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    ub_.use();

    r->bindRenderTargetTexture(res.t4.hdr_scene_rt, 0);
    ub_.set(U::SceneTex, 0);
    r->bindTextureUnit(1, res.t4.hdr_depth_tex);
    ub_.set(U::DepthTex, 1);

    g4->bindImageTexture(res.t4.dof_tex, 0, false, true); // write-only

    ub_.set(U::ScreenSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub_.set(U::Near, kDemoNear);
    ub_.set(U::Far, kDemoFar);
    ub_.set(U::FocalDistance, cfg.dof_focal_distance);
    ub_.set(U::FocalRange, 5.0f);
    ub_.set(U::MaxBlur, 5.0f);
    ub_.set(U::DofStrength, cfg.dof_strength);

    int gx = (fd.viewport_w + 15) / 16;
    int gy = (fd.viewport_h + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    g4->imageMemoryBarrier();
}
